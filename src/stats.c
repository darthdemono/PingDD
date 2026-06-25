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
#include <stdlib.h>
#include <string.h>

#define DBL_MAX_VALUE (DBL_MAX)

/** @brief Hard cap on retained samples to bound memory on infinite runs. */
#define MAX_RETAINED_SAMPLES (1000000U)

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

  if (variance < 0.0) {
    variance = 0.0;
  }

  return sqrt(variance);
}

void Stats_AddSample(stats_t *const stats, double const value) {
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

  /* Retain the sample for percentiles, up to a memory-bounded cap. */
  if (stats->SampleCount >= MAX_RETAINED_SAMPLES) {
    return;
  }
  if (stats->SampleCount == stats->SampleCap) {
    size_t new_cap = (stats->SampleCap == 0U) ? 64U : (stats->SampleCap * 2U);
    double *grown = (double *)realloc(stats->Samples, new_cap * sizeof(double));
    if (grown == NULL) {
      return; /* Out of memory: keep aggregate stats, drop percentiles. */
    }
    stats->Samples = grown;
    stats->SampleCap = new_cap;
  }
  stats->Samples[stats->SampleCount] = value;
  stats->SampleCount++;
}

static int CompareDouble(const void *a, const void *b) {
  double da = *(const double *)a;
  double db = *(const double *)b;
  if (da < db) {
    return -1;
  }
  if (da > db) {
    return 1;
  }
  return 0;
}

double Stats_Percentile(const stats_t *const stats, double const percent) {
  double *sorted = NULL;
  double result = 0.0;
  size_t idx = 0;

  if ((stats == NULL) || (stats->SampleCount == 0U) ||
      (stats->Samples == NULL)) {
    return 0.0;
  }

  sorted = (double *)malloc(stats->SampleCount * sizeof(double));
  if (sorted == NULL) {
    return 0.0;
  }
  memcpy(sorted, stats->Samples, stats->SampleCount * sizeof(double));
  qsort(sorted, stats->SampleCount, sizeof(double), CompareDouble);

  /* Nearest-rank method, clamped to valid index range. */
  if (percent <= 0.0) {
    idx = 0;
  } else if (percent >= 100.0) {
    idx = stats->SampleCount - 1U;
  } else {
    idx = (size_t)ceil((percent / 100.0) * (double)stats->SampleCount);
    if (idx > 0U) {
      idx -= 1U;
    }
    if (idx >= stats->SampleCount) {
      idx = stats->SampleCount - 1U;
    }
  }

  result = sorted[idx];
  free(sorted);
  return result;
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
  stats->Samples = NULL;
  stats->SampleCount = 0U;
  stats->SampleCap = 0U;
}

void Stats_Init(stats_t *const stats) { Stats_Reset(stats); }

void Stats_Free(stats_t *const stats) {
  if (stats == NULL) {
    return;
  }
  free(stats->Samples);
  stats->Samples = NULL;
  stats->SampleCount = 0U;
  stats->SampleCap = 0U;
}
