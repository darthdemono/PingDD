/**
 * @file loadtest.h
 * @brief Authorized load testing and DDoS-resilience measurement.
 *
 * These modes generate concurrent connection load against a target you are
 * authorized to test (your own infrastructure) and measure how latency, loss,
 * and throughput behave under that load. They are bounded by a hard
 * concurrency cap and a maximum duration, and the CLI refuses public targets
 * unless explicitly authorized. This is a measurement tool, not an attack
 * tool: there are no amplification, reflection, spoofing, or bypass features.
 */
#ifndef PINGDD_LOADTEST_H
#define PINGDD_LOADTEST_H

#include "standard.h"

/** @brief Hard ceiling on concurrent workers, regardless of request. */
#define LOADTEST_MAX_CONCURRENCY 256U

/** @brief Hard ceiling on run duration in milliseconds (1 hour). */
#define LOADTEST_MAX_DURATION_MS 3600000U

/**
 * @struct loadtest_cfg_t
 * @brief Configuration for a load-test or resilience run.
 */
typedef struct {
  /** @brief Resolved target (primary address is used by every worker). */
  const host_t *Host;
  /** @brief Per-connection timeout in milliseconds. */
  uint32_t TimeoutMs;
  /** @brief Number of concurrent workers (clamped to the cap). */
  uint32_t Concurrency;
  /** @brief Total run duration in milliseconds (clamped to the cap). */
  uint32_t DurationMs;
  /** @brief When true, ramp concurrency in phases and report the curve. */
  bool Resilience;
  /** @brief Suppress progress output when true. */
  bool Silent;
} loadtest_cfg_t;

/**
 * @struct loadtest_result_t
 * @brief Aggregate outcome of a load-test run (or one resilience phase).
 */
typedef struct {
  /** @brief Concurrency level used for this result. */
  uint32_t Concurrency;
  /** @brief Wall-clock duration actually run, in seconds. */
  double DurationSec;
  /** @brief Total connection attempts. */
  unsigned long long Attempts;
  /** @brief Successful connections. */
  unsigned long long Success;
  /** @brief Failed connections. */
  unsigned long long Failed;
  /** @brief Sum of successful RTTs in seconds. */
  double TotalRtt;
  /** @brief Sum of squared successful RTTs (for stddev). */
  double TotalRttSq;
  /** @brief Minimum successful RTT in seconds. */
  double MinRtt;
  /** @brief Maximum successful RTT in seconds. */
  double MaxRtt;
} loadtest_result_t;

/**
 * @brief Run a single fixed-concurrency load test.
 *
 * @param[in]  cfg Configuration (Resilience is ignored here).
 * @param[out] out Aggregate result.
 * @retval 0  Completed.
 * @retval -1 Invalid arguments or thread-spawn failure.
 */
int LoadTest_Run(const loadtest_cfg_t *const cfg, loadtest_result_t *const out);

/**
 * @brief Run a resilience sweep: ramp concurrency 1,2,4,… up to Concurrency,
 * measuring degradation at each level, and print the curve plus the point at
 * which the service starts to degrade.
 *
 * @param[in] cfg Configuration (Concurrency is the maximum level).
 * @retval 0  Completed.
 * @retval -1 Invalid arguments or thread-spawn failure.
 */
int LoadTest_Resilience(const loadtest_cfg_t *const cfg);

/**
 * @brief Throughput in connections/second for a result.
 */
double LoadTest_Rps(const loadtest_result_t *const r);

/**
 * @brief Average successful RTT in seconds (0 if none).
 */
double LoadTest_AvgRtt(const loadtest_result_t *const r);

/**
 * @brief Standard deviation of successful RTT in seconds (0 if none).
 */
double LoadTest_StdDevRtt(const loadtest_result_t *const r);

#endif /* PINGDD_LOADTEST_H */
