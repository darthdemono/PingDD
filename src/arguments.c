/**
 * @file arguments.c
 * @brief Command-line argument parsing implementation.
 *
 * Implements the functions declared in arguments.h.
 */

#include "arguments.h"
#include "print.h"
#include "version.h"

#include <ctype.h>
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

/** @brief Case-insensitive ASCII string equality. */
static bool EqualsIgnoreCase(pcc_t a, pcc_t b) {
  if ((a == NULL) || (b == NULL)) {
    return false;
  }
  for (; (*a != '\0') && (*b != '\0'); a++, b++) {
    int ca = tolower((unsigned char)*a);
    int cb = tolower((unsigned char)*b);
    if (ca != cb) {
      return false;
    }
  }
  return (*a == '\0') && (*b == '\0');
}

void PrintVersion(void) {
  char buf[128U] = {0};
  (void)snprintf(buf, sizeof(buf), "%s %s\n", NAME, PINGDD_VERSION_FULL);
  FormattedPrint(PRINT_BLUE, buf);
  ResetColor();
}

void PrintUsage(void) {
  FormattedPrint(
      PRINT_YELLOW,
      "PingDD is a cross-platform TCP/UDP port reachability tool.\n"
      "Syntax: pingdd [options] destination\n"
      "\n"
      "Options:\n"
      "  -p, --port N       set port N (required for TCP/UDP)\n"
      "  -t, --timeout N    per-probe timeout in milliseconds (default 1000)\n"
      "  -c, --count N      number of checks (default infinite)\n"
      "  -r, --rate N       delay between checks in ms (default 50)\n"
      "  -w, --deadline N   stop after N milliseconds total (default none)\n"
      "  -P, --protocol P   probe protocol: TCP (default), UDP, or ICMP\n"
      "                     (ICMP is classic ping and may need privilege)\n"
      "  -q, --quiet        suppress per-probe output, show summary only\n"
      "  -a, --audible      ring the terminal bell on each success\n"
      "  --json             emit machine-readable JSON (implies --no-color)\n"
      "  --csv              enable CSV logging\n"
      "  --color            force colored output\n"
      "  --no-color         disable color output\n"
      "  -V, --version      display version\n"
      "  -?, --help         display this help\n"
      "\n"
      "Monitoring & authorized testing:\n"
      "  --monitor          continuous availability monitoring + alerts\n"
      "  --load-test        sustained concurrent load (authorized targets "
      "only)\n"
      "  --resilience       ramp concurrency and report the degradation "
      "point\n"
      "  --concurrency N    concurrent workers for load/resilience (1..256, "
      "default 10)\n"
      "  --duration N       run duration in seconds for load/resilience "
      "(1..3600)\n"
      "  --authorize        confirm you are authorized to load-test the "
      "target\n"
      "  --allow-public     permit load testing a non-private target\n");

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
  arguments->Deadline = 0U;
  arguments->Quiet = false;
  arguments->Audible = false;
  arguments->Json = false;
  arguments->ForceColor = false;
  arguments->Monitor = false;
  arguments->LoadTest = false;
  arguments->Resilience = false;
  arguments->Authorize = false;
  arguments->AllowPublic = false;
  arguments->Concurrency = 10U;
  arguments->DurationMs = 0U;

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
    /* Version */
    else if ((strcmp(arg, "-V") == 0) || (strcmp(arg, "--version") == 0)) {
      PrintVersion();
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
    /* Deadline */
    else if ((strcmp(arg, "-w") == 0) || (strcmp(arg, "--deadline") == 0)) {
      long value = 0;
      if ((i + 1) >= argc) {
        PrintError("Error: -w/--deadline requires value");
        return PINGDD_INVALID_ARGS;
      }
      if (!ParseLong(argv[++i], &value) || (value < 1) || (value > 86400000)) {
        PrintError("Error: -w/--deadline must be an integer in 1..86400000 ms");
        return PINGDD_INVALID_ARGS;
      }
      arguments->Deadline = (uint32_t)value;
    }
    /* Protocol selection (mutually exclusive) */
    else if ((strcmp(arg, "-P") == 0) || (strcmp(arg, "--protocol") == 0)) {
      pcc_t value = NULL;
      if ((i + 1) >= argc) {
        PrintError("Error: -P/--protocol requires a value (TCP, UDP, or ICMP)");
        return PINGDD_INVALID_ARGS;
      }
      value = argv[++i];
      if (EqualsIgnoreCase(value, "TCP")) {
        arguments->Type = IPPROTO_TCP;
      } else if (EqualsIgnoreCase(value, "UDP")) {
        arguments->Type = IPPROTO_UDP;
      } else if (EqualsIgnoreCase(value, "ICMP")) {
        arguments->Type = IPPROTO_ICMP;
      } else {
        PrintError("Error: -P/--protocol must be one of TCP, UDP, or ICMP");
        return PINGDD_INVALID_ARGS;
      }
    }
    /* Quiet */
    else if ((strcmp(arg, "-q") == 0) || (strcmp(arg, "--quiet") == 0)) {
      arguments->Quiet = true;
    }
    /* Audible */
    else if ((strcmp(arg, "-a") == 0) || (strcmp(arg, "--audible") == 0)) {
      arguments->Audible = true;
    }
    /* JSON output */
    else if (strcmp(arg, "--json") == 0) {
      arguments->Json = true;
      arguments->UseColor = false;
    }
    /* Force color */
    else if (strcmp(arg, "--color") == 0) {
      arguments->ForceColor = true;
    }
    /* Availability monitoring mode */
    else if (strcmp(arg, "--monitor") == 0) {
      arguments->Monitor = true;
    }
    /* Authorized load-test mode */
    else if (strcmp(arg, "--load-test") == 0) {
      arguments->LoadTest = true;
    }
    /* Resilience sweep mode */
    else if (strcmp(arg, "--resilience") == 0) {
      arguments->Resilience = true;
    }
    /* Authorization acknowledgement */
    else if (strcmp(arg, "--authorize") == 0) {
      arguments->Authorize = true;
    }
    /* Allow public targets for load testing */
    else if (strcmp(arg, "--allow-public") == 0) {
      arguments->AllowPublic = true;
    }
    /* Concurrency */
    else if (strcmp(arg, "--concurrency") == 0) {
      long value = 0;
      if ((i + 1) >= argc) {
        PrintError("Error: --concurrency requires value");
        return PINGDD_INVALID_ARGS;
      }
      if (!ParseLong(argv[++i], &value) || (value < 1) || (value > 256)) {
        PrintError("Error: --concurrency must be an integer in 1..256");
        return PINGDD_INVALID_ARGS;
      }
      arguments->Concurrency = (uint32_t)value;
    }
    /* Duration (seconds) */
    else if (strcmp(arg, "--duration") == 0) {
      long value = 0;
      if ((i + 1) >= argc) {
        PrintError("Error: --duration requires value (seconds)");
        return PINGDD_INVALID_ARGS;
      }
      if (!ParseLong(argv[++i], &value) || (value < 1) || (value > 3600)) {
        PrintError("Error: --duration must be an integer in 1..3600 seconds");
        return PINGDD_INVALID_ARGS;
      }
      arguments->DurationMs = (uint32_t)value * 1000U;
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
    /* Unknown option: anything starting with '-' that matched nothing above. */
    else if ((arg[0] == '-') && (arg[1] != '\0')) {
      char msg[128U] = {0};
      (void)snprintf(msg, sizeof(msg), "Error: unknown option '%s'", arg);
      PrintError(msg);
      PrintUsage();
      return PINGDD_INVALID_ARGS;
    }
    /* Destination (the single non-option argument). */
    else if (arguments->Destination == NULL) {
      arguments->Destination = arg;
    } else {
      char msg[160U] = {0};
      (void)snprintf(msg, sizeof(msg),
                     "Error: multiple destinations ('%s' and '%s')",
                     arguments->Destination, arg);
      PrintError(msg);
      return PINGDD_INVALID_ARGS;
    }
  }

  /* Validate required arguments. ICMP needs no port; TCP/UDP do. */
  if (arguments->Destination == NULL) {
    PrintError("Error: Missing destination");
    PrintUsage();
    return PINGDD_INVALID_ARGS;
  }
  if ((arguments->Type != IPPROTO_ICMP) && (arguments->Port == 0U)) {
    PrintError("Error: Missing required port (-p) for TCP/UDP");
    PrintUsage();
    return PINGDD_INVALID_ARGS;
  }

  /* Modes are mutually exclusive. */
  {
    int modes = (arguments->Monitor ? 1 : 0) + (arguments->LoadTest ? 1 : 0) +
                (arguments->Resilience ? 1 : 0);
    if (modes > 1) {
      PrintError("Error: choose only one of --monitor, --load-test, "
                 "--resilience");
      return PINGDD_INVALID_ARGS;
    }
  }
  if ((arguments->LoadTest || arguments->Resilience) &&
      (arguments->DurationMs == 0U)) {
    PrintError("Error: --load-test/--resilience require --duration <seconds>");
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
