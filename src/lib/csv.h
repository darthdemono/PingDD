/**
 * @file csv.h
 * @brief CSV logging interface for PingDD
 * @author DarthDemono
 */
#ifndef CSVH
#define CSVH

#include "standard.h"
#include "host.h"
#include "arguments.h"

/**
 * @brief Generate CSV filename: "PingDD-{host}-{datetime}.csv"
 * @param args Command line arguments containing destination host
 * @return Static buffer containing filename (never NULL)
 */
char *GenerateCSVFilename(const arguments_t *const args);

/**
 * @brief Write CSV header row
 * @param file Open CSV file handle
 * @param host Target host information
 * @return 0 on success, -1 on error
 */
int32_t WriteCSVHeader(FILE *const file, const host_t *const host);

/**
 * @brief Write single CSV data row
 * @param file Open CSV file handle
 * @param host Target host information
 * @param rtt Connection time in seconds
 * @param datetime ISO 8601 timestamp string
 * @return 0 on success, -1 on error
 */
int32_t WriteCSVRow(FILE *const file,
                    const host_t *const host,
                    const double rtt,
                    const char *datetime);

#endif /* CSVH */
