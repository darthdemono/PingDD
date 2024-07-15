#ifndef STATS_H
#define STATS_H

#include <string>
#include <cfloat> // Include cfloat for DBL_MAX

class stats_c
{
public:
	int Attempts;
	int Connects;
	int Failures;
	double Total;
	double Minimum;
	double Maximum;

	stats_c() : Attempts(0), Connects(0), Failures(0), Total(0.0), Minimum(DBL_MAX), Maximum(0.0) {} // Constructor

	double Average();
	void UpdateMaxMin(double value);
	int GetStatisticsString(char *str);
	void Reset();
};

#endif // STATS_H
