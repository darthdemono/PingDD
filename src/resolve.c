/**
 * @file resolve.c
 * @brief Reverse-DNS + Team Cymru ASN annotation implementation.
 *
 * Contains a deliberately tiny DNS client that issues a single TXT query and
 * extracts the first answer string. It is just enough to talk to the Team Cymru
 * IP-to-ASN service and avoids pulling in a resolver library, keeping the
 * Windows build free of extra link dependencies.
 */

#include "resolve.h"

#include "socket.h"

#ifndef _WIN32
#include <unistd.h>
#endif

/** @brief DNS TXT record type. */
#define DNS_TYPE_TXT 16
/** @brief DNS IN class. */
#define DNS_CLASS_IN 1
/** @brief Per-query timeout for ASN lookups, in milliseconds. */
#define DNS_TIMEOUT_MS 1500

/** @brief Close a raw DNS socket portably. */
static void DnsClose(pingdd_socket_t fd) {
#ifdef _WIN32
  closesocket(fd);
#else
  close(fd);
#endif
}

/**
 * @brief Choose a recursive resolver to query.
 *
 * On POSIX the first `nameserver` entry in /etc/resolv.conf is used; otherwise
 * (and always on Windows) it falls back to a well-known public resolver. The
 * lookups are non-sensitive (public IP-to-ASN data), so the fallback is safe.
 */
static void PickResolver(struct sockaddr_in *const out) {
  memset(out, 0, sizeof(*out));
  out->sin_family = AF_INET;
  out->sin_port = htons(53);
  (void)inet_pton(AF_INET, "1.1.1.1", &out->sin_addr);

#ifndef _WIN32
  {
    FILE *f = fopen("/etc/resolv.conf", "r");
    char line[256];
    if (f == NULL) {
      return;
    }
    while (fgets(line, (int)sizeof(line), f) != NULL) {
      char ip[64] = {0};
      if (sscanf(line, "nameserver %63s", ip) == 1) {
        struct in_addr a;
        if (inet_pton(AF_INET, ip, &a) == 1) {
          out->sin_addr = a;
          break;
        }
      }
    }
    (void)fclose(f);
  }
#endif
}

/**
 * @brief Encode a dotted name into DNS label format.
 * @return Encoded length, or 0 on overflow.
 */
static size_t EncodeName(pcc_t const name, uint8_t *const out,
                         size_t const out_size) {
  size_t pos = 0;
  pcc_t seg = name;

  for (;;) {
    pcc_t dot = strchr(seg, '.');
    size_t len = (dot != NULL) ? (size_t)(dot - seg) : strlen(seg);
    if ((len == 0U) || (len > 63U) || ((pos + len + 1U) >= out_size)) {
      if (len == 0U) {
        break; /* trailing/empty label ends the name */
      }
      return 0;
    }
    out[pos++] = (uint8_t)len;
    memcpy(&out[pos], seg, len);
    pos += len;
    if (dot == NULL) {
      break;
    }
    seg = dot + 1;
  }
  if ((pos + 1U) >= out_size) {
    return 0;
  }
  out[pos++] = 0; /* root label */
  return pos;
}

/** @brief Advance @p off past a (possibly compressed) DNS name. */
static int SkipName(const uint8_t *const buf, size_t const len, size_t *off) {
  while (*off < len) {
    uint8_t b = buf[*off];
    if ((b & 0xC0U) == 0xC0U) {
      *off += 2U; /* compression pointer terminates the name */
      return 0;
    }
    if (b == 0U) {
      *off += 1U;
      return 0;
    }
    *off += (size_t)b + 1U;
  }
  return -1;
}

/**
 * @brief Run one DNS TXT query and copy the first answer string.
 * @retval true  An answer string was extracted into @p out.
 * @retval false Network error, timeout, or no TXT answer.
 */
static bool DnsTxtQuery(pcc_t const qname, char *const out,
                        size_t const out_size) {
  uint8_t query[512];
  uint8_t resp[1024];
  size_t qlen = 0;
  size_t qn_len = 0;
  pingdd_socket_t fd = PINGDD_INVALID_SOCKET;
  struct sockaddr_in server;
  int n = 0;
  size_t off = 0;
  uint16_t ancount = 0;
  uint16_t qdcount = 0;
  int i = 0;

  out[0] = '\0';

  /* Header: id=0x1234, flags=RD, qdcount=1. */
  memset(query, 0, sizeof(query));
  query[0] = 0x12;
  query[1] = 0x34;
  query[2] = 0x01; /* RD */
  query[5] = 0x01; /* qdcount = 1 */
  qlen = 12;

  qn_len = EncodeName(qname, &query[qlen], sizeof(query) - qlen);
  if (qn_len == 0U) {
    return false;
  }
  qlen += qn_len;
  if ((qlen + 4U) > sizeof(query)) {
    return false;
  }
  query[qlen++] = 0;
  query[qlen++] = (uint8_t)DNS_TYPE_TXT;
  query[qlen++] = 0;
  query[qlen++] = (uint8_t)DNS_CLASS_IN;

  PickResolver(&server);

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd == PINGDD_INVALID_SOCKET) {
    return false;
  }

  {
#ifdef _WIN32
    DWORD tv = DNS_TIMEOUT_MS;
#else
    struct timeval tv;
    tv.tv_sec = DNS_TIMEOUT_MS / 1000;
    tv.tv_usec = (DNS_TIMEOUT_MS % 1000) * 1000;
#endif
    (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv,
                     sizeof(tv));
  }

  if (sendto(fd, (const char *)query, (int)qlen, 0,
             (struct sockaddr *)&server, (socklen_t)sizeof(server)) < 0) {
    DnsClose(fd);
    return false;
  }

  n = (int)recvfrom(fd, (char *)resp, (int)sizeof(resp), 0, NULL, NULL);
  DnsClose(fd);
  if (n < 12) {
    return false;
  }

  qdcount = (uint16_t)((resp[4] << 8) | resp[5]);
  ancount = (uint16_t)((resp[6] << 8) | resp[7]);
  if (ancount == 0U) {
    return false;
  }

  off = 12;
  /* Skip the echoed question section. */
  for (i = 0; i < (int)qdcount; i++) {
    if (SkipName(resp, (size_t)n, &off) != 0) {
      return false;
    }
    off += 4U; /* qtype + qclass */
  }

  for (i = 0; i < (int)ancount; i++) {
    uint16_t type = 0;
    uint16_t rdlength = 0;
    if (SkipName(resp, (size_t)n, &off) != 0) {
      return false;
    }
    if ((off + 10U) > (size_t)n) {
      return false;
    }
    type = (uint16_t)((resp[off] << 8) | resp[off + 1U]);
    rdlength = (uint16_t)((resp[off + 8U] << 8) | resp[off + 9U]);
    off += 10U;
    if ((off + rdlength) > (size_t)n) {
      return false;
    }
    if (type == DNS_TYPE_TXT) {
      uint8_t slen = resp[off];
      if ((size_t)slen + 1U <= rdlength) {
        size_t copy = ((size_t)slen < (out_size - 1U)) ? (size_t)slen
                                                        : (out_size - 1U);
        memcpy(out, &resp[off + 1U], copy);
        out[copy] = '\0';
        return true;
      }
    }
    off += rdlength;
  }
  return false;
}

/** @brief Copy the field at index @p idx of a `|`-delimited Cymru record. */
static void CymruField(pcc_t const record, int const idx, char *const out,
                       size_t const out_size) {
  int cur = 0;
  pcc_t p = record;
  pcc_t start = record;

  out[0] = '\0';
  for (;;) {
    if ((*p == '|') || (*p == '\0')) {
      if (cur == idx) {
        /* Trim surrounding spaces around the field. */
        pcc_t s = start;
        pcc_t e = p;
        size_t len = 0;
        while ((s < e) && (*s == ' ')) {
          s++;
        }
        while ((e > s) && (e[-1] == ' ')) {
          e--;
        }
        len = (size_t)(e - s);
        if (len > (out_size - 1U)) {
          len = out_size - 1U;
        }
        memcpy(out, s, len);
        out[len] = '\0';
        return;
      }
      if (*p == '\0') {
        return;
      }
      cur++;
      start = p + 1;
    }
    p++;
  }
}

/** @brief Build the Cymru origin query name for an address. */
static bool BuildOriginQuery(const struct sockaddr *const addr,
                             char *const out, size_t const out_size) {
  if (addr->sa_family == AF_INET) {
    const uint8_t *b =
        (const uint8_t *)&((const struct sockaddr_in *)addr)->sin_addr;
    (void)snprintf(out, out_size, "%u.%u.%u.%u.origin.asn.cymru.com",
                   (unsigned)b[3], (unsigned)b[2], (unsigned)b[1],
                   (unsigned)b[0]);
    return true;
  }
  if (addr->sa_family == AF_INET6) {
    const uint8_t *b =
        (const uint8_t *)&((const struct sockaddr_in6 *)addr)->sin6_addr;
    char nibbles[80];
    size_t pos = 0;
    int i = 0;
    for (i = 15; i >= 0; i--) {
      int hi = (b[i] >> 4) & 0xF;
      int lo = b[i] & 0xF;
      pos += (size_t)snprintf(&nibbles[pos], sizeof(nibbles) - pos,
                              "%x.%x.", lo, hi);
    }
    (void)snprintf(out, out_size, "%sorigin6.asn.cymru.com", nibbles);
    return true;
  }
  return false;
}

void Resolve_Annotate(host_t *const host) {
  char origin_q[128];
  char record[256];
  char asn_q[64];
  char asnum[16] = {0};

  if ((host == NULL) || (host->AddrCount == 0U)) {
    return;
  }

  host->ReverseName[0] = '\0';
  host->Asn[0] = '\0';
  host->AsnOrg[0] = '\0';

  /* Reverse DNS of the primary address. */
  (void)ReverseResolve((const struct sockaddr *)&host->Addrs[0],
                       host->AddrLens[0], host->ReverseName,
                       sizeof(host->ReverseName));

  /* Origin ASN: <reversed-ip>.origin[6].asn.cymru.com -> "ASN | prefix | ...". */
  if (!BuildOriginQuery((const struct sockaddr *)&host->Addrs[0], origin_q,
                        sizeof(origin_q))) {
    return;
  }
  if (!DnsTxtQuery(origin_q, record, sizeof(record))) {
    return;
  }
  CymruField(record, 0, asnum, sizeof(asnum));
  if (asnum[0] == '\0') {
    return;
  }
  /* A prefix may map to several ASNs ("13335 38803"); keep the first. */
  {
    char *sp = strchr(asnum, ' ');
    if (sp != NULL) {
      *sp = '\0';
    }
  }
  (void)snprintf(host->Asn, sizeof(host->Asn), "AS%s", asnum);

  /* AS holder: AS<num>.asn.cymru.com -> "ASN | CC | registry | date | org". */
  (void)snprintf(asn_q, sizeof(asn_q), "AS%s.asn.cymru.com", asnum);
  if (DnsTxtQuery(asn_q, record, sizeof(record))) {
    CymruField(record, 4, host->AsnOrg, sizeof(host->AsnOrg));
  }
}
