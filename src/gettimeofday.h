#pragma once

#include "standard.h"

#ifdef WIN32 // Windows specific
class gettimeofday_c
{
public:
	static int gettimeofday(struct timeval *tp, struct timezone *tzp);
};
#endif
