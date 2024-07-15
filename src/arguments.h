#ifndef ARGUMENTS_H
#define ARGUMENTS_H

#include <string>

class arguments_c
{
public:
    int Port;
    int Timeout;
    int Count;
    bool Continuous; // Added Continuous
    bool UseColor;
    std::string Destination;
    int Type; // Added Type

    static void PrintBanner();
    static void PrintUsage();
    static int Process(int argc, char *argv[], arguments_c &arguments);
};

#endif // ARGUMENTS_H
