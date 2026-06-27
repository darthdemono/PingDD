/**
 * @file targets.h
 * @brief Multi-target probe set: many hosts x ports x protocols.
 *
 * Expands command-line inputs (positional hosts + port lists/ranges + protocol
 * lists, repeated --target specs, and a --targets file) into a flat list of
 * probe targets, each carrying its own resolved addresses, statistics, and
 * diagnostics.
 */
#ifndef PINGDD_TARGETS_H
#define PINGDD_TARGETS_H

#include "arguments.h"
#include "diag.h"
#include "standard.h"
#include "stats.h"

/**
 * @struct probe_target_t
 * @brief One probe destination (host + port + protocol) with its own state.
 */
typedef struct {
  /** @brief Display label, e.g. "1.1.1.1:443 tcp" or "8.8.8.8 icmp". */
  char Label[336];
  /** @brief Host string as given (hostname or IP literal). */
  char Host[256];
  /** @brief Target port (0 for ICMP). */
  uint16_t Port;
  /** @brief Protocol (IPPROTO_TCP / UDP / ICMP). */
  int32_t Type;

  /** @brief Resolved addresses + connection settings. */
  host_t Resolved;
  /** @brief True if resolution succeeded. */
  bool ResolveOk;
  /** @brief Resolve error code when ResolveOk is false. */
  int32_t ResolveErr;

  /** @brief Per-target statistics. */
  stats_t Stats;
  /** @brief Per-target diagnostics. */
  diag_t Diag;

  /** @brief Scratch: last probe result (used by concurrent scheduling). */
  int32_t LastResult;
  /** @brief Scratch: last RTT in seconds. */
  double LastRtt;
  /** @brief Scratch: last probed IP string. */
  char LastIp[64];
  /** @brief Scratch: TTL/hop-limit of the last reply (-1 if unavailable). */
  int32_t LastTtl;
} probe_target_t;

/**
 * @struct target_list_t
 * @brief Growable list of probe targets.
 */
typedef struct {
  probe_target_t *Items;
  size_t Count;
  size_t Cap;
} target_list_t;

/**
 * @brief Build the target list from parsed arguments.
 *
 * @param[in]  args     Parsed command-line arguments.
 * @param[out] list     Target list to populate (caller frees with
 *                      @ref Targets_Free).
 * @param[out] err      Buffer for a human-readable error on failure.
 * @param[in]  err_size Size of @p err.
 * @retval 0  Built at least one target.
 * @retval -1 Invalid input (message written to @p err).
 */
int Targets_Build(const arguments_t *const args, target_list_t *const list,
                  char *const err, size_t const err_size);

/**
 * @brief Resolve every target's host; sets ResolveOk/ResolveErr per target.
 *
 * @param[in,out] list   Target list.
 * @return Number of targets that resolved successfully.
 */
size_t Targets_Resolve(target_list_t *const list);

/**
 * @brief Free a target list's storage.
 * @param[in,out] list Target list.
 */
void Targets_Free(target_list_t *const list);

#endif /* PINGDD_TARGETS_H */
