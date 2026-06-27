/**
 * @file http.h
 * @brief HTTP(S) service probe.
 *
 * Performs an application-level reachability check: open a TCP connection,
 * negotiate TLS for https URLs (via the bundled mbedTLS), send a request, and
 * read the response status line. This answers "is the service actually
 * serving?", which a bare TCP connect cannot.
 */
#ifndef PINGDD_HTTP_H
#define PINGDD_HTTP_H

#include "standard.h"

/**
 * @struct http_cfg_t
 * @brief HTTP probe parameters.
 */
typedef struct {
  /** @brief HTTP method (e.g. "GET", "HEAD"). */
  char Method[8];
  /** @brief Expected status code, or 0 to accept any 2xx/3xx. */
  int ExpectStatus;
  /** @brief Overall timeout in milliseconds. */
  uint32_t TimeoutMs;
  /** @brief Source IP/interface already applied globally (informational). */
  bool Quiet;
} http_cfg_t;

/**
 * @struct http_result_t
 * @brief Outcome of a single HTTP probe.
 */
typedef struct {
  /** @brief Parsed HTTP status code (0 if none received). */
  int Status;
  /** @brief TCP connect time in milliseconds. */
  double ConnectMs;
  /** @brief Time to the response status line in milliseconds. */
  double TotalMs;
  /** @brief True when the connection used TLS. */
  bool Tls;
  /** @brief Negotiated TLS protocol version string (empty if plain HTTP). */
  char TlsVersion[16];
  /** @brief Negotiated TLS cipher suite name (empty if plain HTTP). */
  char Cipher[64];
  /** @brief Resolved IP that was probed. */
  char Ip[64];
  /** @brief Error description on failure (empty on success). */
  char Error[160];
} http_result_t;

/**
 * @brief Probe an HTTP or HTTPS URL.
 *
 * @param[in]  url URL string (http:// or https://, optional :port and path).
 * @param[in]  cfg Probe parameters.
 * @param[out] out Result (always written; check @ref http_result_t::Status and
 *                 @ref http_result_t::Error).
 *
 * @retval SUCCESS             A status line was received.
 * @retval PINGDD_INVALID_ARGS Malformed URL or arguments.
 * @retval PINGDD_SOCKET_*     Connect/resolve/TLS failure (see Error).
 */
int32_t Http_Probe(pcc_t const url, const http_cfg_t *const cfg,
                   http_result_t *const out);

#endif /* PINGDD_HTTP_H */
