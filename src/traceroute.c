/**
 * @file traceroute.c
 * @brief TTL-stepped path discovery implementation.
 *
 * POSIX uses a raw ICMP/ICMPv6 socket and reads the "time exceeded" replies
 * from intermediate routers. Windows uses the IP Helper API, setting the TTL in
 * the request options, which reports the responding router directly.
 */

#include "traceroute.h"

#include "print.h"
#include "socket.h"
#include "timer.h"

#ifdef _WIN32
#include <iphlpapi.h>
#include <icmpapi.h>
#else
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>
#endif

#ifndef _WIN32

/** @brief One's-complement checksum (IPv4 ICMP needs it filled by hand). */
static uint16_t TraceChecksum(const uint8_t *data, size_t len) {
  uint32_t sum = 0;
  size_t i = 0;
  for (i = 0; (i + 1U) < len; i += 2U) {
    sum += (uint32_t)(((uint32_t)data[i] << 8) | (uint32_t)data[i + 1U]);
  }
  if (i < len) {
    sum += (uint32_t)((uint32_t)data[i] << 8);
  }
  while ((sum >> 16) != 0U) {
    sum = (sum & 0xFFFFU) + (sum >> 16);
  }
  return (uint16_t)(~sum);
}

/**
 * @brief Send one TTL-limited echo and wait for a router/destination reply.
 *
 * @param[out] from_ip  Receives the responder's address text.
 * @param[out] rtt      Receives the round-trip time in seconds.
 * @param[out] reached  Set to 1 when the responder is the destination.
 * @retval SUCCESS              A reply (time-exceeded or echo) was received.
 * @retval PINGDD_SOCKET_TIMEOUT No reply within the timeout.
 * @retval PINGDD_SOCKET_FAILURE Socket creation failed (likely no privilege).
 */
static int32_t TraceProbe(int family, const struct sockaddr *dest,
                          socklen_t destlen, int ttl, uint32_t timeout_ms,
                          uint16_t id, uint16_t seq, char *from_ip,
                          size_t from_ip_size, double *rtt, int *reached) {
  int is_v6 = (family == AF_INET6) ? 1 : 0;
  int proto = is_v6 ? IPPROTO_ICMPV6 : IPPROTO_ICMP;
  pingdd_socket_t fd = socket(family, SOCK_RAW, proto);
  uint8_t pkt[16] = {0};
  uint8_t rbuf[1500];
  pingdd_timer_t timer = (pingdd_timer_t){0};
  size_t j = 0;

  *reached = 0;
  if (fd == PINGDD_INVALID_SOCKET) {
    return PINGDD_SOCKET_FAILURE;
  }

  if (is_v6) {
    int hops = ttl;
    (void)setsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &hops, sizeof(hops));
  } else {
    int t = ttl;
    (void)setsockopt(fd, IPPROTO_IP, IP_TTL, &t, sizeof(t));
  }

  pkt[0] = is_v6 ? 128U : 8U;
  pkt[4] = (uint8_t)(id >> 8);
  pkt[5] = (uint8_t)(id & 0xFFU);
  pkt[6] = (uint8_t)(seq >> 8);
  pkt[7] = (uint8_t)(seq & 0xFFU);
  for (j = 8; j < sizeof(pkt); j++) {
    pkt[j] = (uint8_t)('A' + (int)(j - 8U));
  }
  if (!is_v6) {
    uint16_t c = TraceChecksum(pkt, sizeof(pkt));
    pkt[2] = (uint8_t)(c >> 8);
    pkt[3] = (uint8_t)(c & 0xFFU);
  }

  Timer_Start(&timer);
  if (sendto(fd, pkt, sizeof(pkt), 0, dest, destlen) < 0) {
    CLOSESOCKET(fd);
    return PINGDD_SOCKET_FAILURE;
  }

  for (;;) {
    double elapsed = Timer_Stop(&timer) * 1000.0;
    long remaining = (long)timeout_ms - (long)elapsed;
    struct sockaddr_storage from;
    socklen_t fromlen = (socklen_t)sizeof(from);
    struct pollfd pfd;
    int pr = 0;
    int n = 0;
    size_t off = 0;
    uint8_t type = 0;

    timer.hasValue = true;
    if (remaining <= 0) {
      CLOSESOCKET(fd);
      return PINGDD_SOCKET_TIMEOUT;
    }
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    pr = poll(&pfd, 1, (int)remaining);
    if (pr <= 0) {
      if ((pr < 0) && (errno == EINTR)) {
        continue;
      }
      CLOSESOCKET(fd);
      return PINGDD_SOCKET_TIMEOUT;
    }

    n = (int)recvfrom(fd, rbuf, sizeof(rbuf), 0, (struct sockaddr *)&from,
                      &fromlen);
    if (n < 0) {
      if ((errno == EINTR) || (errno == EAGAIN)) {
        continue;
      }
      CLOSESOCKET(fd);
      return PINGDD_SOCKET_TIMEOUT;
    }

    /* IPv4 raw sockets prepend the IP header; IPv6 raw sockets do not. */
    if (!is_v6) {
      off = (size_t)((rbuf[0] & 0x0FU) * 4U);
    }
    if ((size_t)n < (off + 8U)) {
      continue;
    }
    type = rbuf[off];

    {
      int matched = 0;
      int is_echo_reply = is_v6 ? (type == 129U) : (type == 0U);
      int is_time_exc = is_v6 ? (type == 3U) : (type == 11U);
      int is_unreach = is_v6 ? (type == 1U) : (type == 3U);

      if (is_echo_reply) {
        uint16_t rid = (uint16_t)((rbuf[off + 4U] << 8) | rbuf[off + 5U]);
        uint16_t rseq = (uint16_t)((rbuf[off + 6U] << 8) | rbuf[off + 7U]);
        if ((rseq == seq) && ((rid == id) || is_v6)) {
          *reached = 1;
          matched = 1;
        }
      } else if (is_time_exc || is_unreach) {
        /* Body holds the original packet; verify our seq where we can. */
        size_t inner = off + 8U;
        size_t inner_icmp = 0;
        if (is_v6) {
          inner_icmp = inner + 40U; /* fixed IPv6 header */
        } else if ((size_t)n > (inner)) {
          inner_icmp = inner + (size_t)((rbuf[inner] & 0x0FU) * 4U);
        }
        if (((inner_icmp + 8U) <= (size_t)n)) {
          uint16_t rseq =
              (uint16_t)((rbuf[inner_icmp + 6U] << 8) | rbuf[inner_icmp + 7U]);
          if (rseq == seq) {
            matched = 1;
            if (is_unreach) {
              *reached = 1;
            }
          }
        } else {
          matched = 1; /* can't verify; accept */
        }
      }

      if (!matched) {
        continue;
      }
    }

    *rtt = Timer_Stop(&timer);
    {
      const void *src =
          is_v6 ? (const void *)&((struct sockaddr_in6 *)&from)->sin6_addr
                : (const void *)&((struct sockaddr_in *)&from)->sin_addr;
      from_ip[0] = '\0';
      (void)inet_ntop(family, src, from_ip, (socklen_t)from_ip_size);
    }
    CLOSESOCKET(fd);
    return SUCCESS;
  }
}

#else /* _WIN32 */

/** @brief Send one TTL-limited echo via the IP Helper API. */
static int32_t TraceProbe(int family, const struct sockaddr *dest,
                          socklen_t destlen, int ttl, uint32_t timeout_ms,
                          uint16_t id, uint16_t seq, char *from_ip,
                          size_t from_ip_size, double *rtt, int *reached) {
  unsigned char payload[8] = {0};
  unsigned char reply[sizeof(ICMPV6_ECHO_REPLY) + 8 + 512] = {0};
  IP_OPTION_INFORMATION opts = {0};

  (void)id;
  (void)seq;
  (void)destlen;
  *reached = 0;
  opts.Ttl = (UCHAR)ttl;
  from_ip[0] = '\0';

  if (family == AF_INET6) {
    HANDLE h = Icmp6CreateFile();
    struct sockaddr_in6 src = (struct sockaddr_in6){0};
    struct sockaddr_in6 dst = *(const struct sockaddr_in6 *)dest;
    DWORD ret = 0;
    if (h == INVALID_HANDLE_VALUE) {
      return PINGDD_SOCKET_FAILURE;
    }
    src.sin6_family = AF_INET6;
    ret = Icmp6SendEcho2(h, NULL, NULL, NULL, &src, &dst, payload,
                         (WORD)sizeof(payload), &opts, reply, sizeof(reply),
                         timeout_ms);
    IcmpCloseHandle(h);
    if (ret == 0) {
      return PINGDD_SOCKET_TIMEOUT;
    }
    {
      ICMPV6_ECHO_REPLY *r = (ICMPV6_ECHO_REPLY *)reply;
      (void)inet_ntop(AF_INET6, r->Address.sin6_addr, from_ip,
                      (socklen_t)from_ip_size);
      *rtt = (double)r->RoundTripTime / 1000.0;
      if (r->Status == IP_SUCCESS) {
        *reached = 1;
      }
      return SUCCESS;
    }
  } else {
    HANDLE h = IcmpCreateFile();
    IPAddr d = ((const struct sockaddr_in *)dest)->sin_addr.S_un.S_addr;
    DWORD ret = 0;
    if (h == INVALID_HANDLE_VALUE) {
      return PINGDD_SOCKET_FAILURE;
    }
    ret = IcmpSendEcho2(h, NULL, NULL, NULL, d, payload, (WORD)sizeof(payload),
                        &opts, reply, sizeof(reply), timeout_ms);
    IcmpCloseHandle(h);
    if (ret == 0) {
      DWORD e = GetLastError();
      /* TTL expired in transit is the normal intermediate-hop signal. */
      if ((e == IP_TTL_EXPIRED_TRANSIT) || (e == IP_REQ_TIMED_OUT)) {
        ICMP_ECHO_REPLY *r = (ICMP_ECHO_REPLY *)reply;
        if (e == IP_REQ_TIMED_OUT) {
          return PINGDD_SOCKET_TIMEOUT;
        }
        {
          struct in_addr a;
          a.S_un.S_addr = r->Address;
          (void)inet_ntop(AF_INET, &a, from_ip, (socklen_t)from_ip_size);
        }
        *rtt = (double)r->RoundTripTime / 1000.0;
        return SUCCESS;
      }
      return PINGDD_SOCKET_TIMEOUT;
    }
    {
      ICMP_ECHO_REPLY *r = (ICMP_ECHO_REPLY *)reply;
      struct in_addr a;
      a.S_un.S_addr = r->Address;
      (void)inet_ntop(AF_INET, &a, from_ip, (socklen_t)from_ip_size);
      *rtt = (double)r->RoundTripTime / 1000.0;
      if (r->Status == IP_SUCCESS) {
        *reached = 1;
      }
      return SUCCESS;
    }
  }
}

#endif /* _WIN32 */

int32_t Traceroute_Run(const host_t *const target,
                       const trace_cfg_t *const cfg) {
  const struct sockaddr *dest = NULL;
  socklen_t destlen = 0;
  int family = 0;
  uint16_t id = 0;
  uint16_t seq = 0;
  int hop = 0;
  int reached_dest = 0;
  char line[360] = {0};

  if ((target == NULL) || (cfg == NULL) || (target->AddrCount == 0U)) {
    return PINGDD_INVALID_ARGS;
  }

  dest = (const struct sockaddr *)&target->Addrs[0];
  destlen = target->AddrLens[0];
  family = target->AddrFamilies[0];
#ifndef _WIN32
  id = (uint16_t)(getpid() & 0xFFFF);
#else
  id = 0x1234;
#endif

  (void)snprintf(line, sizeof(line), "traceroute to %s (%s), %d hops max\n",
                 target->Hostname, target->IPAddress, cfg->MaxHops);
  FormattedPrint(PRINT_YELLOW, line);
  ResetColor();

  for (hop = 1; (hop <= cfg->MaxHops) && (reached_dest == 0) &&
                (g_interrupted == 0);
       hop++) {
    char last_ip[64] = {0};
    int q = 0;

    (void)snprintf(line, sizeof(line), "%2d ", hop);
    FormattedPrint(PRINT_WHITE, line);

    for (q = 0; q < cfg->Queries; q++) {
      char from_ip[64] = {0};
      double rtt = 0.0;
      int reached = 0;
      int32_t r = TraceProbe(family, dest, destlen, hop, cfg->TimeoutMs, id,
                             ++seq, from_ip, sizeof(from_ip), &rtt, &reached);

      if (r == PINGDD_SOCKET_FAILURE) {
        FormattedPrint(PRINT_RED,
                       "\n  traceroute needs a raw socket "
                       "(run as root / grant CAP_NET_RAW)\n");
        ResetColor();
        return PINGDD_SOCKET_FAILURE;
      }
      if (r == PINGDD_SOCKET_TIMEOUT) {
        FormattedPrint(PRINT_WHITE, " *");
        continue;
      }

      /* Print the host once per hop, then just the timings. */
      if (strcmp(from_ip, last_ip) != 0) {
        (void)strncpy(last_ip, from_ip, sizeof(last_ip) - 1U);
        if (cfg->Resolve) {
          char name[256] = {0};
          struct sockaddr_storage ss;
          socklen_t sl = 0;
          memset(&ss, 0, sizeof(ss));
          if (family == AF_INET6) {
            struct sockaddr_in6 *s = (struct sockaddr_in6 *)&ss;
            s->sin6_family = AF_INET6;
            (void)inet_pton(AF_INET6, from_ip, &s->sin6_addr);
            sl = (socklen_t)sizeof(*s);
          } else {
            struct sockaddr_in *s = (struct sockaddr_in *)&ss;
            s->sin_family = AF_INET;
            (void)inet_pton(AF_INET, from_ip, &s->sin_addr);
            sl = (socklen_t)sizeof(*s);
          }
          if ((ReverseResolve((const struct sockaddr *)&ss, sl, name,
                              sizeof(name)) == (int32_t)SUCCESS) &&
              (name[0] != '\0')) {
            (void)snprintf(line, sizeof(line), " %s (%s)", name, from_ip);
          } else {
            (void)snprintf(line, sizeof(line), " %s", from_ip);
          }
        } else {
          (void)snprintf(line, sizeof(line), " %s", from_ip);
        }
        FormattedPrint(PRINT_GREEN, line);
      }

      (void)snprintf(line, sizeof(line), "  %.3f ms", rtt * 1000.0);
      FormattedPrint(PRINT_BLUE, line);

      if (reached != 0) {
        reached_dest = 1;
      }
    }
    (void)printf("\n");
    ResetColor();
  }

  return (reached_dest != 0) ? (int32_t)SUCCESS : PINGDD_SOCKET_FAILURE;
}
