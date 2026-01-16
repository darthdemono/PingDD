/**
 * @file timer.h
 * @brief High-resolution timer functions.
 *
 * Declares a timer structure and functions used to measure elapsed time in
 * seconds.
 */
#ifndef TIMER_H
#define TIMER_H

#include "standard.h"

/**
 * @struct pingdd_timer_t
 * @brief Timer state struct used by @ref Timer_Start and @ref Timer_Stop.
 */
typedef struct {
  /** @brief True when the timer has been started and can be stopped. */
  bool hasValue;

#ifdef _WIN32
  /** @brief Start counter value. */
  LARGE_INTEGER start;

  /** @brief Stop counter storage (reserved for platform use). */
  LARGE_INTEGER stop;

  /** @brief Performance counter frequency. */
  LARGE_INTEGER freq;
#else
  /** @brief Start timestamp (monotonic clock). */
  struct timespec start;

  /** @brief Stop timestamp storage (reserved for platform use). */
  struct timespec stop;
#endif
} pingdd_timer_t;

/**
 * @brief Start the timer.
 *
 * Records the current monotonic time so elapsed time can be computed later.
 *
 * @param[out] timer Timer structure to update.
 * @note If @p timer is NULL, the function does nothing.
 */
void Timer_Start(pingdd_timer_t *const timer);

/**
 * @brief Stop the timer and return elapsed time.
 *
 * @param[in,out] timer Timer structure to read and update.
 * @return Elapsed time in seconds.
 *
 * @note Returns 0.0 if @p timer is NULL or if the timer was not started.
 */
double Timer_Stop(pingdd_timer_t *const timer);

#endif /* TIMER_H */
