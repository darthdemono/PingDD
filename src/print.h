#pragma once
#ifndef PRINT_H
#define PRINT_H

#include "standard.h"
#include <string>

#ifdef _WIN32
#include <Windows.h>

// Define color codes for Windows
#define PRINT_COLOR_BLUE (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define PRINT_COLOR_GREEN (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define PRINT_COLOR_RED (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define PRINT_COLOR_YELLOW (FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY)
#define PRINT_COLOR_WHITE (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)

#else

// Define color codes for Unix-like systems
#define PRINT_COLOR_BLUE 34
#define PRINT_COLOR_GREEN 32
#define PRINT_COLOR_RED 31
#define PRINT_COLOR_YELLOW 33
#define PRINT_COLOR_WHITE 37

#endif

class print_c
{
public:
	static int initialColors_; // Declare as static
	static bool UseColor;	   // Declare as static

	static void FormattedPrint(int color, const char *data);
	static void SetColor(int color);
	static void ResetColor();
};

#endif // PRINT_H
