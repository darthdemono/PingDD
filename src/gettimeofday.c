/**
 * @file gettimeofday.c
 * @brief Windows-compatible gettimeofday() implementation
 * @author Jubair Hasan (Joy)
 */

#ifdef _WIN32
#include "gettimeofday.h"
#include <windows.h>

/**
 * @brief Windows-compatible gettimeofday() implementation
 */
int gettimeofday(struct timeval *const tp,
                 struct timezone *const tzp)
{
    FILETIME file_time;
    ULARGE_INTEGER ularge;

    SYSTEMTIME system_time;
    GetSystemTime(&system_time);

    /* Convert to FILETIME */
    SystemTimeToFileTime(&system_time, &file_time);

    /* Convert to microseconds since epoch */
    ularge.LowPart = file_time.dwLowDateTime;
    ularge.HighPart = file_time.dwHighDateTime;

    /* Windows epoch -> Unix epoch adjustment */
    ularge.QuadPart -= 116444736000000000ULL; /* Jan 1, 1601 -> Jan 1, 1970 */
    ularge.QuadPart /= 10ULL;                 /* 100ns -> 1us */

    if (tp != NULL)
    {
        tp->tv_sec = (long)(ularge.QuadPart / 1000000ULL);
        tp->tv_usec = (long)(ularge.QuadPart % 1000000ULL);
    }

    /* tzp ignored on Windows */
    (void)tzp;

    return 0;
}
#endif /* _WIN32 */
