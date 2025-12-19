/**
 * @file gettimeofday.h - FIXED
 */
#ifndef GETTIMEOFDAY_H
#define GETTIMEOFDAY_H

#include "standard.h"

struct timezone {
    int tz_minuteswest;
    int tz_dsttime;
};

int gettimeofday(struct timeval * const tp, struct timezone * const tzp);

#endif /* GETTIMEOFDAY_H */
