/**
 * @file http.c
 * @brief HTTP(S) service probe implementation.
 *
 * A small, self-contained HTTP/1.1 client: resolve, TCP connect with timeout,
 * optional TLS via the vendored mbedTLS, send one request, and parse the
 * response status line. TLS is used purely to reach the application layer; the
 * certificate chain is not verified (this is a reachability/diagnostic probe,
 * not a secure client), but the negotiated version and cipher are reported.
 */

#include "http.h"

#include "socket.h"
#include "timer.h"

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/error.h"
#include "mbedtls/ssl.h"

#ifndef _WIN32
#include <poll.h>
#include <unistd.h>
#endif

/** @brief Active connection: a socket, optionally wrapped in TLS. */
typedef struct {
  pingdd_socket_t Fd;
  bool Tls;
  mbedtls_ssl_context Ssl;
  mbedtls_ssl_config Conf;
  mbedtls_ctr_drbg_context Drbg;
  mbedtls_entropy_context Entropy;
} conn_t;

/** @brief Parsed pieces of a URL. */
typedef struct {
  bool Https;
  char Host[256];
  uint16_t Port;
  char Path[1024];
} url_t;

/** @brief Close a socket portably. */
static void HttpCloseSocket(pingdd_socket_t fd) {
#ifdef _WIN32
  closesocket(fd);
#else
  close(fd);
#endif
}

/** @brief Parse an http(s) URL into its components. */
static bool ParseUrl(pcc_t const url, url_t *const out) {
  pcc_t p = url;
  pcc_t host_start = NULL;
  pcc_t path_start = NULL;
  size_t host_len = 0;

  memset(out, 0, sizeof(*out));

  if (strncmp(p, "https://", 8) == 0) {
    out->Https = true;
    out->Port = 443;
    p += 8;
  } else if (strncmp(p, "http://", 7) == 0) {
    out->Https = false;
    out->Port = 80;
    p += 7;
  } else {
    return false;
  }

  host_start = p;
  /* Host runs until '/', '?' or end. */
  while ((*p != '\0') && (*p != '/') && (*p != '?')) {
    p++;
  }
  path_start = p;
  host_len = (size_t)(p - host_start);
  if ((host_len == 0U) || (host_len >= sizeof(out->Host))) {
    return false;
  }
  memcpy(out->Host, host_start, host_len);
  out->Host[host_len] = '\0';

  /* Split an optional :port (but keep bracketed IPv6 literals intact). */
  if (out->Host[0] == '[') {
    char *rb = strchr(out->Host, ']');
    if (rb == NULL) {
      return false;
    }
    if (rb[1] == ':') {
      long pv = strtol(rb + 2, NULL, 10);
      if ((pv < 1) || (pv > 65535)) {
        return false;
      }
      out->Port = (uint16_t)pv;
    }
    *rb = '\0';
    memmove(out->Host, out->Host + 1, strlen(out->Host + 1) + 1U);
  } else {
    char *colon = strrchr(out->Host, ':');
    if (colon != NULL) {
      long pv = strtol(colon + 1, NULL, 10);
      if ((pv < 1) || (pv > 65535)) {
        return false;
      }
      out->Port = (uint16_t)pv;
      *colon = '\0';
    }
  }

  if (*path_start == '\0') {
    (void)snprintf(out->Path, sizeof(out->Path), "/");
  } else {
    (void)snprintf(out->Path, sizeof(out->Path), "%s", path_start);
  }
  return true;
}

/** @brief Resolve + TCP-connect with a timeout; returns the socket or -1. */
static pingdd_socket_t TcpConnect(const url_t *const u, uint32_t timeout_ms,
                                  char *const ip_out, size_t ip_out_size,
                                  char *const err, size_t err_size) {
  struct addrinfo hints;
  struct addrinfo *res = NULL;
  struct addrinfo *it = NULL;
  char portstr[8];
  pingdd_socket_t fd = PINGDD_INVALID_SOCKET;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  (void)snprintf(portstr, sizeof(portstr), "%u", (unsigned)u->Port);

  if (getaddrinfo(u->Host, portstr, &hints, &res) != 0) {
    (void)snprintf(err, err_size, "cannot resolve %s", u->Host);
    return PINGDD_INVALID_SOCKET;
  }

  for (it = res; it != NULL; it = it->ai_next) {
    int connected = 0;
    fd = socket(it->ai_family, SOCK_STREAM, 0);
    if (fd == PINGDD_INVALID_SOCKET) {
      continue;
    }

#ifdef _WIN32
    {
      u_long nb = 1;
      (void)ioctlsocket(fd, FIONBIO, &nb);
    }
#else
    {
      int fl = fcntl(fd, F_GETFL, 0);
      (void)fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    }
#endif

    if (connect(fd, it->ai_addr, (socklen_t)it->ai_addrlen) == 0) {
      connected = 1;
    } else {
#ifdef _WIN32
      int e = WSAGetLastError();
      int inprog = (e == WSAEWOULDBLOCK);
#else
      int inprog = (errno == EINPROGRESS);
#endif
      if (inprog) {
#ifdef _WIN32
        fd_set wset;
        struct timeval tv;
        tv.tv_sec = (long)(timeout_ms / 1000U);
        tv.tv_usec = (long)((timeout_ms % 1000U) * 1000U);
        FD_ZERO(&wset);
        FD_SET(fd, &wset);
        if (select(0, NULL, &wset, NULL, &tv) > 0) {
          int so = 0;
          int sl = (int)sizeof(so);
          if ((getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&so, &sl) == 0) &&
              (so == 0)) {
            connected = 1;
          }
        }
#else
        struct pollfd pfd;
        pfd.fd = fd;
        pfd.events = POLLOUT;
        pfd.revents = 0;
        if (poll(&pfd, 1, (int)timeout_ms) > 0) {
          int so = 0;
          socklen_t sl = (socklen_t)sizeof(so);
          if ((getsockopt(fd, SOL_SOCKET, SO_ERROR, &so, &sl) == 0) &&
              (so == 0)) {
            connected = 1;
          }
        }
#endif
      }
    }

    if (connected) {
      /* Back to blocking with per-call timeouts for the request/response. */
#ifdef _WIN32
      u_long nb = 0;
      DWORD tv = timeout_ms;
      (void)ioctlsocket(fd, FIONBIO, &nb);
      (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv,
                       sizeof(tv));
      (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv,
                       sizeof(tv));
#else
      int fl = fcntl(fd, F_GETFL, 0);
      struct timeval tv;
      tv.tv_sec = (long)(timeout_ms / 1000U);
      tv.tv_usec = (long)((timeout_ms % 1000U) * 1000U);
      (void)fcntl(fd, F_SETFL, fl & ~O_NONBLOCK);
      (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
      (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
      if (ip_out != NULL) {
        const void *src =
            (it->ai_family == AF_INET6)
                ? (const void *)&((struct sockaddr_in6 *)it->ai_addr)->sin6_addr
                : (const void *)&((struct sockaddr_in *)it->ai_addr)->sin_addr;
        (void)inet_ntop(it->ai_family, src, ip_out, (socklen_t)ip_out_size);
      }
      freeaddrinfo(res);
      return fd;
    }

    HttpCloseSocket(fd);
    fd = PINGDD_INVALID_SOCKET;
  }

  freeaddrinfo(res);
  (void)snprintf(err, err_size, "connection failed");
  return PINGDD_INVALID_SOCKET;
}

/** @brief mbedTLS BIO send callback over our socket. */
static int BioSend(void *ctx, const unsigned char *buf, size_t len) {
  pingdd_socket_t fd = *(pingdd_socket_t *)ctx;
  int n = (int)send(fd, (const char *)buf, (int)len, 0);
  if (n < 0) {
#ifdef _WIN32
    int e = WSAGetLastError();
    if ((e == WSAEWOULDBLOCK) || (e == WSAETIMEDOUT))
#else
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
#endif
    {
      return MBEDTLS_ERR_SSL_WANT_WRITE;
    }
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  return n;
}

/** @brief mbedTLS BIO receive callback over our socket. */
static int BioRecv(void *ctx, unsigned char *buf, size_t len) {
  pingdd_socket_t fd = *(pingdd_socket_t *)ctx;
  int n = (int)recv(fd, (char *)buf, (int)len, 0);
  if (n < 0) {
#ifdef _WIN32
    int e = WSAGetLastError();
    if ((e == WSAEWOULDBLOCK) || (e == WSAETIMEDOUT))
#else
    if ((errno == EAGAIN) || (errno == EWOULDBLOCK))
#endif
    {
      return MBEDTLS_ERR_SSL_WANT_READ;
    }
    return MBEDTLS_ERR_SSL_INTERNAL_ERROR;
  }
  return n;
}

/** @brief Bring up TLS on an already-connected socket. */
static bool TlsHandshake(conn_t *const c, pcc_t const host, char *const err,
                         size_t err_size) {
  static const char *pers = "pingdd-http";
  int ret = 0;

  mbedtls_ssl_init(&c->Ssl);
  mbedtls_ssl_config_init(&c->Conf);
  mbedtls_ctr_drbg_init(&c->Drbg);
  mbedtls_entropy_init(&c->Entropy);

  if (mbedtls_ctr_drbg_seed(&c->Drbg, mbedtls_entropy_func, &c->Entropy,
                            (const unsigned char *)pers, strlen(pers)) != 0) {
    (void)snprintf(err, err_size, "TLS RNG seed failed");
    return false;
  }
  if (mbedtls_ssl_config_defaults(&c->Conf, MBEDTLS_SSL_IS_CLIENT,
                                  MBEDTLS_SSL_TRANSPORT_STREAM,
                                  MBEDTLS_SSL_PRESET_DEFAULT) != 0) {
    (void)snprintf(err, err_size, "TLS config failed");
    return false;
  }
  /* Diagnostic probe: do not verify the chain (no bundled CA store). */
  mbedtls_ssl_conf_authmode(&c->Conf, MBEDTLS_SSL_VERIFY_NONE);
  mbedtls_ssl_conf_rng(&c->Conf, mbedtls_ctr_drbg_random, &c->Drbg);

  if (mbedtls_ssl_setup(&c->Ssl, &c->Conf) != 0) {
    (void)snprintf(err, err_size, "TLS setup failed");
    return false;
  }
  (void)mbedtls_ssl_set_hostname(&c->Ssl, host); /* SNI */
  mbedtls_ssl_set_bio(&c->Ssl, &c->Fd, BioSend, BioRecv, NULL);

  while ((ret = mbedtls_ssl_handshake(&c->Ssl)) != 0) {
    if ((ret != MBEDTLS_ERR_SSL_WANT_READ) &&
        (ret != MBEDTLS_ERR_SSL_WANT_WRITE)) {
      char eb[96] = {0};
      mbedtls_strerror(ret, eb, sizeof(eb));
      (void)snprintf(err, err_size, "TLS handshake failed: %s", eb);
      return false;
    }
  }
  return true;
}

/** @brief Send all bytes over the connection (TLS or plain). */
static bool ConnSendAll(conn_t *const c, pcc_t const data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    int n = 0;
    if (c->Tls) {
      n = mbedtls_ssl_write(&c->Ssl, (const unsigned char *)data + sent,
                            len - sent);
      if ((n == MBEDTLS_ERR_SSL_WANT_READ) ||
          (n == MBEDTLS_ERR_SSL_WANT_WRITE)) {
        continue;
      }
    } else {
      n = (int)send(c->Fd, data + sent, (int)(len - sent), 0);
    }
    if (n <= 0) {
      return false;
    }
    sent += (size_t)n;
  }
  return true;
}

/** @brief Read available bytes (one read) over the connection. */
static int ConnRecv(conn_t *const c, char *const buf, size_t len) {
  if (c->Tls) {
    int n = mbedtls_ssl_read(&c->Ssl, (unsigned char *)buf, len);
    if ((n == MBEDTLS_ERR_SSL_WANT_READ) || (n == MBEDTLS_ERR_SSL_WANT_WRITE)) {
      return -2; /* retry */
    }
    return n;
  }
  return (int)recv(c->Fd, buf, (int)len, 0);
}

/** @brief Release all connection resources. */
static void ConnClose(conn_t *const c) {
  if (c->Tls) {
    (void)mbedtls_ssl_close_notify(&c->Ssl);
    mbedtls_ssl_free(&c->Ssl);
    mbedtls_ssl_config_free(&c->Conf);
    mbedtls_ctr_drbg_free(&c->Drbg);
    mbedtls_entropy_free(&c->Entropy);
  }
  if (c->Fd != PINGDD_INVALID_SOCKET) {
    HttpCloseSocket(c->Fd);
  }
}

int32_t Http_Probe(pcc_t const url, const http_cfg_t *const cfg,
                   http_result_t *const out) {
  url_t u;
  conn_t c;
  pingdd_timer_t t_connect = (pingdd_timer_t){0};
  pingdd_timer_t t_total = (pingdd_timer_t){0};
  char req[1600] = {0};
  char resp[2048] = {0};
  size_t got = 0;
  int status = 0;
  pingdd_timer_t deadline = (pingdd_timer_t){0};

  if ((url == NULL) || (cfg == NULL) || (out == NULL)) {
    return PINGDD_INVALID_ARGS;
  }
  memset(out, 0, sizeof(*out));
  memset(&c, 0, sizeof(c));
  c.Fd = PINGDD_INVALID_SOCKET;

  if (!ParseUrl(url, &u)) {
    (void)snprintf(out->Error, sizeof(out->Error),
                   "malformed URL (use http(s)://host[:port][/path])");
    return PINGDD_INVALID_ARGS;
  }

  Timer_Start(&t_total);
  Timer_Start(&t_connect);
  c.Fd = TcpConnect(&u, cfg->TimeoutMs, out->Ip, sizeof(out->Ip), out->Error,
                    sizeof(out->Error));
  out->ConnectMs = Timer_Stop(&t_connect) * 1000.0;
  if (c.Fd == PINGDD_INVALID_SOCKET) {
    return PINGDD_SOCKET_FAILURE;
  }

  if (u.Https) {
    if (!TlsHandshake(&c, u.Host, out->Error, sizeof(out->Error))) {
      ConnClose(&c);
      return PINGDD_SOCKET_FAILURE;
    }
    c.Tls = true;
    out->Tls = true;
    (void)snprintf(out->TlsVersion, sizeof(out->TlsVersion), "%s",
                   mbedtls_ssl_get_version(&c.Ssl));
    (void)snprintf(out->Cipher, sizeof(out->Cipher), "%s",
                   mbedtls_ssl_get_ciphersuite(&c.Ssl));
  }

  (void)snprintf(req, sizeof(req),
                 "%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: PingDD\r\n"
                 "Accept: */*\r\nConnection: close\r\n\r\n",
                 cfg->Method, u.Path, u.Host);
  if (!ConnSendAll(&c, req, strlen(req))) {
    (void)snprintf(out->Error, sizeof(out->Error), "request send failed");
    ConnClose(&c);
    return PINGDD_SOCKET_FAILURE;
  }

  /* Read until we have the status line (or hit the buffer / timeout). */
  Timer_Start(&deadline);
  while (got < (sizeof(resp) - 1U)) {
    int n = 0;
    if ((Timer_Stop(&deadline) * 1000.0) > (double)cfg->TimeoutMs) {
      break;
    }
    deadline.hasValue = true;
    n = ConnRecv(&c, resp + got, sizeof(resp) - 1U - got);
    if (n == -2) {
      continue; /* TLS wants more I/O */
    }
    if (n <= 0) {
      break;
    }
    got += (size_t)n;
    resp[got] = '\0';
    if (strstr(resp, "\r\n") != NULL) {
      break; /* status line complete */
    }
  }

  out->TotalMs = Timer_Stop(&t_total) * 1000.0;
  ConnClose(&c);

  /* Parse "HTTP/1.x SSS ...". */
  if ((got >= 12U) && (strncmp(resp, "HTTP/", 5) == 0)) {
    pcc_t sp = strchr(resp, ' ');
    if (sp != NULL) {
      status = (int)strtol(sp + 1, NULL, 10);
    }
  }
  if (status == 0) {
    if (out->Error[0] == '\0') {
      (void)snprintf(out->Error, sizeof(out->Error), "no HTTP status received");
    }
    return PINGDD_SOCKET_FAILURE;
  }
  out->Status = status;
  return SUCCESS;
}

#ifdef PINGDD_FUZZ_URL
/**
 * @brief libFuzzer entry point exercising the URL parser with untrusted input.
 *
 * Build with: clang -DPINGDD_FUZZ_URL -fsanitize=fuzzer,address,undefined.
 * ParseUrl handles attacker-controlled strings (e.g. from a --targets file),
 * so it must never read out of bounds regardless of input.
 */
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  char buf[2048];
  url_t u;
  size_t n = (size < (sizeof(buf) - 1U)) ? size : (sizeof(buf) - 1U);
  memcpy(buf, data, n);
  buf[n] = '\0';
  (void)ParseUrl(buf, &u);
  return 0;
}
#endif /* PINGDD_FUZZ_URL */
