/**
 * @file stats.c
 * @brief Statistics tracking implementation.
 *
 * Implements the functions declared in stats.h.
 */

#include "stats.h"

#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define DBL_MAX_VALUE (DBL_MAX)

double Stats_Average(const stats_t *const stats) {
  if ((stats == NULL) || (stats->Connects == 0U)) {
    return 0.0;
  }

  return (stats->Total / (double)stats->Connects);
}

double Stats_StdDev(const stats_t *const stats) {
  double mean = 0.0;
  double variance = 0.0;

  if ((stats == NULL) || (stats->Connects == 0U)) {
    return 0.0;
  }

  mean = stats->Total / (double)stats->Connects;
  variance = (stats->TotalSq / (double)stats->Connects) - (mean * mean);

  /* Guard against tiny negative values from floating-point rounding. */
  if (variance < 0.0) {
    variance = 0.0;
  }

  return sqrt(variance);
}

void Stats_UpdateMaxMin(stats_t *const stats, double const value) {
  if (stats == NULL) {
    return;
  }

  if (value < stats->Minimum) {
    stats->Minimum = value;
  }

  if (value > stats->Maximum) {
    stats->Maximum = value;
  }

  stats->Total += value;
  stats->TotalSq += value * value;
}

void Stats_Reset(stats_t *const stats) {
  if (stats == NULL) {
    return;
  }

  stats->Attempts = 0U;
  stats->Connects = 0U;
  stats->Failures = 0U;
  stats->Total = 0.0;
  stats->TotalSq = 0.0;
  stats->Minimum = DBL_MAX_VALUE;
  stats->Maximum = 0.0;
}

void Stats_Init(stats_t *const stats) { Stats_Reset(stats); }
