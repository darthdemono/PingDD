/**
 * @file stats.h
 * @brief Statistics tracking interface
 * @author Jubair Hasan (Joy)
 */

#ifndef STATS_H
#define STATS_H

#include <stdbool.h>
#include <stddef.h>
/* Statistics counters */
typedef struct
{
    unsigned long Attempts;
    unsigned long Connects;
    unsigned long Failures;
    double Total;
    double Minimum;
    double Maximum;
} stats_t;

/**
 * @brief Initialize statistics structure
 * @param stats Statistics structure
 */
void Stats_Init(stats_t *const stats);

/**
 * @brief Reset all statistics counters
 * @param stats Statistics structure
 */
void Stats_Reset(stats_t *const stats);

/**
 * @brief Update minimum and maximum values
 * @param stats Statistics structure
 * @param value New value to compare
 */
void Stats_UpdateMaxMin(stats_t *const stats, double const value);

/**
 * @brief Calculate average connection time
 * @param stats Statistics structure
 * @return Average time in seconds
 */
double Stats_Average(const stats_t *const stats);

/**
 * @brief Generate formatted statistics string
 * @param stats Statistics structure
 * @param str Output buffer
 * @param str_size Buffer size
 * @return Characters written
 */
int Stats_GetStatisticsString(const stats_t *const stats,
                              char *const str,
                              size_t const str_size);

#endif /* STATS_H */
