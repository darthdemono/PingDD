/**
 * @file diag.h
 * @brief Network-quality diagnostics: downtime, latency, jitter, loss,
 *        bufferbloat, DNS health, and noise-vs-issue classification.
 *
 * The diagnostics layer observes each probe result, distinguishes transient
 * noise (isolated drops) from real incidents (sustained outages, degraded
 * links), raises live state-change alerts, and prints a final assessment.
 */
#ifndef PINGDD_DIAG_H
#define PINGDD_DIAG_H

#include "standard.h"
#include "stats.h"

/** @brief Consecutive failures before a link is declared DOWN (vs. noise). */
#define DIAG_DOWN_THRESHOLD 3U

/** @brief DNS resolution slower than this (ms) is flagged. */
#define DIAG_SLOW_DNS_MS 300.0

/** @brief EWMA smoothing factor for tracked latency. */
#define DIAG_EWMA_ALPHA 0.3

/** @brief Sustained latency above this multiple of baseline raises an alert. */
#define DIAG_LATENCY_SPIKE_FACTOR 3.0

/** @brief Minimum absolute latency rise (ms) before a spike is reported. */
#define DIAG_LATENCY_SPIKE_FLOOR_MS 20.0

/**
 * @struct diag_t
 * @brief Live diagnostic state accumulated across a run.
 */
typedef struct {
  /** @brief DNS resolution time in milliseconds. */
  double DnsMs;
  /** @brief True if DNS resolution failed. */
  bool DnsFailed;

  /** @brief Current consecutive-failure streak. */
  unsigned long ConsecFails;
  /** @brief Longest consecutive-failure streak observed. */
  unsigned long LongestFailStreak;
  /** @brief Number of distinct outage episodes (streak >= threshold). */
  unsigned long Outages;
  /** @brief True while currently in a declared-DOWN episode. */
  bool IsDown;

  /** @brief Baseline (minimum) latency in seconds. */
  double BaselineSec;
  /** @brief Exponentially-weighted moving average latency in seconds. */
  double EwmaSec;
  /** @brief True once the EWMA has been seeded. */
  bool EwmaInit;
  /** @brief True while a sustained high-latency condition is active. */
  bool HighLatency;

  /** @brief Suppress live alert printing when true (quiet/JSON modes). */
  bool Silent;
} diag_t;

/**
 * @brief Initialize diagnostic state.
 * @param[out] diag   State to initialize.
 * @param[in]  silent When true, no live alerts are printed.
 */
void Diag_Init(diag_t *const diag, bool const silent);

/**
 * @brief Record the DNS resolution outcome.
 * @param[in,out] diag   Diagnostic state.
 * @param[in]     dns_ms Resolution time in milliseconds.
 * @param[in]     failed True if resolution failed.
 */
void Diag_SetDns(diag_t *const diag, double const dns_ms, bool const failed);

/**
 * @brief Observe one probe result, updating state and emitting live alerts on
 * meaningful transitions (down, recovery, latency spike).
 *
 * @param[in,out] diag    Diagnostic state.
 * @param[in]     success True if the probe succeeded.
 * @param[in]     rtt_sec Round-trip time in seconds (ignored on failure).
 */
void Diag_Observe(diag_t *const diag, bool const success, double const rtt_sec);

/**
 * @brief Print the final human-readable diagnostics block.
 * @param[in] diag  Diagnostic state.
 * @param[in] stats Aggregate statistics for the run.
 */
void Diag_PrintSummary(const diag_t *const diag, const stats_t *const stats);

/**
 * @brief Print the final diagnostics as a JSON object (one line).
 * @param[in,out] out   Destination stream (stdout or an open log file).
 * @param[in] diag  Diagnostic state.
 * @param[in] stats Aggregate statistics for the run.
 */
void Diag_PrintJson(FILE *const out, const diag_t *const diag,
                    const stats_t *const stats);

#endif /* PINGDD_DIAG_H */
