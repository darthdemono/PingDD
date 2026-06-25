/**
 * @file stats.h
 * @brief Statistics tracking functions.
 *
 * Declares the @ref stats_t structure and functions used to initialize, reset,
 * update, and format connection statistics.
 */
#ifndef STATS_H
#define STATS_H

#include <stddef.h>

/**
 * @struct stats_t
 * @brief Statistics counters and timing aggregates.
 */
typedef struct {
  /** @brief Total number of connection attempts. */
  unsigned long Attempts;

  /** @brief Number of successful connections. */
  unsigned long Connects;

  /** @brief Number of failed connections. */
  unsigned long Failures;

  /** @brief Sum of observed connection times in seconds. */
  double Total;

  /** @brief Sum of squares of observed connection times (for stddev). */
  double TotalSq;

  /** @brief Minimum observed connection time in seconds. */
  double Minimum;

  /** @brief Maximum observed connection time in seconds. */
  double Maximum;

  /** @brief Retained RTT samples (seconds) for percentile calculation. */
  double *Samples;

  /** @brief Number of valid entries in @ref Samples. */
  size_t SampleCount;

  /** @brief Allocated capacity of @ref Samples. */
  size_t SampleCap;
} stats_t;

/**
 * @brief Initialize a statistics structure.
 *
 * @param[out] stats Statistics structure to initialize.
 *
 * @note This function resets all fields to their default values.
 */
void Stats_Init(stats_t *const stats);

/**
 * @brief Reset all statistics counters and aggregates.
 *
 * @param[in,out] stats Statistics structure to reset.
 */
void Stats_Reset(stats_t *const stats);

/**
 * @brief Record a new RTT sample: update min/max/total and retain the value
 * for percentile calculation.
 *
 * @param[in,out] stats Statistics structure to update.
 * @param[in]     value New sample value in seconds.
 */
void Stats_AddSample(stats_t *const stats, double const value);

/**
 * @brief Compute a percentile of the retained samples.
 *
 * @param[in] stats   Statistics structure.
 * @param[in] percent Percentile in the range [0, 100].
 * @return The percentile value in seconds, or 0.0 if no samples exist.
 */
double Stats_Percentile(const stats_t *const stats, double const percent);

/**
 * @brief Release any memory retained for percentile samples.
 *
 * @param[in,out] stats Statistics structure to clean up.
 */
void Stats_Free(stats_t *const stats);

/**
 * @brief Calculate the average connection time.
 *
 * @param[in] stats Statistics structure.
 * @return Average time in seconds.
 *
 * @note Returns 0.0 if @p stats is NULL or if no successful connections were
 * recorded.
 */
double Stats_Average(const stats_t *const stats);

/**
 * @brief Calculate the population standard deviation of connection times.
 *
 * @param[in] stats Statistics structure.
 * @return Standard deviation in seconds, or 0.0 if fewer than one sample.
 */
double Stats_StdDev(const stats_t *const stats);

#endif /* STATS_H */
