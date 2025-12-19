/**
 * @file timer.h
 * @brief High-resolution timer interface
 * @author Jubair Hasan (Joy)
 */

#ifndef TIMER_H
#define TIMER_H

#include <stdbool.h>
#include <sys/time.h>

/* Timer structure */
typedef struct
{
    bool hasValue;
    struct timeval start;
    struct timeval stop;
} timer_t;

/**
 * @brief Start high-resolution timer
 * @param timer Timer structure to initialize
 */
void Timer_Start(timer_t *const timer);

/**
 * @brief Stop timer and return elapsed time in seconds
 * @param timer Timer structure
 * @return Elapsed time in seconds (0.0 if invalid)
 */
double Timer_Stop(timer_t *const timer);

#endif /* TIMER_H */
