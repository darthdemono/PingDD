/**
 * @file stats.c
 * @brief MISRA C compliant statistics tracking for PingDD
 * @author DarthDemono
 * @version 1.0.0
 * @date 2025-12-19
 */

#include "stats.h"
#include "i18n.h"

#include <stdio.h>
#include <string.h>
#include <float.h>
#include <stdbool.h>

/* Constants */
#define MICROSECONDS_PER_SECOND (1000000.0)
#define DBL_MAX_VALUE (DBL_MAX)

/**
 * @brief Calculate average connection time
 * @param stats Statistics structure
 * @return Average time in seconds (0.0 if no connections)
 */
double Stats_Average(const stats_t *const stats)
{
    if ((stats == NULL) || (stats->Connects == 0U))
    {
        return 0.0;
    }

    return (stats->Total / (double)stats->Connects);
}

/**
 * @brief Generate formatted statistics string
 * @param stats Statistics structure
 * @param str Output buffer
 * @param str_size Buffer size
 * @return Number of characters written (or would be written)
 */
int Stats_GetStatisticsString(const stats_t *const stats,
                              char *const str,
                              size_t const str_size)
{
    double fail_percent = 0.0;

    /* Validate parameters */
    if ((stats == NULL) || (str == NULL) || (str_size == 0U))
    {
        return 0;
    }

    /* Calculate failure percentage */
    if (stats->Attempts > 0U)
    {
        fail_percent = ((double)stats->Failures / (double)stats->Attempts) * 100.0;
    }

    return snprintf(str, str_size,
                    "Attempted = %lu , Connected = %lu , Failed = %lu ( %.2f%% )",
                    (unsigned long)stats->Attempts,
                    (unsigned long)stats->Connects,
                    (unsigned long)stats->Failures,
                    fail_percent);
}

/**
 * @brief Update minimum and maximum values
 */
void Stats_UpdateMaxMin(stats_t *const stats, double const value)
{
    if (stats == NULL)
    {
        return;
    }

    if (stats->Attempts == 0U)
    {
        stats->Minimum = value;
        stats->Maximum = value;
        stats->Total = value;
    }
    else
    {
        if (value < stats->Minimum)
        {
            stats->Minimum = value;
        }
        if (value > stats->Maximum)
        {
            stats->Maximum = value;
        }
        stats->Total += value;
    }
}

/**
 * @brief Reset all statistics counters
 * @param stats Statistics structure
 */
void Stats_Reset(stats_t *const stats)
{
    if (stats == NULL)
    {
        return;
    }

    stats->Attempts = 0U;
    stats->Connects = 0U;
    stats->Failures = 0U;
    stats->Total = 0.0;
    stats->Minimum = DBL_MAX_VALUE;
    stats->Maximum = 0.0;
}

/**
 * @brief Initialize statistics structure
 * @param stats Statistics structure
 */
void Stats_Init(stats_t *const stats)
{
    Stats_Reset(stats);
}
