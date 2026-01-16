/**
 * @file main.c
 * @brief PingDD application entry point.
 *
 * Contains the program main loop, signal handling, timestamp formatting,
 * optional CSV logging, and final statistics printing.
 */

#include "arguments.h"
#include "csv.h"
#include "i18n.h"
#include "print.h"
#include "socket.h"
#include "standard.h"
#include "stats.h"
#include "version.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef _WIN32
#include <windows.h>
#else
#endif

/**
 * @brief Interrupt flag set by the SIGINT handler.
 *
 * The main loop checks this flag to stop cleanly when Ctrl+C is pressed.
 */
static volatile sig_atomic_t g_interrupted = 0;

/**
 * @brief Global CSV output file handle.
 */
static FILE *csv_file = NULL;

/**
 * @brief Build an ISO 8601 timestamp string with timezone offset.
 *
 * @param[out] buf      Output buffer.
 * @param[in]  buf_size Size of @p buf in bytes.
 */
static inline void GetTimestampString(char *const buf, const size_t buf_size) {
  time_t now = time(NULL);
  struct tm *tm_local = localtime(&now);

#ifdef _WIN32
  DYNAMIC_TIME_ZONE_INFORMATION tz_info;
  DWORD result = GetDynamicTimeZoneInformation(&tz_info);

  LONG bias_minutes = (result == TIME_ZONE_ID_DAYLIGHT)
                          ? tz_info.Bias + tz_info.DaylightBias
                          : tz_info.Bias;

  int timezone_offset_minutes = -bias_minutes;
#else
  extern long timezone;
  extern int daylight;

  long tz_seconds = timezone;
  if (daylight != 0) {
    tz_seconds -= 3600;
  }

  int timezone_offset_minutes = -(int)(tz_seconds / 60);
#endif

  int timezone_hours = timezone_offset_minutes / 60;
  int timezone_mins = abs(timezone_offset_minutes % 60);

  char sign = (timezone_offset_minutes >= 0) ? '+' : '-';
  int abs_hours = abs(timezone_hours);

  (void)snprintf(buf, buf_size, "%04d-%02d-%02dT%02d:%02d:%02d%c%02d:%02d",
                 tm_local->tm_year + 1900, tm_local->tm_mon + 1,
                 tm_local->tm_mday, tm_local->tm_hour, tm_local->tm_min,
                 tm_local->tm_sec, sign, abs_hours, timezone_mins);
}

/**
 * @brief Sleep for a number of milliseconds.
 *
 * @param[in] ms Time to sleep in milliseconds.
 */
static inline void delay_ms(const uint32_t ms) {
#ifdef _WIN32
  Sleep(ms);
#else
  sleep(ms / 1000U);
#endif
}

/**
 * @brief SIGINT handler.
 *
 * @param[in] signal Signal number (unused).
 */
static void SignalHandler(int signal) {
  (void)signal;
  g_interrupted = 1;
}

/**
 * @brief Print the current timestamp using the configured output coloring.
 */
static inline void PrintTimestamp(void) {
  char ts_buf[64U] = {0};
  GetTimestampString(ts_buf, sizeof(ts_buf));
  FormattedPrint(PRINT_GREEN, ts_buf);
}

/**
 * @brief Program entry point.
 *
 * @param[in] argc Number of command-line arguments.
 * @param[in] argv Array of command-line argument strings.
 * @return Process exit code.
 */
int main(int argc, char *const *argv) {
  arguments_t args = {0};
  host_t host = {0};
  stats_t stats = {0};
  int32_t resolve_result = 0;
  int32_t connect_result = 0;
  int32_t i = 0;
  double rtt = 0.0;
  char header[512U] = {0};

  if (ProcessArguments(argc, argv, &args) != 0) {
    return 1;
  }

  UseColor = args.UseColor;
  (void)signal(SIGINT, SignalHandler);

  Stats_Init(&stats);
  SetPortAndType(args.Port, IPPROTO_TCP, &host);

  resolve_result = Resolve(args.Destination, &host);
  if (resolve_result != SUCCESS) {
    PrintError(GetFriendlyTypeName(resolve_result));
    return 1;
  }

  if (args.CSVOutput) {
    char *csv_filename = GenerateCSVFilename(&args);
    csv_file = fopen(csv_filename, "w");
    if (csv_file == NULL) {
      PrintError("Failed to create CSV file");
      return 1;
    }
    (void)WriteCSVHeader(csv_file, &host);

    FormattedPrint(PRINT_YELLOW, "\nCSV logging: ");
    FormattedPrint(PRINT_GREEN, csv_filename);
    FormattedPrint(PRINT_YELLOW, "\n");
  }

  (void)snprintf(header, sizeof(header), "%s v%s - Copyright (c) %s\n", NAME,
                 PINGDD_VERSION_FULL, AUTHOR);
  FormattedPrint(PRINT_BLUE, header);

  FormattedPrint(PRINT_YELLOW, "Connecting to ");
  FormattedPrint(PRINT_GREEN, args.Destination);
  FormattedPrint(PRINT_YELLOW, " on TCP ");
  {
    char port_buf[32U] = {0};
    (void)snprintf(port_buf, sizeof(port_buf), "%u", (unsigned)args.Port);
    FormattedPrint(PRINT_GREEN, port_buf);
  }
  FormattedPrint(PRINT_YELLOW, " on ");
  PrintTimestamp();
  FormattedPrint(PRINT_YELLOW, ":\n");
  ResetColor();

  while ((g_interrupted == 0) && ((args.Count == -1) || (i < args.Count))) {
    rtt = 0.0;
    connect_result = Connect(&host, args.Timeout, &rtt);

    if (g_interrupted != 0) {
      break;
    }

    if (connect_result == SUCCESS) {
      char rtt_buf[64U] = {0};
      char port_buf[32U] = {0};

      FormattedPrint(PRINT_WHITE, "Connected to  ");
      FormattedPrint(PRINT_GREEN, host.IPAddress);
      FormattedPrint(PRINT_WHITE, ": time=");

      (void)snprintf(rtt_buf, sizeof(rtt_buf), "%.4fms ", rtt * 1000.0);
      FormattedPrint(PRINT_GREEN, rtt_buf);

      FormattedPrint(PRINT_WHITE, "protocol=");
      FormattedPrint(PRINT_GREEN, "TCP ");
      FormattedPrint(PRINT_WHITE, "port=");

      (void)snprintf(port_buf, sizeof(port_buf), "%u", (unsigned)host.Port);
      FormattedPrint(PRINT_GREEN, port_buf);
      FormattedPrint(PRINT_WHITE, " datetime=");
      PrintTimestamp();
      (void)printf("\n");

      if (args.CSVOutput && (csv_file != NULL)) {
        char datetime_buf[64U] = {0};
        GetTimestampString(datetime_buf, sizeof(datetime_buf));
        (void)WriteCSVRow(csv_file, &host, rtt, datetime_buf);
      }

      Stats_UpdateMaxMin(&stats, rtt);
      stats.Connects++;
    } else {
      FormattedPrint(PRINT_RED, GetFriendlyTypeName(connect_result));
      (void)printf("\n");
      stats.Failures++;
    }

    stats.Attempts++;
    i++;

    if (args.Rate > 0U) {
      delay_ms(args.Rate);
    } else if (args.Count != -1) {
      delay_ms(50U);
    }
  }

  {
    char buf[64U] = {0};
    double fail_percent = 0.0;

    if (stats.Attempts > 0U) {
      fail_percent = ((double)stats.Failures / (double)stats.Attempts) * 100.0;
    }

    FormattedPrint(PRINT_YELLOW, "\nConnection statistics:\n");
    ResetColor();

    (void)printf("        Attempted = ");
    (void)snprintf(buf, sizeof(buf), "%lu", (unsigned long)stats.Attempts);
    FormattedPrint(PRINT_BLUE, buf);

    (void)printf(" , Connected = ");
    (void)snprintf(buf, sizeof(buf), "%lu", (unsigned long)stats.Connects);
    FormattedPrint(PRINT_BLUE, buf);

    (void)printf(" , Failed = ");
    (void)snprintf(buf, sizeof(buf), "%lu", (unsigned long)stats.Failures);
    FormattedPrint(PRINT_BLUE, buf);

    (void)printf(" ( ");
    (void)snprintf(buf, sizeof(buf), "%.2f%%", fail_percent);
    FormattedPrint(PRINT_BLUE, buf);
    (void)printf(" )\n");

    FormattedPrint(PRINT_YELLOW, "Approximate connection times:\n");
    ResetColor();

    (void)printf("        Minimum = ");
    (void)snprintf(buf, sizeof(buf), "%.4fms", stats.Minimum * 1000.0);
    FormattedPrint(PRINT_BLUE, buf);

    (void)printf(" , Maximum = ");
    (void)snprintf(buf, sizeof(buf), "%.4fms", stats.Maximum * 1000.0);
    FormattedPrint(PRINT_BLUE, buf);

    (void)printf(" , Average = ");
    (void)snprintf(buf, sizeof(buf), "%.5fms", Stats_Average(&stats) * 1000.0);
    FormattedPrint(PRINT_BLUE, buf);
    (void)printf("\n");
  }

  if ((args.CSVOutput != 0U) && (csv_file != NULL)) {
    (void)fclose(csv_file);
    csv_file = NULL;
  }

  return 0;
}
