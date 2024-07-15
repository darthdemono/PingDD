#include "standard.h"
#include <iostream>
#include <new>
#include "timer.h"
#include <sys/time.h>

void timer_c::Start()
{
	hasValue_ = true;
	gettimeofday(&start_, NULL);
}

double timer_c::Stop()
{
	if (!hasValue_)
		return 0.0;
	gettimeofday(&stop_, NULL);
	double usecStop = static_cast<double>(stop_.tv_usec) + (static_cast<double>(stop_.tv_sec) * 1000000);
	double usecStart = static_cast<double>(start_.tv_usec) + (static_cast<double>(start_.tv_sec) * 1000000);
	return (usecStop - usecStart) / 1000000.0;
}