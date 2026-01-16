/**
 * @file stats.c
 * @brief Statistics tracking implementation.
 *
 * Implements the functions declared in stats.h.
 */

#include "stats.h"
#include "i18n.h"

#include <float.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MICROSECONDS_PER_SECOND (1000000.0)
#define DBL_MAX_VALUE (DBL_MAX)

double Stats_Average(const stats_t *const stats) {
  if ((stats == NULL) || (stats->Connects == 0U)) {
    return 0.0;
  }

  return (stats->Total / (double)stats->Connects);
}

int Stats_GetStatisticsString(const stats_t *const stats, char *const str,
                              size_t const str_size) {
  double fail_percent = 0.0;

  if ((stats == NULL) || (str == NULL) || (str_size == 0U)) {
    return 0;
  }

  if (stats->Attempts > 0U) {
    fail_percent = ((double)stats->Failures / (double)stats->Attempts) * 100.0;
  }

  return snprintf(str, str_size,
                  "Attempted = %lu , Connected = %lu , Failed = %lu ( %.2f%% )",
                  (unsigned long)stats->Attempts,
                  (unsigned long)stats->Connects,
                  (unsigned long)stats->Failures, fail_percent);
}

void Stats_UpdateMaxMin(stats_t *const stats, double const value) {
  if (stats == NULL) {
    return;
  }

  if (stats->Attempts == 0U) {
    stats->Minimum = value;
    stats->Maximum = value;
    stats->Total = value;
  } else {
    if (value < stats->Minimum) {
      stats->Minimum = value;
    }

    if (value > stats->Maximum) {
      stats->Maximum = value;
    }

    stats->Total += value;
  }
}

void Stats_Reset(stats_t *const stats) {
  if (stats == NULL) {
    return;
  }

  stats->Attempts = 0U;
  stats->Connects = 0U;
  stats->Failures = 0U;
  stats->Total = 0.0;
  stats->Minimum = DBL_MAX_VALUE;
  stats->Maximum = 0.0;
}

void Stats_Init(stats_t *const stats) { Stats_Reset(stats); }
