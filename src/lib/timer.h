#ifndef TIMER_H
#define TIMER_H

#include "standard.h" /* Now has POSIX macros + time.h */

typedef struct
{
    bool hasValue;
#ifdef _WIN32
    LARGE_INTEGER start;
    LARGE_INTEGER stop;
    LARGE_INTEGER freq;
#else
    struct timespec start; /* NOW DEFINED */
    struct timespec stop;
#endif
} pingdd_timer_t;

void Timer_Start(pingdd_timer_t *const timer);
double Timer_Stop(pingdd_timer_t *const timer);

#endif
