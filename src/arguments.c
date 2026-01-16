/**
 * @file arguments.c
 * @brief Command-line argument parsing implementation.
 *
 * Implements the functions declared in arguments.h.
 */

#include "arguments.h"
#include "print.h"
#include "version.h"

void PrintBanner(void) {
  char banner[512U] = {0};

  (void)snprintf(banner, sizeof(banner), "%s v%s - Copyright (c) %s\n", NAME,
                 PINGDD_VERSION_FULL, AUTHOR);

  FormattedPrint(PRINT_BLUE, banner);
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
      if ((i + 1) < argc) {
        arguments->Port = (uint16_t)atoi(argv[++i]);
      } else {
        PrintError("Error: -p/--port requires port number");
        return PINGDD_INVALID_ARGS;
      }
    }
    /* Timeout */
    else if ((strcmp(arg, "-t") == 0) || (strcmp(arg, "--timeout") == 0)) {
      if ((i + 1) < argc) {
        arguments->Timeout = (uint32_t)atoi(argv[++i]);
      } else {
        PrintError("Error: -t/--timeout requires value");
        return PINGDD_INVALID_ARGS;
      }
    }
    /* Count */
    else if ((strcmp(arg, "-c") == 0) || (strcmp(arg, "--count") == 0)) {
      if ((i + 1) < argc) {
        arguments->Count = atoi(argv[++i]);
      } else {
        PrintError("Error: -c/--count requires value");
        return PINGDD_INVALID_ARGS;
      }
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
      if (i + 1 >= argc) {
        PrintError("Error: --rate requires value");
        return PINGDD_INVALID_ARGS;
      }
      arguments->Rate = (uint32_t)atoi(argv[++i]);
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
