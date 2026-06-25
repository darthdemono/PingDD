/**
 * @file csv.h
 * @brief CSV file output functions.
 *
 * Declares the functions used to create a CSV filename and to write CSV
 * header/data rows.
 */
#ifndef PINGDD_CSV_H
#define PINGDD_CSV_H

#include <stdio.h>

#include "arguments.h"
#include "standard.h"

/**
 * @brief Build a filename for a new CSV log file.
 *
 * @param[in] args Parsed arguments (destination may be used in the filename).
 * @return Pointer to an internal static buffer containing the filename.
 *
 * @warning The returned buffer is static and will be overwritten on the next
 * call.
 */
char *GenerateCSVFilename(const arguments_t *const args);

/**
 * @brief Write the CSV column header line.
 *
 * @param[in,out] file Open CSV file handle.
 * @param[in]     host Target host information.
 * @retval 0  Success.
 * @retval -1 Error (invalid input).
 */
int32_t WriteCSVHeader(FILE *const file, const host_t *const host);

/**
 * @brief Write one CSV record line.
 *
 * @param[in,out] file     Open CSV file handle.
 * @param[in]     host     Target host information.
 * @param[in]     rtt      Connection time in seconds.
 * @param[in]     datetime ISO 8601 timestamp string.
 * @retval 0  Success.
 * @retval -1 Error (invalid input).
 */
int32_t WriteCSVRow(FILE *const file, const host_t *const host,
                    const double rtt, const char *datetime, const char *ip,
                    const char *proto);

#endif /* PINGDD_CSV_H */
