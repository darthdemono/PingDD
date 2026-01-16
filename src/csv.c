/**
 * @file csv.c
 * @brief CSV logging implementation.
 *
 * Implements the functions declared in csv.h.
 */

#include "csv.h"

#include <time.h>

char *GenerateCSVFilename(const arguments_t *const args) {
  static char filename[256U] = {0};
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);

  if ((args == NULL) || (args->Destination == NULL)) {
    (void)snprintf(filename, sizeof(filename),
                   "PingDD-unknown-%04d%02d%02d_%02d%02d%02d.csv",
                   tm_info->tm_year + 1900, tm_info->tm_mon + 1,
                   tm_info->tm_mday, tm_info->tm_hour, tm_info->tm_min,
                   tm_info->tm_sec);
  } else {
    (void)snprintf(
        filename, sizeof(filename), "PingDD-%s-%04d%02d%02d_%02d%02d%02d.csv",
        args->Destination, tm_info->tm_year + 1900, tm_info->tm_mon + 1,
        tm_info->tm_mday, tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
  }

  return filename;
}

int32_t WriteCSVHeader(FILE *file, const host_t *const host) {
  if ((file == NULL) || (host == NULL)) {
    return -1;
  }

  (void)fprintf(file, "DateTime,Host,IPAddress,Protocol,Port,Time_ms\n");
  (void)fflush(file);

  return 0;
}

int32_t WriteCSVRow(FILE *file, const host_t *const host, double rtt,
                    const char *datetime) {
  if ((file == NULL) || (host == NULL) || (datetime == NULL)) {
    return -1;
  }

  (void)fprintf(file, "\"%s\",\"%s\",\"%s\",\"TCP\",%u,%.4f\n", datetime,
                host->Hostname, host->IPAddress, (unsigned int)host->Port,
                rtt * 1000.0);

  (void)fflush(file);

  return 0;
}
