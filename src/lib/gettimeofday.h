/**
 * @file gettimeofday.h
 * @brief gettimeofday compatibility interface.
 *
 * Declares a gettimeofday() function for platforms that do not provide it
 * natively.
 */
#ifndef PINGDD_GETTIMEOFDAY_H
#define PINGDD_GETTIMEOFDAY_H

#include "standard.h"

/**
 * @brief Get the current time of day.
 *
 * Fills a @c struct timeval with the current wall-clock time.
 *
 * @param[out] tp  Output time value.
 * @param[in]  tzp Timezone information (may be unused depending on platform).
 * @retval 0  Success.
 * @retval -1 Error.
 */
int gettimeofday(struct timeval *const tp, struct timezone *const tzp);

#endif /* PINGDD_GETTIMEOFDAY_H */
