/**
 * @file traceroute.h
 * @brief TTL-stepped path discovery (traceroute) for PingDD.
 *
 * Sends ICMP echo probes with an increasing TTL / hop-limit and records the
 * router that returns each "time exceeded" message, mapping the path to the
 * destination. IPv4 and IPv6 are both supported.
 *
 * On POSIX this needs a raw ICMP socket (root or CAP_NET_RAW), like classic
 * traceroute. On Windows it uses the IP Helper API (no special privilege).
 */
#ifndef PINGDD_TRACEROUTE_H
#define PINGDD_TRACEROUTE_H

#include "standard.h"

/**
 * @struct trace_cfg_t
 * @brief Traceroute run parameters.
 */
typedef struct {
  /** @brief Maximum number of hops to probe. */
  int MaxHops;
  /** @brief Probes sent per hop. */
  int Queries;
  /** @brief Per-probe timeout in milliseconds. */
  uint32_t TimeoutMs;
  /** @brief When true, reverse-resolve each hop address to a name. */
  bool Resolve;
} trace_cfg_t;

/**
 * @brief Run a traceroute to a resolved target's primary address.
 *
 * @param[in] target Resolved host (uses its first address and family).
 * @param[in] cfg    Run parameters.
 *
 * @retval SUCCESS            The destination was reached.
 * @retval PINGDD_SOCKET_FAILURE Could not create the probe socket (e.g. no
 *                               privilege) or the destination was not reached.
 * @retval PINGDD_INVALID_ARGS  Invalid arguments.
 */
int32_t Traceroute_Run(const host_t *const target, const trace_cfg_t *const cfg);

#endif /* PINGDD_TRACEROUTE_H */
