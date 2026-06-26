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

void PrintVersion(void) {
  char buf[128U] = {0};
  (void)snprintf(buf, sizeof(buf), "%s %s\n", NAME, PINGDD_VERSION_FULL);
  FormattedPrint(PRINT_BLUE, buf);
  ResetColor();
}

void PrintUsage(void) {
  FormattedPrint(
      PRINT_YELLOW,
      "PingDD is a cross-platform TCP/UDP/ICMP reachability & diagnostics "
      "tool.\n"
      "Syntax: pingdd [options] host [host ...]\n"
      "Ports may be lists/ranges (-p 80,443,8000-8010); protocols a list "
      "(-P TCP,UDP).\n"
      "\n"
      "Options:\n"
      "  -p, --port N       set port N (required for TCP/UDP)\n"
      "  -t, --timeout N    per-probe timeout in milliseconds (default 1000)\n"
      "  -c, --count N      number of checks (default infinite)\n"
      "  -r, --rate N       delay between checks in ms (default 50)\n"
      "  -w, --deadline N   stop after N milliseconds total (default none)\n"
      "  -P, --protocol P   protocol list: TCP (default), UDP, ICMP "
      "(comma-separated)\n"
      "  -I, --interface X  bind probes to a source IP or interface name\n"
      "  --target SPEC      add a target host:port/proto (repeatable)\n"
      "  --targets FILE     read targets from a file (one per line)\n"
      "  --concurrent       probe all targets in parallel each cycle\n"
      "  -q, --quiet        suppress per-probe output, show summary only\n"
      "  -a, --audible      ring the terminal bell on each success\n"
      "  --json             emit machine-readable JSON (implies --no-color)\n"
      "  --json-file        log NDJSON to an auto-named .json file\n"
      "  --csv              enable CSV logging\n"
      "  --color            force colored output\n"
      "  --no-color         disable color output\n"
      "  -V, --version      display version\n"
      "  -?, --help         display this help\n"
      "\n"
      "Monitoring & testing:\n"
      "  --monitor          continuous availability monitoring + alerts\n"
      "  --load-test        sustained concurrent load\n"
      "  --resilience       ramp concurrency and report the degradation "
      "point\n"
      "  --concurrency N    concurrent workers for load/resilience (1..256, "
      "default 10)\n"
      "  --duration N       run duration in seconds for load/resilience "
      "(1..3600)\n");

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
  arguments->JsonFile = false;
  arguments->ForceColor = false;
  arguments->Monitor = false;
  arguments->LoadTest = false;
  arguments->Resilience = false;
  arguments->Concurrency = 10U;
  arguments->DurationMs = 0U;
  arguments->HostCount = 0U;
  arguments->PortSpec = NULL;
  arguments->ProtoSpec = NULL;
  arguments->TargetSpecCount = 0U;
  arguments->TargetsFile = NULL;
  arguments->Interface = NULL;
  arguments->Concurrent = false;

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
    /* Port(s): single, comma list, or range — validated during target build. */
    else if ((strcmp(arg, "-p") == 0) || (strcmp(arg, "--port") == 0)) {
      if ((i + 1) >= argc) {
        PrintError("Error: -p/--port requires a port, list, or range");
        return PINGDD_INVALID_ARGS;
      }
      arguments->PortSpec = argv[++i];
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
    /* Protocol(s): one or a comma list; validated during target build. */
    else if ((strcmp(arg, "-P") == 0) || (strcmp(arg, "--protocol") == 0)) {
      if ((i + 1) >= argc) {
        PrintError("Error: -P/--protocol requires a value (TCP, UDP, or ICMP)");
        return PINGDD_INVALID_ARGS;
      }
      arguments->ProtoSpec = argv[++i];
    }
    /* Repeated explicit target spec: host:port/proto */
    else if (strcmp(arg, "--target") == 0) {
      if ((i + 1) >= argc) {
        PrintError("Error: --target requires a spec (host:port/proto)");
        return PINGDD_INVALID_ARGS;
      }
      if (arguments->TargetSpecCount >= MAX_TARGET_SPECS) {
        PrintError("Error: too many --target specs");
        return PINGDD_INVALID_ARGS;
      }
      arguments->TargetSpecs[arguments->TargetSpecCount++] = argv[++i];
    }
    /* Targets file (one per line). */
    else if (strcmp(arg, "--targets") == 0) {
      if ((i + 1) >= argc) {
        PrintError("Error: --targets requires a file path");
        return PINGDD_INVALID_ARGS;
      }
      arguments->TargetsFile = argv[++i];
    }
    /* Source interface (IP or name). */
    else if ((strcmp(arg, "-I") == 0) || (strcmp(arg, "--interface") == 0)) {
      if ((i + 1) >= argc) {
        PrintError("Error: -I/--interface requires an IP or interface name");
        return PINGDD_INVALID_ARGS;
      }
      arguments->Interface = argv[++i];
    }
    /* Concurrent scheduling across targets. */
    else if (strcmp(arg, "--concurrent") == 0) {
      arguments->Concurrent = true;
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
    /* JSON logging to a file */
    else if (strcmp(arg, "--json-file") == 0) {
      arguments->JsonFile = true;
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
    /* Positional host (one or more). */
    else if (arguments->HostCount < MAX_HOSTS) {
      if (arguments->Destination == NULL) {
        arguments->Destination = arg; /* first host, for CSV naming etc. */
      }
      arguments->Hosts[arguments->HostCount++] = arg;
    } else {
      PrintError("Error: too many hosts");
      return PINGDD_INVALID_ARGS;
    }
  }

  /* A destination is required in some form. Port/protocol validity and the
   * "TCP/UDP need a port" rule are enforced per target in Targets_Build. */
  if ((arguments->HostCount == 0U) && (arguments->TargetSpecCount == 0U) &&
      (arguments->TargetsFile == NULL)) {
    PrintError("Error: Missing destination (host, --target, or --targets)");
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
