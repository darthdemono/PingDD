/**
 * @file unit_stats.c
 * @brief Self-contained unit tests for the statistics module (stats.c).
 *
 * stats.c has no platform or socket dependencies, so it can be compiled and
 * linked on its own. This exercises the pure RTT math (min/max/avg/stddev and
 * percentiles) that the diagnostics and summaries rely on. Built and run by the
 * makefile `test` target.
 */
#include "stats.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int g_fail = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
      g_fail++;                                                                \
    }                                                                          \
  } while (0)

/** @brief A value is within an absolute epsilon of the expected. */
static int near(double a, double b) { return fabs(a - b) < 1e-9; }

int main(void) {
  stats_t s;

  /* Empty stats: no division by zero, sane zeros. */
  Stats_Init(&s);
  CHECK(near(Stats_Average(&s), 0.0));
  CHECK(near(Stats_StdDev(&s), 0.0));
  CHECK(near(Stats_Percentile(&s, 95.0), 0.0));

  /* Samples 1,2,3,4 (seconds). avg=2.5, min=1, max=4.
   * Average/StdDev divide by Connects, which the caller maintains on success;
   * AddSample only accumulates Total/TotalSq, so set Connects to match. */
  Stats_AddSample(&s, 1.0);
  Stats_AddSample(&s, 2.0);
  Stats_AddSample(&s, 3.0);
  Stats_AddSample(&s, 4.0);
  s.Connects = 4U;
  CHECK(near(Stats_Average(&s), 2.5));
  CHECK(near(s.Minimum, 1.0));
  CHECK(near(s.Maximum, 4.0));
  CHECK(s.SampleCount == 4U);
  /* Population stddev of {1,2,3,4} = sqrt(1.25). */
  CHECK(near(Stats_StdDev(&s), sqrt(1.25)));
  /* Percentiles must be within the observed range and monotonic. */
  CHECK(Stats_Percentile(&s, 0.0) >= 1.0 - 1e-9);
  CHECK(Stats_Percentile(&s, 100.0) <= 4.0 + 1e-9);
  CHECK(Stats_Percentile(&s, 50.0) <= Stats_Percentile(&s, 95.0));

  /* Reset clears the accumulators. */
  Stats_Reset(&s);
  CHECK(s.SampleCount == 0U);
  CHECK(near(Stats_Average(&s), 0.0));

  Stats_Free(&s);

  if (g_fail == 0) {
    (void)printf("unit_stats: all checks passed\n");
    return 0;
  }
  (void)printf("unit_stats: %d failure(s)\n", g_fail);
  return 1;
}
