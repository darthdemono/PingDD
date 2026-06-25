/**
 * @file socket.c
 * @brief Socket, resolution, and connection-probe implementation.
 *
 * Implements the functions declared in socket.h.
 */

#include "socket.h"

#include "timer.h"

#include <string.h>

#ifdef _WIN32
#include <iphlpapi.h>
#include <icmpapi.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

static int32_t InitializeWinsock(void);
static void CloseSocket(pingdd_socket_t socket_fd);
static int32_t MapConnectError(int const err);
static int WaitWritable(pingdd_socket_t fd, uint32_t timeout_ms);
static int WaitReadable(pingdd_socket_t fd, uint32_t timeout_ms);
static int32_t ProbeTcp(const struct sockaddr *addr, socklen_t addrlen,
                        int32_t family, uint32_t timeout_ms, double *rtt);
static int32_t ProbeUdp(const struct sockaddr *addr, socklen_t addrlen,
                        int32_t family, uint32_t timeout_ms, double *rtt);
static int32_t ProbeIcmp(const struct sockaddr *addr, socklen_t addrlen,
                         int32_t family, uint32_t timeout_ms, double *rtt);

pcc_t GetFriendlyTypeName(int32_t const type) {
  switch (type) {
  case SUCCESS:
    return "Success";
  case PINGDD_SOCKET_TIMEOUT:
    return "Connection timeout";
  case PINGDD_SOCKET_RESOLVE:
    return "Cannot resolve hostname";
  case PINGDD_SOCKET_FAILURE:
    return "Socket general failure";
  case PINGDD_SOCKET_CLOSED:
    return "Connection refused (closed port)";
  case PINGDD_SOCKET_UNREACH:
    return "Destination unreachable";
  case PINGDD_UDP_OPENFILTERED:
    return "No response (open|filtered)";
  case PINGDD_INTERRUPTED:
    return "Interrupted";
  case PINGDD_INVALID_ARGS:
    return "Invalid arguments";
  default:
    return "Unknown error";
  }
}

void SetPortAndType(uint16_t const port, int32_t const type,
                    host_t *const host) {
  if (host != NULL) {
    host->Port = port;
    host->Type = type;
  }
}

int32_t GetSocketType(int32_t const type) {
  switch (type) {
  case IPPROTO_UDP:
    return SOCK_DGRAM;
  case IPPROTO_TCP:
  default:
    return SOCK_STREAM;
  }
}

/** @brief Write a port number into a stored socket address (IPv4 or IPv6). */
static void SetSockAddrPort(int32_t const family,
                            struct sockaddr_storage *const addr,
                            uint16_t const port) {
  if (family == AF_INET6) {
    ((struct sockaddr_in6 *)addr)->sin6_port = htons(port);
  } else {
    ((struct sockaddr_in *)addr)->sin_port = htons(port);
  }
}

/** @brief Convert a stored address into its text form. */
static void SockAddrToString(const struct sockaddr *const addr,
                             int32_t const family, char *const out,
                             size_t const out_size) {
  const void *src = NULL;

  if ((out == NULL) || (out_size == 0U)) {
    return;
  }
  out[0] = '\0';

  if (family == AF_INET6) {
    src = &((const struct sockaddr_in6 *)addr)->sin6_addr;
  } else {
    src = &((const struct sockaddr_in *)addr)->sin_addr;
  }
  (void)inet_ntop(family, src, out, (socklen_t)out_size);
}

int32_t Resolve(pcc_t const destination, host_t *const host) {
  struct addrinfo hints = (struct addrinfo){0};
  struct addrinfo *result = NULL;
  struct addrinfo *iter = NULL;

  if ((destination == NULL) || (host == NULL)) {
    return PINGDD_INVALID_ARGS;
  }

  if (InitializeWinsock() != SUCCESS) {
    return PINGDD_SOCKET_FAILURE;
  }

  /* AF_UNSPEC returns both IPv4 and IPv6 candidates. */
  hints.ai_family = AF_UNSPEC;
  if (host->Type == IPPROTO_ICMP) {
    /* ICMP: resolve addresses only, no socket type / protocol constraint. */
    hints.ai_socktype = 0;
    hints.ai_protocol = 0;
  } else {
    hints.ai_socktype = GetSocketType(host->Type);
    hints.ai_protocol = host->Type;
  }

  if (getaddrinfo(destination, NULL, &hints, &result) != 0) {
    return PINGDD_SOCKET_RESOLVE;
  }

  host->AddrCount = 0U;

  for (iter = result; (iter != NULL) && (host->AddrCount < MAX_RESOLVED_ADDRS);
       iter = iter->ai_next) {
    if ((iter->ai_family != AF_INET) && (iter->ai_family != AF_INET6)) {
      continue;
    }
    if (iter->ai_addrlen > sizeof(host->Addrs[0])) {
      continue;
    }

    memcpy(&host->Addrs[host->AddrCount], iter->ai_addr, iter->ai_addrlen);
    host->AddrLens[host->AddrCount] = (socklen_t)iter->ai_addrlen;
    host->AddrFamilies[host->AddrCount] = iter->ai_family;
    host->AddrCount++;
  }

  freeaddrinfo(result);

  if (host->AddrCount == 0U) {
    return PINGDD_SOCKET_RESOLVE;
  }

  strncpy(host->Hostname, destination, sizeof(host->Hostname) - 1U);
  host->Hostname[sizeof(host->Hostname) - 1U] = '\0';

  SockAddrToString((const struct sockaddr *)&host->Addrs[0],
                   host->AddrFamilies[0], host->IPAddress,
                   sizeof(host->IPAddress));
  host->ReverseName[0] = '\0';

  return SUCCESS;
}

int32_t ReverseResolve(const struct sockaddr *const addr,
                       socklen_t const addrlen, char *const out,
                       size_t const out_size) {
  if ((addr == NULL) || (out == NULL) || (out_size == 0U)) {
    return PINGDD_INVALID_ARGS;
  }
  out[0] = '\0';

  if (getnameinfo(addr, addrlen, out, (socklen_t)out_size, NULL, 0,
                  NI_NAMEREQD) != 0) {
    out[0] = '\0';
    return PINGDD_SOCKET_RESOLVE;
  }

  return SUCCESS;
}

/** @brief Map a socket-level errno/SO_ERROR value to a PingDD result code. */
static int32_t MapConnectError(int const err) {
#ifdef _WIN32
  switch (err) {
  case WSAECONNREFUSED:
    return PINGDD_SOCKET_CLOSED;
  case WSAETIMEDOUT:
    return PINGDD_SOCKET_TIMEOUT;
  case WSAEHOSTUNREACH:
  case WSAENETUNREACH:
    return PINGDD_SOCKET_UNREACH;
  default:
    return PINGDD_SOCKET_FAILURE;
  }
#else
  switch (err) {
  case ECONNREFUSED:
    return PINGDD_SOCKET_CLOSED;
  case ETIMEDOUT:
    return PINGDD_SOCKET_TIMEOUT;
  case EHOSTUNREACH:
  case ENETUNREACH:
    return PINGDD_SOCKET_UNREACH;
  default:
    return PINGDD_SOCKET_FAILURE;
  }
#endif
}

/**
 * @brief Wait until @p fd is ready for the requested event or the timeout
 * elapses, restarting cleanly across EINTR and honouring g_interrupted.
 *
 * @return 1 ready, 0 timeout, -1 error, -2 interrupted by the user.
 */
static int WaitEvent(pingdd_socket_t fd, uint32_t timeout_ms, int for_write) {
  pingdd_timer_t timer = (pingdd_timer_t){0};
  Timer_Start(&timer); /* single monotonic origin for the whole wait */

  for (;;) {
    /* Timer_Stop reads the clock without disturbing the origin, so each
     * iteration sees the true cumulative elapsed time. */
    double elapsed_ms = Timer_Stop(&timer) * 1000.0;
    long remaining = (long)timeout_ms - (long)elapsed_ms;
    int ret = 0;

    if (g_interrupted != 0) {
      return -2;
    }
    if (remaining < 0) {
      remaining = 0;
    }
    timer.hasValue = true; /* Timer_Stop cleared it; keep origin for reuse */

#ifdef _WIN32
    {
      fd_set set;
      struct timeval tv;
      tv.tv_sec = (long)(remaining / 1000);
      tv.tv_usec = (long)((remaining % 1000) * 1000);
      FD_ZERO(&set);
      FD_SET(fd, &set);
      if (for_write) {
        ret = select(0, NULL, &set, NULL, &tv);
      } else {
        ret = select(0, &set, NULL, NULL, &tv);
      }
      if (ret == PINGDD_SOCKET_ERROR) {
        return -1; /* Winsock select() is not interrupted by signals. */
      }
    }
#else
    {
      struct pollfd pfd;
      pfd.fd = fd;
      pfd.events = (short)(for_write ? POLLOUT : POLLIN);
      pfd.revents = 0;
      ret = poll(&pfd, 1, (int)remaining);
      if (ret < 0) {
        if (errno == EINTR) {
          continue; /* recompute remaining and retry */
        }
        return -1;
      }
    }
#endif

    return (ret > 0) ? 1 : 0;
  }
}

static int WaitWritable(pingdd_socket_t fd, uint32_t timeout_ms) {
  return WaitEvent(fd, timeout_ms, 1);
}

static int WaitReadable(pingdd_socket_t fd, uint32_t timeout_ms) {
  return WaitEvent(fd, timeout_ms, 0);
}

/** @brief Set a socket to non-blocking mode. */
static int SetNonBlocking(pingdd_socket_t fd) {
#ifdef _WIN32
  u_long mode = 1U;
  return (ioctlsocket(fd, FIONBIO, &mode) == 0) ? 0 : -1;
#else
  int flags = fcntl(fd, F_GETFL, 0);
  return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) ? -1 : 0;
#endif
}

/** @brief Single-address TCP connect probe. */
static int32_t ProbeTcp(const struct sockaddr *addr, socklen_t addrlen,
                        int32_t family, uint32_t timeout_ms, double *rtt) {
  pingdd_socket_t fd = PINGDD_INVALID_SOCKET;
  pingdd_timer_t timer = (pingdd_timer_t){0};
  int wait_ret = 0;

  fd = socket(family, SOCK_STREAM, 0);
  if (fd == PINGDD_INVALID_SOCKET) {
    return PINGDD_SOCKET_FAILURE;
  }
  if (SetNonBlocking(fd) != 0) {
    CloseSocket(fd);
    return PINGDD_SOCKET_FAILURE;
  }

  Timer_Start(&timer);

  if (connect(fd, addr, addrlen) == PINGDD_SOCKET_ERROR) {
#ifdef _WIN32
    int err = WSAGetLastError();
    if (err != WSAEWOULDBLOCK)
#else
    int err = errno;
    if (err != EINPROGRESS)
#endif
    {
      *rtt = Timer_Stop(&timer);
      CloseSocket(fd);
      return MapConnectError(err);
    }
  }

  wait_ret = WaitWritable(fd, timeout_ms);
  if (wait_ret == -2) {
    CloseSocket(fd);
    return PINGDD_INTERRUPTED;
  }
  if (wait_ret == 0) {
    CloseSocket(fd);
    return PINGDD_SOCKET_TIMEOUT;
  }
  if (wait_ret < 0) {
    CloseSocket(fd);
    return PINGDD_SOCKET_FAILURE;
  }

  *rtt = Timer_Stop(&timer);

  {
    int error = 0;
    socklen_t len = (socklen_t)sizeof(error);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&error, &len) < 0) {
      CloseSocket(fd);
      return PINGDD_SOCKET_FAILURE;
    }
    if (error != 0) {
      CloseSocket(fd);
      return MapConnectError(error);
    }
  }

  CloseSocket(fd);
  return SUCCESS;
}

/**
 * @brief Single-address UDP probe.
 *
 * Sends a datagram on a connected UDP socket. A reply means the port is open;
 * an ICMP port-unreachable surfaces as a refused error; silence is ambiguous
 * (open|filtered).
 */
static int32_t ProbeUdp(const struct sockaddr *addr, socklen_t addrlen,
                        int32_t family, uint32_t timeout_ms, double *rtt) {
  pingdd_socket_t fd = PINGDD_INVALID_SOCKET;
  pingdd_timer_t timer = (pingdd_timer_t){0};
  const char payload[1] = {0};
  int wait_ret = 0;

  fd = socket(family, SOCK_DGRAM, 0);
  if (fd == PINGDD_INVALID_SOCKET) {
    return PINGDD_SOCKET_FAILURE;
  }
  if (SetNonBlocking(fd) != 0) {
    CloseSocket(fd);
    return PINGDD_SOCKET_FAILURE;
  }

  /* connect() on a UDP socket just pins the peer so we receive ICMP errors. */
  if (connect(fd, addr, addrlen) == PINGDD_SOCKET_ERROR) {
    CloseSocket(fd);
    return PINGDD_SOCKET_FAILURE;
  }

  Timer_Start(&timer);

  if (send(fd, payload, sizeof(payload), 0) == PINGDD_SOCKET_ERROR) {
#ifdef _WIN32
    int err = WSAGetLastError();
#else
    int err = errno;
#endif
    *rtt = Timer_Stop(&timer);
    CloseSocket(fd);
    return MapConnectError(err);
  }

  wait_ret = WaitReadable(fd, timeout_ms);
  if (wait_ret == -2) {
    CloseSocket(fd);
    return PINGDD_INTERRUPTED;
  }
  if (wait_ret == 0) {
    /* No reply and no ICMP error: open or filtered, indistinguishable. */
    CloseSocket(fd);
    return PINGDD_UDP_OPENFILTERED;
  }
  if (wait_ret < 0) {
    CloseSocket(fd);
    return PINGDD_SOCKET_FAILURE;
  }

  *rtt = Timer_Stop(&timer);

  {
    char buf[64];
    int n = (int)recv(fd, buf, sizeof(buf), 0);
    if (n < 0) {
#ifdef _WIN32
      int err = WSAGetLastError();
#else
      int err = errno;
#endif
      CloseSocket(fd);
      return MapConnectError(err);
    }
  }

  CloseSocket(fd);
  return SUCCESS;
}

bool IsPrivateAddress(const struct sockaddr *const addr) {
  if (addr == NULL) {
    return false;
  }

  if (addr->sa_family == AF_INET) {
    const struct sockaddr_in *a4 = (const struct sockaddr_in *)addr;
    uint32_t h = ntohl(a4->sin_addr.s_addr);
    if ((h >> 24) == 127U) {
      return true; /* 127.0.0.0/8 loopback */
    }
    if ((h >> 24) == 10U) {
      return true; /* 10.0.0.0/8 */
    }
    if ((h & 0xFFF00000U) == 0xAC100000U) {
      return true; /* 172.16.0.0/12 */
    }
    if ((h & 0xFFFF0000U) == 0xC0A80000U) {
      return true; /* 192.168.0.0/16 */
    }
    if ((h & 0xFFFF0000U) == 0xA9FE0000U) {
      return true; /* 169.254.0.0/16 link-local */
    }
    if ((h & 0xFFC00000U) == 0x64400000U) {
      return true; /* 100.64.0.0/10 CGNAT */
    }
    return false;
  }

  if (addr->sa_family == AF_INET6) {
    const struct sockaddr_in6 *a6 = (const struct sockaddr_in6 *)addr;
    const uint8_t *b = (const uint8_t *)&a6->sin6_addr;
    size_t k = 0;
    int is_loopback = 1;

    for (k = 0; k < 15U; k++) {
      if (b[k] != 0U) {
        is_loopback = 0;
        break;
      }
    }
    if ((is_loopback != 0) && (b[15] == 1U)) {
      return true; /* ::1 loopback */
    }
    if ((b[0] & 0xFEU) == 0xFCU) {
      return true; /* fc00::/7 unique local */
    }
    if ((b[0] == 0xFEU) && ((b[1] & 0xC0U) == 0x80U)) {
      return true; /* fe80::/10 link-local */
    }
    return false;
  }

  return false;
}

/** @brief Standard internet (one's complement) checksum over a byte buffer. */
static uint16_t IcmpChecksum(const uint8_t *data, size_t len) {
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

#ifdef _WIN32

/** @brief Windows ICMP echo via the IP Helper API (no raw socket needed). */
static int32_t ProbeIcmp(const struct sockaddr *addr, socklen_t addrlen,
                         int32_t family, uint32_t timeout_ms, double *rtt) {
  unsigned char payload[8] = {0};
  unsigned char reply[sizeof(ICMPV6_ECHO_REPLY) + 8 + 256] = {0};
  size_t k = 0;

  for (k = 0; k < sizeof(payload); k++) {
    payload[k] = (unsigned char)('A' + (int)k);
  }

  if (family == AF_INET6) {
    HANDLE h = Icmp6CreateFile();
    struct sockaddr_in6 src = (struct sockaddr_in6){0};
    struct sockaddr_in6 dst = *(const struct sockaddr_in6 *)addr;
    DWORD ret = 0;
    (void)addrlen;
    if (h == INVALID_HANDLE_VALUE) {
      return PINGDD_SOCKET_FAILURE;
    }
    src.sin6_family = AF_INET6;
    ret = Icmp6SendEcho2(h, NULL, NULL, NULL, &src, &dst, payload,
                         (WORD)sizeof(payload), NULL, reply, sizeof(reply),
                         timeout_ms);
    IcmpCloseHandle(h);
    if (ret == 0) {
      return (GetLastError() == IP_REQ_TIMED_OUT) ? PINGDD_SOCKET_TIMEOUT
                                                   : PINGDD_SOCKET_UNREACH;
    }
    {
      ICMPV6_ECHO_REPLY *r = (ICMPV6_ECHO_REPLY *)reply;
      if (r->Status == IP_SUCCESS) {
        *rtt = (double)r->RoundTripTime / 1000.0;
        return SUCCESS;
      }
      return PINGDD_SOCKET_UNREACH;
    }
  } else {
    HANDLE h = IcmpCreateFile();
    IPAddr dest = ((const struct sockaddr_in *)addr)->sin_addr.S_un.S_addr;
    DWORD ret = 0;
    (void)addrlen;
    if (h == INVALID_HANDLE_VALUE) {
      return PINGDD_SOCKET_FAILURE;
    }
    ret = IcmpSendEcho(h, dest, payload, (WORD)sizeof(payload), NULL, reply,
                       sizeof(reply), timeout_ms);
    IcmpCloseHandle(h);
    if (ret == 0) {
      return (GetLastError() == IP_REQ_TIMED_OUT) ? PINGDD_SOCKET_TIMEOUT
                                                   : PINGDD_SOCKET_UNREACH;
    }
    {
      ICMP_ECHO_REPLY *r = (ICMP_ECHO_REPLY *)reply;
      if (r->Status == IP_SUCCESS) {
        *rtt = (double)r->RoundTripTime / 1000.0;
        return SUCCESS;
      }
      return PINGDD_SOCKET_UNREACH;
    }
  }
}

#else /* POSIX */

/**
 * @brief POSIX ICMP/ICMPv6 echo probe.
 *
 * Prefers an unprivileged datagram ICMP socket (Linux ping_group_range,
 * macOS) and falls back to a raw socket (needs CAP_NET_RAW / root).
 */
static int32_t ProbeIcmp(const struct sockaddr *addr, socklen_t addrlen,
                         int32_t family, uint32_t timeout_ms, double *rtt) {
  static uint16_t icmp_seq = 0U;
  pingdd_socket_t fd = PINGDD_INVALID_SOCKET;
  pingdd_timer_t rtt_timer = (pingdd_timer_t){0};
  pingdd_timer_t budget = (pingdd_timer_t){0};
  uint8_t pkt[16] = {0};
  uint8_t rbuf[1500] = {0};
  int is_v6 = (family == AF_INET6) ? 1 : 0;
  int proto = is_v6 ? IPPROTO_ICMPV6 : IPPROTO_ICMP;
  int is_raw = 0;
  uint16_t id = (uint16_t)(getpid() & 0xFFFF);
  uint16_t seq = ++icmp_seq;
  uint8_t expect = is_v6 ? 129U : 0U;
  size_t j = 0;

  fd = socket(family, SOCK_DGRAM, proto);
  if (fd == PINGDD_INVALID_SOCKET) {
    fd = socket(family, SOCK_RAW, proto);
    is_raw = 1;
  }
  if (fd == PINGDD_INVALID_SOCKET) {
    /* Typically EPERM: ICMP needs privilege or ping_group_range tuning. */
    return PINGDD_SOCKET_FAILURE;
  }
  if (SetNonBlocking(fd) != 0) {
    CloseSocket(fd);
    return PINGDD_SOCKET_FAILURE;
  }

  pkt[0] = is_v6 ? 128U : 8U; /* echo request type */
  pkt[1] = 0U;                /* code */
  pkt[4] = (uint8_t)(id >> 8);
  pkt[5] = (uint8_t)(id & 0xFFU);
  pkt[6] = (uint8_t)(seq >> 8);
  pkt[7] = (uint8_t)(seq & 0xFFU);
  for (j = 8; j < sizeof(pkt); j++) {
    pkt[j] = (uint8_t)('A' + (int)(j - 8U));
  }
  if (!is_v6) {
    /* ICMPv6 checksum is computed by the kernel; ICMPv4 is ours to fill. */
    uint16_t cks = IcmpChecksum(pkt, sizeof(pkt));
    pkt[2] = (uint8_t)(cks >> 8);
    pkt[3] = (uint8_t)(cks & 0xFFU);
  }

  Timer_Start(&rtt_timer);

  if (sendto(fd, pkt, sizeof(pkt), 0, addr, addrlen) == PINGDD_SOCKET_ERROR) {
    int err = errno;
    CloseSocket(fd);
    return MapConnectError(err);
  }

  Timer_Start(&budget);
  for (;;) {
    double elapsed_ms = Timer_Stop(&budget) * 1000.0;
    long remaining = (long)timeout_ms - (long)elapsed_ms;
    int w = 0;
    int n = 0;
    size_t off = 0;

    budget.hasValue = true;
    if (remaining <= 0) {
      CloseSocket(fd);
      return PINGDD_SOCKET_TIMEOUT;
    }

    w = WaitReadable(fd, (uint32_t)remaining);
    if (w == -2) {
      CloseSocket(fd);
      return PINGDD_INTERRUPTED;
    }
    if (w == 0) {
      CloseSocket(fd);
      return PINGDD_SOCKET_TIMEOUT;
    }
    if (w < 0) {
      CloseSocket(fd);
      return PINGDD_SOCKET_FAILURE;
    }

    n = (int)recvfrom(fd, rbuf, sizeof(rbuf), 0, NULL, NULL);
    if (n < 0) {
      if ((errno == EINTR) || (errno == EAGAIN)) {
        continue;
      }
      CloseSocket(fd);
      return MapConnectError(errno);
    }

    /* A raw IPv4 socket prepends the IP header; strip it. */
    if ((is_raw != 0) && (is_v6 == 0)) {
      off = (size_t)((rbuf[0] & 0x0FU) * 4U);
    }
    if ((size_t)n < (off + 8U)) {
      continue;
    }
    if (rbuf[off] != expect) {
      continue; /* not an echo reply */
    }
    {
      uint16_t rseq = (uint16_t)((rbuf[off + 6U] << 8) | rbuf[off + 7U]);
      uint16_t rid = (uint16_t)((rbuf[off + 4U] << 8) | rbuf[off + 5U]);
      if (rseq != seq) {
        continue;
      }
      /* Datagram sockets rewrite the id field; only match it on raw. */
      if ((is_raw != 0) && (rid != id)) {
        continue;
      }
    }

    *rtt = Timer_Stop(&rtt_timer);
    CloseSocket(fd);
    return SUCCESS;
  }
}

#endif /* _WIN32 */

int32_t Connect(host_t const *const host, uint32_t const timeout_ms,
                double *const rtt, char *const out_ip,
                size_t const out_ip_size) {
  size_t i = 0;
  int32_t last_result = PINGDD_SOCKET_FAILURE;

  if ((host == NULL) || (rtt == NULL) || (host->AddrCount == 0U)) {
    return PINGDD_INVALID_ARGS;
  }

  if (InitializeWinsock() != SUCCESS) {
    return PINGDD_SOCKET_FAILURE;
  }

  for (i = 0; i < host->AddrCount; i++) {
    struct sockaddr_storage addr = host->Addrs[i];
    int32_t family = host->AddrFamilies[i];
    socklen_t addrlen = host->AddrLens[i];
    int32_t result = 0;

    *rtt = 0.0;

    if (host->Type == IPPROTO_ICMP) {
      /* ICMP has no port; the address is used as resolved. */
      result = ProbeIcmp((const struct sockaddr *)&addr, addrlen, family,
                         timeout_ms, rtt);
    } else if (host->Type == IPPROTO_UDP) {
      SetSockAddrPort(family, &addr, host->Port);
      result = ProbeUdp((const struct sockaddr *)&addr, addrlen, family,
                        timeout_ms, rtt);
    } else {
      SetSockAddrPort(family, &addr, host->Port);
      result = ProbeTcp((const struct sockaddr *)&addr, addrlen, family,
                        timeout_ms, rtt);
    }

    if ((result == SUCCESS) || (result == PINGDD_INTERRUPTED)) {
      if (out_ip != NULL) {
        SockAddrToString((const struct sockaddr *)&addr, family, out_ip,
                         out_ip_size);
      }
      return result;
    }

    last_result = result;
    /* Otherwise fall through and try the next resolved address. */
  }

  /* All addresses failed: report the last address tried and its error. */
  if ((out_ip != NULL) && (host->AddrCount > 0U)) {
    size_t last = host->AddrCount - 1U;
    SockAddrToString((const struct sockaddr *)&host->Addrs[last],
                     host->AddrFamilies[last], out_ip, out_ip_size);
  }
  return last_result;
}

static int32_t InitializeWinsock(void) {
#ifdef _WIN32
  static bool initialized = false;

  if (!initialized) {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
      return PINGDD_SOCKET_FAILURE;
    }
    initialized = true;
  }
#endif

  return SUCCESS;
}

static void CloseSocket(pingdd_socket_t socket_fd) {
#ifdef _WIN32
  closesocket(socket_fd);
#else
  close(socket_fd);
#endif
}
