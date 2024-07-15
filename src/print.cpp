#include "print.h"
#include <iostream>

#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

int print_c::initialColors_ = PRINT_COLOR_WHITE; // Initialize initial color to white
bool print_c::UseColor = true;					 // Assuming color output is enabled by default

void print_c::FormattedPrint(int color, const char *data)
{
	if (UseColor)
	{
#ifdef _WIN32
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
		std::cout << data;
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), initialColors_);
#else
		std::cout << "\033[" << color << "m" << data << "\033[0m";
#endif
	}
	else
	{
		std::cout << data;
	}
}

void print_c::SetColor(int color)
{
	if (UseColor)
	{
#ifdef _WIN32
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
#else
		std::cout << "\033[" << color << "m";
#endif
	}
}

void print_c::ResetColor()
{
	if (UseColor)
	{
#ifdef _WIN32
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), initialColors_);
#else
		std::cout << "\033[0m";
#endif
	}
}
