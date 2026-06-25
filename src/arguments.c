/**
 * @file arguments.c
 * @brief Command-line argument parsing implementation.
 *
 * Implements the functions declared in arguments.h.
 */

#include "arguments.h"
#include "print.h"
#include "version.h"

#include <errno.h>
#include <limits.h>

/**
 * @brief Parse a decimal string into a long, rejecting junk and overflow.
 *
 * @param[in]  text Numeric string to parse.
 * @param[out] out  Receives the parsed value on success.
 * @retval true  Parsed cleanly (whole string consumed, in long range).
 * @retval false Empty, non-numeric, trailing garbage, or out of range.
 */
static bool ParseLong(pcc_t const text, long *const out) {
  char *end = NULL;
  long value = 0;

  if ((text == NULL) || (text[0] == '\0')) {
    return false;
  }

  errno = 0;
  value = strtol(text, &end, 10);

  if ((errno != 0) || (end == text) || (*end != '\0')) {
    return false;
  }

  *out = value;
  return true;
}

void PrintUsage(void) {
  FormattedPrint(
      PRINT_YELLOW,
      "PingDD is a cross-platform ping tool for TCP port checking.\n"
      "Syntax: pingdd [options] destination\n"
      "\n"
      "Options:\n"
      "  -p, --port N       set TCP port N (required)\n"
      "  -t, --timeout N    timeout in milliseconds (default 1000)\n"
      "  -c, --count N      set number of checks to N (default infinite)\n"
      "  -r, --rate N       set rate of pings to 1 ping per N ms (default "
      "50ms)\n"
      "  --no-color         disable color output\n"
      "  --csv              enable CSV output\n"
      "  -?, --help         display this help\n");

  ResetColor();
}

int32_t ProcessArguments(int32_t const argc, char *const *const argv,
                         arguments_t *const arguments) {
  int32_t i = 0;

  /* Validate inputs */
  if ((argc < 1) || (argv == NULL) || (arguments == NULL)) {
    return PINGDD_INVALID_ARGS;
  }

  /* Initialize defaults */
  arguments->Port = DEFAULT_PORT;
  arguments->Timeout = DEFAULT_TIMEOUT;
  arguments->Count = INFINITE_COUNT;
  arguments->Continuous = true;
  arguments->UseColor = true;
  arguments->Destination = NULL;
  arguments->Type = IPPROTO_TCP;
  arguments->CSVOutput = false;
  arguments->Rate = 0U;

  /* Parse arguments */
  for (i = 1; i < argc; i++) {
    pcc_t arg = argv[i];

    if (arg == NULL) {
      continue;
    }

    /* Help */
    if ((strcmp(arg, "-?") == 0) || (strcmp(arg, "--help") == 0)) {
      PrintUsage();
      exit(0);
    }
    /* Port */
    else if ((strcmp(arg, "-p") == 0) || (strcmp(arg, "--port") == 0)) {
      long value = 0;
      if ((i + 1) >= argc) {
        PrintError("Error: -p/--port requires port number");
        return PINGDD_INVALID_ARGS;
      }
      if (!ParseLong(argv[++i], &value) || (value < 1) || (value > 65535)) {
        PrintError("Error: -p/--port must be an integer in 1..65535");
        return PINGDD_INVALID_ARGS;
      }
      arguments->Port = (uint16_t)value;
    }
    /* Timeout */
    else if ((strcmp(arg, "-t") == 0) || (strcmp(arg, "--timeout") == 0)) {
      long value = 0;
      if ((i + 1) >= argc) {
        PrintError("Error: -t/--timeout requires value");
        return PINGDD_INVALID_ARGS;
      }
      if (!ParseLong(argv[++i], &value) || (value < 1) || (value > 3600000)) {
        PrintError("Error: -t/--timeout must be an integer in 1..3600000 ms");
        return PINGDD_INVALID_ARGS;
      }
      arguments->Timeout = (uint32_t)value;
    }
    /* Count */
    else if ((strcmp(arg, "-c") == 0) || (strcmp(arg, "--count") == 0)) {
      long value = 0;
      if ((i + 1) >= argc) {
        PrintError("Error: -c/--count requires value");
        return PINGDD_INVALID_ARGS;
      }
      if (!ParseLong(argv[++i], &value) || (value < -1) || (value > INT32_MAX)) {
        PrintError("Error: -c/--count must be -1 (infinite) or >= 0");
        return PINGDD_INVALID_ARGS;
      }
      arguments->Count = (int32_t)value;
    }
    /* No color */
    else if (strcmp(arg, "--no-color") == 0) {
      arguments->UseColor = false;
    }
    /* CSV output */
    else if (strcmp(arg, "--csv") == 0) {
      arguments->CSVOutput = true;
    }
    /* Rate */
    else if ((strcmp(arg, "-r") == 0) || (strcmp(arg, "--rate") == 0)) {
      long value = 0;
      if ((i + 1) >= argc) {
        PrintError("Error: -r/--rate requires value");
        return PINGDD_INVALID_ARGS;
      }
      if (!ParseLong(argv[++i], &value) || (value < 0) || (value > 3600000)) {
        PrintError("Error: -r/--rate must be an integer in 0..3600000 ms");
        return PINGDD_INVALID_ARGS;
      }
      arguments->Rate = (uint32_t)value;
    }
    /* Destination (last non-option argument) */
    else {
      arguments->Destination = arg;
    }
  }

  /* Validate required arguments */
  if ((arguments->Port == 0U) || (arguments->Destination == NULL)) {
    PrintError("Error: Missing required arguments (port and destination)");
    PrintUsage();
    return PINGDD_INVALID_ARGS;
  }

  return SUCCESS;
}

void PrintError(pcc_t const message) {
  if (message != NULL) {
    FormattedPrint(PRINT_RED, message);
    (void)printf("\n");
  }
}
