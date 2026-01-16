/**
 * @file gettimeofday.c
 * @brief gettimeofday compatibility implementation for Windows.
 *
 * Provides gettimeofday() for Windows builds.
 */

#ifdef _WIN32

#include "gettimeofday.h"

#include <windows.h>

int gettimeofday(struct timeval *const tp, struct timezone *const tzp) {
  FILETIME file_time;
  ULARGE_INTEGER ularge;
  SYSTEMTIME system_time;

  GetSystemTime(&system_time);

  /* Convert SYSTEMTIME to FILETIME (100 ns intervals since 1601-01-01). */
  SystemTimeToFileTime(&system_time, &file_time);

  ularge.LowPart = file_time.dwLowDateTime;
  ularge.HighPart = file_time.dwHighDateTime;

  /* Convert Windows epoch (1601-01-01) to Unix epoch (1970-01-01). */
  ularge.QuadPart -= 116444736000000000ULL;

  /* Convert 100 ns units to microseconds. */
  ularge.QuadPart /= 10ULL;

  if (tp != NULL) {
    tp->tv_sec = (long)(ularge.QuadPart / 1000000ULL);
    tp->tv_usec = (long)(ularge.QuadPart % 1000000ULL);
  }

  /* tzp is unused on Windows. */
  (void)tzp;

  return 0;
}

#endif /* _WIN32 */
