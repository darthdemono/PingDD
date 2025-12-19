/**
 * @file timer.h
 * @brief Cross-platform monotonic high-resolution timer
 */

#ifndef TIMER_H
#define TIMER_H

#include "standard.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

typedef struct
{
    bool hasValue;
#ifdef _WIN32
    LARGE_INTEGER start;
    LARGE_INTEGER stop;
    LARGE_INTEGER freq;
#else
    struct timespec start;
    struct timespec stop;
#endif
} timer_t;

/**
 * @brief Start the timer
 */
void Timer_Start(timer_t *const timer);

/**
 * @brief Stop the timer and return elapsed time in seconds
 */
double Timer_Stop(timer_t *const timer);

#endif /* TIMER_H */
