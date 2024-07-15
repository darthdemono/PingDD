#include "gettimeofday.h"

#ifdef WIN32 // Windows specific
int gettimeofday(struct timeval *tp, struct timezone *tzp)
{
	// Implement gettimeofday for Windows
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag;

	if (tp != NULL)
	{
		GetSystemTimeAsFileTime(&ft);

		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		// Convert to microseconds
		tmpres /= 10;
		tmpres -= DELTA_EPOCH_IN_MICROSECS;

		tp->tv_sec = (long)(tmpres / 1000000UL);
		tp->tv_usec = (long)(tmpres % 1000000UL);
	}

	if (tzp != NULL)
	{
		if (!tzflag)
		{
			_tzset();
			tzflag++;
		}
		tzp->tz_minuteswest = _timezone / 60;
		tzp->tz_dsttime = _daylight;
	}

	return 0;
}
#endif
