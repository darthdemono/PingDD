/**
 * @file csv.c
 * @brief CSV logging implementation.
 *
 * Implements the functions declared in csv.h.
 */

#include "csv.h"

#include <ctype.h>
#include <time.h>

/**
 * @brief Copy a destination into a filename-safe slug.
 *
 * Keeps alphanumerics, '.', '-' and '_'; replaces every other character
 * (including '/', ':' from IPv6, and path separators) with '_'.
 */
static void SanitizeForFilename(pcc_t const src, char *const dst,
                                size_t const dst_size) {
  size_t i = 0;

  if (dst_size == 0U) {
    return;
  }

  for (i = 0; (src[i] != '\0') && (i < (dst_size - 1U)); i++) {
    unsigned char c = (unsigned char)src[i];
    if ((isalnum(c) != 0) || (c == '.') || (c == '-') || (c == '_')) {
      dst[i] = (char)c;
    } else {
      dst[i] = '_';
    }
  }
  dst[i] = '\0';
}

char *GenerateCSVFilename(const arguments_t *const args) {
  static char filename[256U] = {0};
  char dest[128U] = {0};
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);

  if ((args == NULL) || (args->Destination == NULL)) {
    (void)strncpy(dest, "unknown", sizeof(dest) - 1U);
  } else {
    SanitizeForFilename(args->Destination, dest, sizeof(dest));
  }

  (void)snprintf(filename, sizeof(filename),
                 "PingDD-%s-%04d%02d%02d_%02d%02d%02d.csv", dest,
                 tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
                 tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);

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
                    const char *datetime, const char *ip, const char *proto) {
  const char *ip_field = NULL;

  if ((file == NULL) || (host == NULL) || (datetime == NULL)) {
    return -1;
  }

  /* Log the address actually probed (fail-over may pick a non-primary one);
   * fall back to the primary if the caller did not provide it. */
  ip_field = ((ip != NULL) && (ip[0] != '\0')) ? ip : host->IPAddress;

  (void)fprintf(file, "\"%s\",\"%s\",\"%s\",\"%s\",%u,%.4f\n", datetime,
                host->Hostname, ip_field, (proto != NULL) ? proto : "TCP",
                (unsigned int)host->Port, rtt * 1000.0);

  (void)fflush(file);

  return 0;
}
