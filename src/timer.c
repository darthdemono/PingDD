/**
 * @file timer.c
 * @brief High-resolution monotonic timer implementation.
 *
 * Implements the functions declared in timer.h.
 */

#include "timer.h"

void Timer_Start(pingdd_timer_t *const timer) {
  if (timer == NULL) {
    return;
  }

  timer->hasValue = true;

#ifdef _WIN32
  (void)QueryPerformanceFrequency(&timer->freq);
  (void)QueryPerformanceCounter(&timer->start);
#else
  (void)clock_gettime(CLOCK_MONOTONIC, &timer->start);
#endif
}

double Timer_Stop(pingdd_timer_t *const timer) {
  double elapsed = 0.0;

  if ((timer == NULL) || (timer->hasValue == false)) {
    return 0.0;
  }

#ifdef _WIN32
  {
    LARGE_INTEGER end;
    double ticks = 0.0;

    (void)QueryPerformanceCounter(&end);

    ticks = (double)(end.QuadPart - timer->start.QuadPart);
    if (timer->freq.QuadPart != 0) {
      elapsed = ticks / (double)timer->freq.QuadPart;
    }
  }
#else
  {
    struct timespec end;
    long sec_diff = 0;
    long nsec_diff = 0;

    (void)clock_gettime(CLOCK_MONOTONIC, &end);

    sec_diff = (long)(end.tv_sec - timer->start.tv_sec);
    nsec_diff = (long)(end.tv_nsec - timer->start.tv_nsec);

    elapsed = (double)sec_diff + (double)nsec_diff / 1.0e9;
  }
#endif

  timer->hasValue = false;
  return elapsed;
}
