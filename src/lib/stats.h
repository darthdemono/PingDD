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
 * @brief Format a statistics summary string.
 *
 * Writes a single-line summary including attempted, connected, failed, and
 * failure percentage.
 *
 * @param[in]  stats    Statistics structure.
 * @param[out] str      Output buffer.
 * @param[in]  str_size Size of @p str in bytes.
 *
 * @return Number of characters written (or that would be written) as returned
 * by @c snprintf.
 * @note Returns 0 if any input is invalid.
 */
int Stats_GetStatisticsString(const stats_t *const stats, char *const str,
                              size_t const str_size);

#endif /* STATS_H */
