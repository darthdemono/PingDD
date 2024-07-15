#include "standard.h"
#include "socket.h"
#include "stats.h"
#include "print.h"
#include "i18n.h"
#include <iostream>
#include <cstdlib>	// For atoi
#include <iomanip>	// For std::setprecision
#include <csignal>	// For signal handling
#include <unistd.h> // For usleep

// Global variables for signal handler
bool g_interrupted = false;

// Signal handler function
void SignalHandler(int signal)
{
	if (signal == SIGINT)
	{
		std::cout << "\nReceived Ctrl+C. Printing statistics...\n";
		g_interrupted = true;
	}
}
// Function prototypes
void PrintUsage();
void PrintError(const std::string &message);

int main(int argc, char *argv[])
{
	// Command-line argument defaults
	int port = 0;
	int count = -1; // Infinite loop until specified
	int timeout = 1000;
	int packetSize = 0; // Default packet size (MTU)
	std::string destination;
	bool useColor = true;

	signal(SIGINT, SignalHandler);

	// Parse command-line arguments
	if (argc < 2)
	{
		PrintUsage();
		return 1;
	}

	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if (arg == "-?" || arg == "--help")
		{
			PrintUsage();
			return 0;
		}
		else if (arg == "-p" || arg == "--port")
		{
			if (i + 1 < argc)
			{
				port = atoi(argv[++i]);
			}
			else
			{
				PrintError("Error: -p/--port requires an argument.");
				return 1;
			}
		}
		else if (arg == "-c" || arg == "--count")
		{
			if (i + 1 < argc)
			{
				count = atoi(argv[++i]);
				if (count <= 0)
				{
					PrintError("Error: -c/--count must be a positive integer.");
					return 1;
				}
			}
			else
			{
				PrintError("Error: -c/--count requires an argument.");
				return 1;
			}
		}
		else if (arg == "-t" || arg == "--timeout")
		{
			if (i + 1 < argc)
			{
				timeout = atoi(argv[++i]);
			}
			else
			{
				PrintError("Error: -t/--timeout requires an argument.");
				return 1;
			}
		}
		else if (arg == "--nocolor")
		{
			useColor = false;
		}
		else if (arg == "--packet-size")
		{
			if (i + 1 < argc)
			{
				packetSize = atoi(argv[++i]);
				if (packetSize <= 0)
				{
					PrintError("Error: --packet-size must be a positive integer.");
					return 1;
				}
			}
			else
			{
				PrintError("Error: --packet-size requires an argument.");
				return 1;
			}
		}
		else
		{
			destination = arg;
		}
	}

	// Validate required arguments
	if (port == 0 || destination.empty())
	{
		PrintError("Error: Missing required arguments.");
		PrintUsage();
		return 1;
	}

	// Initialize socket and statistics objects
	socket_c socket;
	stats_c stats;
	host_c host;

	// Set port and type
	socket.SetPortAndType(port, IPPROTO_TCP, host);

	// Resolve destination IP address
	int resolveResult = socket.Resolve(destination, host);
	if (resolveResult != SUCCESS)
	{
		PrintError(socket.GetFriendlyTypeName(resolveResult));
		return 1;
	}

	// Print header
	if (useColor)
	{
		print_c::FormattedPrint(PRINT_COLOR_BLUE, NAME " v" VERSION " - Copyright (c) " AUTHOR "\n");
		print_c::FormattedPrint(PRINT_COLOR_YELLOW, "\nConnecting to ");
		print_c::FormattedPrint(PRINT_COLOR_GREEN, destination.c_str());
		print_c::FormattedPrint(PRINT_COLOR_YELLOW, " on TCP ");
		print_c::FormattedPrint(PRINT_COLOR_GREEN, std::to_string(port).c_str());
		print_c::FormattedPrint(PRINT_COLOR_YELLOW, ":\n");
		print_c::ResetColor();
	}
	else
	{
		std::cout << NAME " v" VERSION " - Copyright (c) " AUTHOR "\n";
		std::cout << "Connecting to " << destination << " on TCP " << port << ":\n";
		std::cout << "\n";
	}

	// Connect to the destination and gather statistics
	double totalTime = 0.0;
	int connectedCount = 0;
	int i = 0;
	while (!g_interrupted && (count == -1 || i < count))
	{
		double rtt = 0.0;
		int connectResult = socket.Connect(host, timeout, rtt);
		if (g_interrupted)
		{
			break; // Exit loop if interrupted
		}
		if (connectResult == SUCCESS)
		{
			totalTime += rtt;
			++connectedCount;
			if (useColor)
			{
				print_c::FormattedPrint(PRINT_COLOR_WHITE, "Connected to ");
				print_c::FormattedPrint(PRINT_COLOR_GREEN, host.IPAddress.c_str());
				print_c::FormattedPrint(PRINT_COLOR_WHITE, ": time=");
				// Format RTT in milliseconds with two decimal places
				std::ostringstream rttString;
				rttString << std::fixed << std::setprecision(2) << rtt * 1000.0 << "ms ";
				print_c::FormattedPrint(PRINT_COLOR_GREEN, rttString.str().c_str());
				print_c::FormattedPrint(PRINT_COLOR_WHITE, "protocol=");
				print_c::FormattedPrint(PRINT_COLOR_GREEN, "TCP ");
				print_c::FormattedPrint(PRINT_COLOR_WHITE, "port=");
				print_c::FormattedPrint(PRINT_COLOR_GREEN, std::to_string(port).c_str());
				std::cout << "\n";
			}
			else
			{
				std::cout << "Connected to " << host.IPAddress << ": time=" << std::fixed << std::setprecision(2) << rtt * 1000.0 << "ms protocol=TCP port=" << port << "\n";
			}
			// Update statistics
			stats.UpdateMaxMin(rtt);
		}
		else if (connectResult == ERROR_OUTOFMEMORY ||
				 connectResult == ERROR_SOCKET_TIMEOUT ||
				 connectResult == ERROR_SOCKET_GENERALFAILURE ||
				 connectResult == ERROR_SOCKET_CANNOTRESOLVE)
		{
			++stats.Failures;
			if (useColor)
			{
				print_c::FormattedPrint(PRINT_COLOR_RED, socket.GetFriendlyTypeName(connectResult).c_str());
				std::cout << "\n";
			}
			else
			{
				std::cout << socket.GetFriendlyTypeName(connectResult) << "\n";
			}
		}
		else
		{
			++stats.Failures;
			if (useColor)
			{
				print_c::FormattedPrint(PRINT_COLOR_RED, socket.GetFriendlyTypeName(connectResult).c_str());
			}
			else
			{
				std::cout << socket.GetFriendlyTypeName(connectResult) << "\n";
			}
		}
		++stats.Attempts;
		++i;
	}

	// Calculate statistics
	stats.Connects = connectedCount;
	stats.Total = totalTime;
	char statisticsStr[256];
	int len = stats.GetStatisticsString(statisticsStr);
	double failPercent = (static_cast<double>(stats.Failures) / static_cast<double>(stats.Attempts)) * 100.0;
	// Print connection statistics
	if (useColor)
	{
		print_c::FormattedPrint(PRINT_COLOR_YELLOW, "\nConnection statistics:\n");
		print_c::ResetColor(); // Ensure color reset before printing statistics

		// Convert numerical values to strings with appropriate formatting
		std::ostringstream attemptsStr, connectsStr, failuresStr, failPercentStr, minTimeStr, maxTimeStr, avgTimeStr;
		attemptsStr << stats.Attempts;
		connectsStr << stats.Connects;
		failuresStr << stats.Failures;
		failPercentStr << std::fixed << std::setprecision(2) << failPercent << "%";
		minTimeStr << std::fixed << std::setprecision(2) << stats.Minimum * 1000.0 << "ms";
		maxTimeStr << std::fixed << std::setprecision(2) << stats.Maximum * 1000.0 << "ms";
		avgTimeStr << std::fixed << std::setprecision(2) << stats.Average() * 1000.0 << "ms";

		// Output formatted strings with color
		std::cout << "        Attempted = ";
		print_c::FormattedPrint(PRINT_COLOR_BLUE, attemptsStr.str().c_str());
		std::cout << " , Connected = ";
		print_c::FormattedPrint(PRINT_COLOR_BLUE, connectsStr.str().c_str());
		std::cout << " , Failed = ";
		print_c::FormattedPrint(PRINT_COLOR_BLUE, failuresStr.str().c_str());
		std::cout << " ( ";
		print_c::FormattedPrint(PRINT_COLOR_BLUE, failPercentStr.str().c_str());
		std::cout << " )\n";

		std::cout << "Approximate connection times:\n";
		std::cout << "        Minimum = ";
		print_c::FormattedPrint(PRINT_COLOR_BLUE, minTimeStr.str().c_str());
		std::cout << " , Maximum = ";
		print_c::FormattedPrint(PRINT_COLOR_BLUE, maxTimeStr.str().c_str());
		std::cout << " , Average = ";
		print_c::FormattedPrint(PRINT_COLOR_BLUE, avgTimeStr.str().c_str());
		std::cout << "\n";
	}
	else
	{
		std::cout << "\nConnection statistics:\n";
		std::cout << "        Attempted = " << stats.Attempts << " , Connected = " << stats.Connects << " , Failed = " << stats.Failures << " ( " << std::fixed << std::setprecision(2) << failPercent << "% )\n";
		std::cout << "Approximate connection times:\n";
		std::cout << "        Minimum = " << std::fixed << std::setprecision(2) << stats.Minimum * 1000.0 << "ms , Maximum = " << std::fixed << std::setprecision(2) << stats.Maximum * 1000.0 << "ms , Average = " << std::fixed << std::setprecision(2) << stats.Average() * 1000.0 << "ms\n";
	}

	// Cleanup
	return 0;
}

void PrintUsage()
{
	std::cout << NAME " - A simple ping utility\n"
					  "Syntax: PingDD [options] destination\n"
					  "\n"
					  "Options:\n"
					  " -?, --help         display usage\n"
					  " -p, --port N       set TCP port N (required)\n"
					  " --packet-size N    set packet size to N bytes (default MTU)\n"
					  "     --nocolor      Disable color output\n"
					  " -t, --timeout N    timeout in milliseconds (default 1000)\n"
					  " -c, --count N      set number of checks to N (default infinite)\n";
}

void PrintError(const std::string &message)
{
	std::cerr << message << "\n";
}
