#include "arguments.h"
#include "i18n.h" // Assuming this header file contains the declaration for i18n_c class
#include <iostream>
#include <cstdlib>
#include <new>

void arguments_c::PrintBanner()
{
	std::cout << "PingDD - A simple ping utility" << std::endl;
}

void arguments_c::PrintUsage()
{
	std::cout << i18n_c::GetString(STRING_USAGE) << std::endl;
}

int arguments_c::Process(int argc, char *argv[], arguments_c &arguments)
{
	for (int i = 1; i < argc; ++i)
	{
		if (std::string(argv[i]) == "--port" && i + 1 < argc)
		{
			arguments.Port = std::atoi(argv[i + 1]);
		}
		else if (std::string(argv[i]) == "--timeout" && i + 1 < argc)
		{
			arguments.Timeout = std::atoi(argv[i + 1]);
		}
		else if (std::string(argv[i]) == "--count" && i + 1 < argc)
		{
			arguments.Count = std::atoi(argv[i + 1]);
		}
		else if (std::string(argv[i]) == "--no-color")
		{
			arguments.UseColor = false;
		}
		else
		{
			arguments.Destination = argv[i];
		}
	}

	if (arguments.Port == 0 || arguments.Destination.empty())
	{
		PrintUsage();
		return -1;
	}

	return 0;
}
