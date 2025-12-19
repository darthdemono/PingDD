/**
 * @file timer.c
 * @brief MISRA C compliant high-resolution timer implementation
 * @author Jubair Hasan (Joy)
 * @version 1.0.0
 * @date 2025-12-19
 */

#include "timer.h"

#include <sys/time.h> /* For gettimeofday */
#include <stdbool.h>

/* Constants */
#define MICROSECONDS_PER_SECOND (1000000.0)

/**
 * @brief Start high-resolution timer
 * @param timer Timer structure to initialize
 */
void Timer_Start(timer_t *const timer)
{
    if (timer != NULL)
    {
        timer->hasValue = true;
        (void)gettimeofday(&timer->start, NULL);
    }
}

/**
 * @brief Stop timer and return elapsed time in seconds
 * @param timer Timer structure
 * @return Elapsed time in seconds (0.0 if invalid)
 */
double Timer_Stop(timer_t *const timer)
{
    double elapsed_usec = 0.0;
    double start_usec = 0.0;
    double stop_usec = 0.0;

    /* Validate input */
    if ((timer == NULL) || (!timer->hasValue))
    {
        return 0.0;
    }

    /* Get stop time */
    (void)gettimeofday(&timer->stop, NULL);

    /* Convert to microseconds with overflow-safe calculation */
    start_usec = ((double)timer->start.tv_sec * MICROSECONDS_PER_SECOND) +
                 (double)timer->start.tv_usec;
    stop_usec = ((double)timer->stop.tv_sec * MICROSECONDS_PER_SECOND) +
                (double)timer->stop.tv_usec;

    /* Calculate elapsed time */
    elapsed_usec = stop_usec - start_usec;

    /* Return seconds */
    return (elapsed_usec / MICROSECONDS_PER_SECOND);
}
