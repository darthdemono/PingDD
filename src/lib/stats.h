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
 * @brief Update minimum, maximum, and total using a new sample value.
 *
 * @param[in,out] stats Statistics structure to update.
 * @param[in]     value New sample value in seconds.
 *
 * @note If this is the first sample, minimum and maximum are set to @p value.
 */
void Stats_UpdateMaxMin(stats_t *const stats, double const value);

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
