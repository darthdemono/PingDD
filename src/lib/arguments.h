/**
 * @file arguments.h
 * @brief Command-line argument types and function declarations.
 *
 * This header defines the @ref arguments_t struct and declares functions for:
 * parsing command-line arguments, printing help/banner messages, and writing
 * CSV output.
 */

#ifndef ARGUMENTS_H
#define ARGUMENTS_H

#include "standard.h"

/**
 * @struct arguments_t
 * @brief Stores all parsed command-line options.
 */
typedef struct {
  /** @brief TCP port number to connect to. */
  uint16_t Port;

  /** @brief Timeout in milliseconds. */
  uint32_t Timeout;

  /**
   * @brief Number of attempts to run.
   * @note A value of -1 means infinite attempts.
   */
  int32_t Count;

  /** @brief When true, the program keeps running repeatedly. */
  bool Continuous;

  /** @brief When true, colored output is used (if supported). */
  bool UseColor;

  /** @brief When true, CSV output is enabled. */
  bool CSVOutput;

  /** @brief Destination hostname or IP address string. */
  pcc_t Destination;

  /** @brief Protocol type (typically TCP). */
  int32_t Type;

  /** @brief Delay between attempts in milliseconds (rate control). */
  uint32_t Rate;
} arguments_t;

/**
 * @brief Print the program banner.
 * @details Prints the application name and version banner to the terminal.
 */
void PrintBanner(void);

/**
 * @brief Print usage/help text.
 * @details Prints the command syntax and available options.
 */
void PrintUsage(void);

/**
 * @brief Parse command-line arguments into an @ref arguments_t struct.
 *
 * @param[in]  argc      Number of command-line arguments.
 * @param[in]  argv      Array of argument strings.
 * @param[out] arguments Output struct that receives parsed values.
 *
 * @retval SUCCESS             Parsing succeeded and required fields are
 * present.
 * @retval PINGDD_INVALID_ARGS Parsing failed or required arguments are missing.
 *
 * @note If `-?` or `--help` is provided, the function prints usage and exits.
 */
int32_t ProcessArguments(int32_t const argc, char *const *const argv,
                         arguments_t *const arguments);

/**
 * @brief Print an error message.
 *
 * @param[in] message Error message string to print.
 * @note If `message` is NULL, nothing is printed.
 */
void PrintError(pcc_t const message);

/**
 * @brief Generate a CSV filename.
 *
 * @param[in] args Parsed arguments (destination may be used in the filename).
 *
 * @return Pointer to an internal static buffer containing the filename.
 *
 * @warning The returned buffer is static and will be overwritten on the next
 * call.
 */
char *GenerateCSVFilename(const arguments_t *const args);

/**
 * @brief Write the CSV header row.
 *
 * @param[in,out] file Open file handle for writing CSV.
 * @param[in]     host Host information used for context.
 *
 * @retval 0  Success.
 * @retval -1 Error (invalid input).
 */
int32_t WriteCSVHeader(FILE *file, const host_t *const host);

/**
 * @brief Write one CSV data row.
 *
 * @param[in,out] file     Open file handle for writing CSV.
 * @param[in]     host     Host information for the row.
 * @param[in]     rtt      Round-trip time in seconds.
 * @param[in]     datetime Timestamp string for the row.
 *
 * @retval 0  Success.
 * @retval -1 Error (invalid input).
 */
int32_t WriteCSVRow(FILE *file, const host_t *const host, double rtt,
                    const char *datetime);

#endif /* ARGUMENTS_H */
