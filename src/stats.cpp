#include "stats.h"
#include "i18n.h"
#include <cstdio> // For snprintf, sprintf
#include <iostream>
#include <limits>  // For DBL_MAX
#include <iomanip> // For std::setprecision

double stats_c::Average()
{
	if (this->Attempts == 0)
	{
		return 0.0;
	}
	return (this->Total / static_cast<double>(this->Attempts));
}

void stats_c::UpdateMaxMin(double value)
{
	if (this->Attempts == 0)
	{
		this->Minimum = value;
		this->Maximum = value;
	}
	else
	{
		if (value < this->Minimum)
		{
			this->Minimum = value;
		}
		if (value > this->Maximum)
		{
			this->Maximum = value;
		}
	}
}

int stats_c::GetStatisticsString(char *str)
{
	const char *format = i18n_c::GetString(STRING_STATS).c_str();
	double failPercent = (static_cast<double>(this->Failures) / static_cast<double>(this->Attempts)) * 100.0;

	// Ensure format string is valid
	if (format == nullptr || strlen(format) == 0)
	{
		return 0; // Return 0 length if format string is invalid
	}

	// Calculate required length for formatted string
	int length = snprintf(NULL, 0, format, this->Attempts, this->Connects, this->Failures, failPercent, this->Minimum, this->Maximum, this->Average());

	// If str pointer is provided, format and store the string
	if (str != nullptr)
	{
		sprintf(str, format, this->Attempts, this->Connects, this->Failures, failPercent, this->Minimum, this->Maximum, this->Average());
	}

	return length;
}

void stats_c::Reset()
{
	this->Attempts = 0;
	this->Connects = 0;
	this->Failures = 0;
	this->Total = 0.0;
	this->Minimum = std::numeric_limits<double>::max();
	this->Maximum = 0.0;
}
