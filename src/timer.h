#pragma once
#ifndef TIMER_H
#define TIMER_H
#include "standard.h"
#include <sys/time.h>

class timer_c
{
public:
	void Start();
	double Stop();

private:
	bool hasValue_;
	struct timeval start_, stop_;
};

#endif