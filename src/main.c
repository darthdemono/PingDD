/**
 * @file main.c
 * @brief Main file for PingDD application
 * @author Jubair Hasan (Joy)
 */
#include "standard.h"
#include "socket.h"
#include "stats.h"
#include "print.h"
#include "i18n.h"
#include "arguments.h"
#include "version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#else
#endif

/* Global interrupt flag: set by SIGINT handler, polled in main loop */
static volatile sig_atomic_t g_interrupted = 0;

static inline void delay_ms(uint32_t ms)
{
#ifdef _WIN32
    Sleep(ms);
#else
    sleep(ms);
#endif
}
/* Static function prototypes */
static void SignalHandler(int signal);

/**
 * @brief Signal handler for SIGINT (Ctrl+C)
 * @param signal Signal number
 */
static void SignalHandler(int signal)
{
    if (signal == SIGINT)
    {
        g_interrupted = 1;
    }
}

/**
 * @brief Main entry point
 * @param argc Argument count
 * @param argv Argument vector
 * @return 0 on success, 1 on error
 */
int main(int argc, char *argv[])
{
    arguments_t args = {0};
    host_t host = {0};
    stats_t stats = {0};
    int resolve_result = 0;
    int connect_result = 0;
    int i = 0;
    double rtt = 0.0;
    char header[512U] = {0};

    /* Process command line arguments */
    if (ProcessArguments(argc, argv, &args) != 0)
    {
        return 1;
    }

    UseColor = args.UseColor;
    (void)signal(SIGINT, SignalHandler);

    Stats_Init(&stats);
    SetPortAndType(args.Port, IPPROTO_TCP, &host);

    /* Resolve hostname */
    resolve_result = Resolve(args.Destination, &host);
    if (resolve_result != SUCCESS)
    {
        PrintError(GetFriendlyTypeName(resolve_result));
        return 1;
    }

    /* Print header */
    (void)snprintf(header, sizeof(header),
                   "%s v%s - Copyright (c) %s\n",
                   NAME, PINGDD_VERSION_FULL, AUTHOR);
    FormattedPrint(PRINT_BLUE, header);

    /* Print connecting info (segment colors like original C++) */
    FormattedPrint(PRINT_YELLOW, "\nConnecting to ");
    FormattedPrint(PRINT_GREEN, args.Destination);
    FormattedPrint(PRINT_YELLOW, " on TCP ");
    {
        char port_buf[32U] = {0};
        (void)snprintf(port_buf, sizeof(port_buf), "%u",
                       (unsigned)args.Port);
        FormattedPrint(PRINT_GREEN, port_buf);
    }
    FormattedPrint(PRINT_YELLOW, ":\n");
    ResetColor();

    /* Main ping loop */
    while ((g_interrupted == 0) &&
           ((args.Count == -1) || (i < args.Count)))
    {
        rtt = 0.0;
        connect_result = Connect(&host, args.Timeout, &rtt);

        if (g_interrupted != 0)
        {
            break; /* fall through to stats print and clean exit */
        }

        if (connect_result == SUCCESS)
        {
            char rtt_buf[64U] = {0};
            char port_buf[32U] = {0};

            FormattedPrint(PRINT_WHITE, "Connected to ");
            FormattedPrint(PRINT_GREEN, host.IPAddress);
            FormattedPrint(PRINT_WHITE, ": time=");

            /* more accurate RTT: adjust precision here if needed */
            (void)snprintf(rtt_buf, sizeof(rtt_buf), "%.4fms ",
                           rtt * 1000.0);
            FormattedPrint(PRINT_GREEN, rtt_buf);

            FormattedPrint(PRINT_WHITE, "protocol=");
            FormattedPrint(PRINT_GREEN, "TCP ");
            FormattedPrint(PRINT_WHITE, "port=");

            (void)snprintf(port_buf, sizeof(port_buf), "%u",
                           (unsigned)host.Port);
            FormattedPrint(PRINT_GREEN, port_buf);
            (void)printf("\n");

            Stats_UpdateMaxMin(&stats, rtt); /* also updates Total */
            stats.Connects++;
        }
        else
        {
            FormattedPrint(PRINT_RED,
                           GetFriendlyTypeName(connect_result));
            (void)printf("\n");
            stats.Failures++;
        }

        stats.Attempts++;
        i++;

        if (args.Count != -1)
        {
            delay_ms(50U); /* 50 ms */
        }
    }

    /* Print final statistics */
    {
        char buf[64U] = {0};
        double fail_percent = 0.0;

        if (stats.Attempts > 0U)
        {
            fail_percent =
                ((double)stats.Failures / (double)stats.Attempts) * 100.0;
        }

        FormattedPrint(PRINT_YELLOW, "\nConnection statistics:\n");
        ResetColor();

        (void)printf("        Attempted = ");
        (void)snprintf(buf, sizeof(buf), "%lu",
                       (unsigned long)stats.Attempts);
        FormattedPrint(PRINT_BLUE, buf);

        (void)printf(" , Connected = ");
        (void)snprintf(buf, sizeof(buf), "%lu",
                       (unsigned long)stats.Connects);
        FormattedPrint(PRINT_BLUE, buf);

        (void)printf(" , Failed = ");
        (void)snprintf(buf, sizeof(buf), "%lu",
                       (unsigned long)stats.Failures);
        FormattedPrint(PRINT_BLUE, buf);

        (void)printf(" ( ");
        (void)snprintf(buf, sizeof(buf), "%.2f%%", fail_percent);
        FormattedPrint(PRINT_BLUE, buf);
        (void)printf(" )\n");

        FormattedPrint(PRINT_YELLOW,
                       "Approximate connection times:\n");
        ResetColor();

        (void)printf("        Minimum = ");
        (void)snprintf(buf, sizeof(buf), "%.4fms",
                       stats.Minimum * 1000.0);
        FormattedPrint(PRINT_BLUE, buf);

        (void)printf(" , Maximum = ");
        (void)snprintf(buf, sizeof(buf), "%.4fms",
                       stats.Maximum * 1000.0);
        FormattedPrint(PRINT_BLUE, buf);

        (void)printf(" , Average = ");
        (void)snprintf(buf, sizeof(buf), "%.5fms",
                       Stats_Average(&stats) * 1000.0);
        FormattedPrint(PRINT_BLUE, buf);
        (void)printf("\n");
    }

    return 0;
}
