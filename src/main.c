/**
 * @file main.c
 * @brief PingDD application entry point.
 *
 * Contains the program main loop, signal handling, timestamp formatting,
 * optional CSV/JSON logging, and final statistics printing.
 */

#include "arguments.h"
#include "csv.h"
#include "diag.h"
#include "loadtest.h"
#include "print.h"
#include "socket.h"
#include "standard.h"
#include "stats.h"
#include "timer.h"
#include "version.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

/**
 * @brief Global interrupt flag set by the SIGINT/SIGTERM handler.
 *
 * Non-static so blocking wait loops in socket.c can abort cleanly.
 */
volatile sig_atomic_t g_interrupted = 0;

/** @brief Global CSV output file handle. */
static FILE *csv_file = NULL;

/** @brief Return non-zero when standard output is an interactive terminal. */
static int StdoutIsTty(void) {
#ifdef _WIN32
  return _isatty(_fileno(stdout));
#else
  return isatty(fileno(stdout));
#endif
}

/**
 * @brief Human/JSON protocol label for a probe.
 *
 * @param type Protocol selector (IPPROTO_TCP/UDP/ICMP).
 * @param ip   Address text, used to distinguish ICMP from ICMPv6.
 */
static pcc_t ProtoLabel(int32_t const type, pcc_t const ip) {
  if (type == IPPROTO_ICMP) {
    return ((ip != NULL) && (strchr(ip, ':') != NULL)) ? "ICMPv6" : "ICMP";
  }
  return (type == IPPROTO_UDP) ? "UDP" : "TCP";
}

/** @brief Short machine-readable status token for a probe result code. */
static pcc_t StatusToken(int32_t const result) {
  switch (result) {
  case SUCCESS:
    return "success";
  case PINGDD_SOCKET_CLOSED:
    return "refused";
  case PINGDD_SOCKET_TIMEOUT:
    return "timeout";
  case PINGDD_SOCKET_UNREACH:
    return "unreachable";
  case PINGDD_UDP_OPENFILTERED:
    return "open|filtered";
  case PINGDD_SOCKET_RESOLVE:
    return "resolve-failed";
  default:
    return "error";
  }
}

/**
 * @brief Build an ISO 8601 timestamp string with timezone offset.
 *
 * @param[out] buf      Output buffer.
 * @param[in]  buf_size Size of @p buf in bytes.
 */
static inline void GetTimestampString(char *const buf, const size_t buf_size) {
  time_t now = time(NULL);
  struct tm tm_local;

#ifdef _WIN32
  /* localtime() uses thread-local storage on Windows; copy out immediately. */
  tm_local = *localtime(&now);

  DYNAMIC_TIME_ZONE_INFORMATION tz_info;
  DWORD result = GetDynamicTimeZoneInformation(&tz_info);

  LONG bias_minutes = (result == TIME_ZONE_ID_DAYLIGHT)
                          ? tz_info.Bias + tz_info.DaylightBias
                          : tz_info.Bias;

  int timezone_offset_minutes = -bias_minutes;
#else
  extern long timezone;
  extern int daylight;

  (void)localtime_r(&now, &tm_local);
  tzset(); /* ensure timezone/daylight globals are populated */

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
                 tm_local.tm_year + 1900, tm_local.tm_mon + 1, tm_local.tm_mday,
                 tm_local.tm_hour, tm_local.tm_min, tm_local.tm_sec, sign,
                 abs_hours, timezone_mins);
}

/** @brief Sleep for a number of milliseconds. */
static inline void delay_ms(const uint32_t ms) {
#ifdef _WIN32
  Sleep(ms);
#else
  /* sleep() takes seconds; ms/1000 truncates sub-second rates to 0. Use
   * nanosleep for true millisecond resolution. */
  struct timespec ts;
  ts.tv_sec = (time_t)(ms / 1000U);
  ts.tv_nsec = (long)(ms % 1000U) * 1000000L;
  (void)nanosleep(&ts, NULL);
#endif
}

/** @brief SIGINT/SIGTERM handler: request a clean shutdown. */
static void SignalHandler(int signal) {
  (void)signal;
  g_interrupted = 1;
}

/** @brief Read elapsed milliseconds without consuming the timer origin. */
static double ElapsedMs(pingdd_timer_t *const timer) {
  double seconds = Timer_Stop(timer);
  timer->hasValue = true; /* keep the origin so it can be read again */
  return seconds * 1000.0;
}

/** @brief Copy @p src into @p dst, escaping characters unsafe in JSON. */
static void JsonEscape(pcc_t const src, char *const dst, size_t const dst_size) {
  size_t out = 0;
  size_t in = 0;

  if ((dst == NULL) || (dst_size == 0U)) {
    return;
  }
  if (src == NULL) {
    dst[0] = '\0';
    return;
  }

  for (in = 0; (src[in] != '\0') && (out + 2U < dst_size); in++) {
    char c = src[in];
    if ((c == '"') || (c == '\\')) {
      dst[out++] = '\\';
      dst[out++] = c;
    } else if ((unsigned char)c < 0x20U) {
      dst[out++] = ' '; /* drop control chars */
    } else {
      dst[out++] = c;
    }
  }
  dst[out] = '\0';
}

/** @brief Print one human-readable probe result line. */
static void PrintProbeHuman(int32_t result, unsigned long seq, pcc_t ip,
                            double rtt, pcc_t proto, bool show_port,
                            uint16_t port, pcc_t datetime) {
  char buf[96U] = {0};

  if (result == SUCCESS) {
    FormattedPrint(PRINT_WHITE, "Connected to  ");
    FormattedPrint(PRINT_GREEN, ip);
    FormattedPrint(PRINT_WHITE, ": seq=");
    (void)snprintf(buf, sizeof(buf), "%lu", seq);
    FormattedPrint(PRINT_GREEN, buf);
    FormattedPrint(PRINT_WHITE, " time=");
    (void)snprintf(buf, sizeof(buf), "%.4fms", rtt * 1000.0);
    FormattedPrint(PRINT_GREEN, buf);
    FormattedPrint(PRINT_WHITE, " protocol=");
    FormattedPrint(PRINT_GREEN, proto);
    if (show_port) {
      FormattedPrint(PRINT_WHITE, " port=");
      (void)snprintf(buf, sizeof(buf), "%u", (unsigned)port);
      FormattedPrint(PRINT_GREEN, buf);
    }
    FormattedPrint(PRINT_WHITE, " datetime=");
    FormattedPrint(PRINT_GREEN, datetime);
    (void)printf("\n");
  } else {
    FormattedPrint(PRINT_WHITE, "seq=");
    (void)snprintf(buf, sizeof(buf), "%lu", seq);
    FormattedPrint(PRINT_GREEN, buf);
    (void)printf(" ");
    FormattedPrint(PRINT_RED, GetFriendlyTypeName(result));
    (void)snprintf(buf, sizeof(buf), " (%s)", ip);
    FormattedPrint(PRINT_WHITE, buf);
    (void)printf("\n");
  }
}

/** @brief Print one probe result as a compact JSON object. */
static void PrintProbeJson(int32_t result, unsigned long seq, pcc_t ip,
                           double rtt, pcc_t proto, bool show_port,
                           uint16_t port, pcc_t datetime) {
  char ip_esc[80U] = {0};
  JsonEscape(ip, ip_esc, sizeof(ip_esc));

  (void)printf("{\"seq\":%lu,\"timestamp\":\"%s\",\"ip\":\"%s\",\"proto\":\"%s\""
               ",",
               seq, datetime, ip_esc, proto);
  if (show_port) {
    (void)printf("\"port\":%u,", (unsigned)port);
  } else {
    (void)printf("\"port\":null,");
  }
  (void)printf("\"status\":\"%s\",", StatusToken(result));
  if (result == SUCCESS) {
    (void)printf("\"rtt_ms\":%.4f}\n", rtt * 1000.0);
  } else {
    (void)printf("\"rtt_ms\":null}\n");
  }
}

/** @brief Print the final human-readable statistics block (classic layout). */
static void PrintSummaryHuman(const stats_t *const stats) {
  char buf[128U] = {0};
  double fail_percent = 0.0;

  if (stats->Attempts > 0U) {
    fail_percent = ((double)stats->Failures / (double)stats->Attempts) * 100.0;
  }

  FormattedPrint(PRINT_YELLOW, "\nConnection statistics:\n");
  ResetColor();

  (void)printf("        Attempted = ");
  (void)snprintf(buf, sizeof(buf), "%lu", (unsigned long)stats->Attempts);
  FormattedPrint(PRINT_BLUE, buf);
  (void)printf(" , Connected = ");
  (void)snprintf(buf, sizeof(buf), "%lu", (unsigned long)stats->Connects);
  FormattedPrint(PRINT_BLUE, buf);
  (void)printf(" , Failed = ");
  (void)snprintf(buf, sizeof(buf), "%lu", (unsigned long)stats->Failures);
  FormattedPrint(PRINT_BLUE, buf);
  (void)printf(" ( ");
  (void)snprintf(buf, sizeof(buf), "%.2f%%", fail_percent);
  FormattedPrint(PRINT_BLUE, buf);
  (void)printf(" )\n");

  FormattedPrint(PRINT_YELLOW, "Approximate connection times:\n");
  ResetColor();

  if (stats->Connects == 0U) {
    FormattedPrint(PRINT_BLUE, "        no successful connections\n");
    return;
  }

  (void)printf("        Minimum = ");
  (void)snprintf(buf, sizeof(buf), "%.4fms", stats->Minimum * 1000.0);
  FormattedPrint(PRINT_BLUE, buf);
  (void)printf(" , Maximum = ");
  (void)snprintf(buf, sizeof(buf), "%.4fms", stats->Maximum * 1000.0);
  FormattedPrint(PRINT_BLUE, buf);
  (void)printf(" , Average = ");
  (void)snprintf(buf, sizeof(buf), "%.5fms", Stats_Average(stats) * 1000.0);
  FormattedPrint(PRINT_BLUE, buf);
  (void)printf(" , StdDev = ");
  (void)snprintf(buf, sizeof(buf), "%.5fms", Stats_StdDev(stats) * 1000.0);
  FormattedPrint(PRINT_BLUE, buf);
  (void)printf("\n");

  (void)printf("        Percentiles p50/p95/p99 = ");
  (void)snprintf(buf, sizeof(buf), "%.4f / %.4f / %.4f ms",
                 Stats_Percentile(stats, 50.0) * 1000.0,
                 Stats_Percentile(stats, 95.0) * 1000.0,
                 Stats_Percentile(stats, 99.0) * 1000.0);
  FormattedPrint(PRINT_BLUE, buf);
  (void)printf("\n");
}

/** @brief Print the final statistics block as a JSON summary object. */
static void PrintSummaryJson(const stats_t *const stats, pcc_t host) {
  char host_esc[280U] = {0};
  double fail_percent = 0.0;

  if (stats->Attempts > 0U) {
    fail_percent = ((double)stats->Failures / (double)stats->Attempts) * 100.0;
  }
  JsonEscape(host, host_esc, sizeof(host_esc));

  (void)printf("{\"summary\":{\"host\":\"%s\",\"sent\":%lu,\"received\":%lu,"
               "\"lost\":%lu,\"loss_pct\":%.2f,",
               host_esc, (unsigned long)stats->Attempts,
               (unsigned long)stats->Connects, (unsigned long)stats->Failures,
               fail_percent);

  if (stats->Connects == 0U) {
    (void)printf("\"min_ms\":null,\"avg_ms\":null,\"max_ms\":null,"
                 "\"mdev_ms\":null,\"p50_ms\":null,\"p95_ms\":null,"
                 "\"p99_ms\":null}}\n");
    return;
  }

  (void)printf(
      "\"min_ms\":%.4f,\"avg_ms\":%.4f,\"max_ms\":%.4f,\"mdev_ms\":%.4f,"
      "\"p50_ms\":%.4f,\"p95_ms\":%.4f,\"p99_ms\":%.4f}}\n",
      stats->Minimum * 1000.0, Stats_Average(stats) * 1000.0,
      stats->Maximum * 1000.0, Stats_StdDev(stats) * 1000.0,
      Stats_Percentile(stats, 50.0) * 1000.0,
      Stats_Percentile(stats, 95.0) * 1000.0,
      Stats_Percentile(stats, 99.0) * 1000.0);
}

/**
 * @brief Run an authorized load-test or resilience sweep, enforcing the
 * authorization and public-target guardrails first.
 *
 * @return EXIT_SUCCESS on completion, EXIT_FAILURE if refused or it fails.
 */
static int RunLoadTestMode(const arguments_t *const args, host_t *const host) {
  loadtest_cfg_t cfg;
  char line[256] = {0};
  pcc_t proto = ProtoLabel(host->Type, host->IPAddress);
  bool is_icmp = (host->Type == IPPROTO_ICMP);

  /* Guardrail 1: explicit authorization acknowledgement. */
  if (!args->Authorize) {
    PrintError("Refusing: load/resilience testing requires --authorize. "
               "Only run this against systems you own or have written "
               "permission to test.");
    return EXIT_FAILURE;
  }
  /* Guardrail 2: public targets require an extra explicit opt-in. */
  if (!IsPrivateAddress((const struct sockaddr *)&host->Addrs[0]) &&
      !args->AllowPublic) {
    (void)snprintf(line, sizeof(line),
                   "Refusing: %s is a public address. Add --allow-public "
                   "only if you are authorized to test it.",
                   host->IPAddress);
    PrintError(line);
    return EXIT_FAILURE;
  }

  (void)snprintf(line, sizeof(line), "%s v%s\n", NAME, PINGDD_VERSION_FULL);
  FormattedPrint(PRINT_BLUE, line);
  FormattedPrint(PRINT_RED, args->Resilience ? "AUTHORIZED RESILIENCE TEST"
                                             : "AUTHORIZED LOAD TEST");
  FormattedPrint(PRINT_YELLOW, " against ");
  FormattedPrint(PRINT_GREEN, host->Hostname);
  if (is_icmp) {
    (void)snprintf(line, sizeof(line), " (%s) %s", host->IPAddress, proto);
  } else {
    (void)snprintf(line, sizeof(line), " (%s) port %u %s", host->IPAddress,
                   (unsigned)host->Port, proto);
  }
  FormattedPrint(PRINT_YELLOW, line);
  (void)snprintf(line, sizeof(line), "  [concurrency %u, %u s]\n",
                 args->Concurrency, args->DurationMs / 1000U);
  FormattedPrint(PRINT_YELLOW, line);
  ResetColor();
  (void)fflush(stdout);

  memset(&cfg, 0, sizeof(cfg));
  cfg.Host = host;
  cfg.TimeoutMs = args->Timeout;
  cfg.Concurrency = args->Concurrency;
  cfg.DurationMs = args->DurationMs;
  cfg.Resilience = args->Resilience;
  cfg.Silent = false;

  if (args->Resilience) {
    if (LoadTest_Resilience(&cfg) != 0) {
      PrintError("Resilience test failed to start");
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
  }

  {
    loadtest_result_t r;
    double loss = 0.0;

    if (LoadTest_Run(&cfg, &r) != 0) {
      PrintError("Load test failed to start");
      return EXIT_FAILURE;
    }
    if (r.Attempts > 0ULL) {
      loss = ((double)r.Failed / (double)r.Attempts) * 100.0;
    }

    FormattedPrint(PRINT_YELLOW, "\nLoad test result:\n");
    ResetColor();
    (void)snprintf(line, sizeof(line),
                   "        duration    = %.2f s , concurrency = %u\n",
                   r.DurationSec, r.Concurrency);
    FormattedPrint(PRINT_BLUE, line);
    (void)snprintf(line, sizeof(line),
                   "        connections = %llu ( %llu ok , %llu failed , "
                   "%.2f%% loss )\n",
                   r.Attempts, r.Success, r.Failed, loss);
    FormattedPrint(PRINT_BLUE, line);
    (void)snprintf(line, sizeof(line),
                   "        throughput  = %.0f conn/s\n", LoadTest_Rps(&r));
    FormattedPrint(PRINT_BLUE, line);
    if (r.Success > 0ULL) {
      (void)snprintf(line, sizeof(line),
                     "        latency     = %.4f / %.4f / %.4f / %.4f ms "
                     "(min/avg/max/stddev)\n",
                     r.MinRtt * 1000.0, LoadTest_AvgRtt(&r) * 1000.0,
                     r.MaxRtt * 1000.0, LoadTest_StdDevRtt(&r) * 1000.0);
      FormattedPrint(PRINT_BLUE, line);
    }
    ResetColor();
  }

  return EXIT_SUCCESS;
}

/** @brief Program entry point. */
int main(int argc, char *const *argv) {
  arguments_t args = {0};
  host_t host = {0};
  stats_t stats = {0};
  diag_t diag = {0};
  pingdd_timer_t total = {0};
  pingdd_timer_t dns_timer = {0};
  double dns_ms = 0.0;
  int32_t resolve_result = 0;
  unsigned long seq = 0U;
  int32_t i = 0;
  bool color = false;

  if (ProcessArguments(argc, argv, &args) != 0) {
    return EXIT_FAILURE;
  }

  /* Color policy: --json/--no-color force off; otherwise auto-detect a TTY and
   * honour the NO_COLOR convention unless --color forces it on. */
  color = args.UseColor;
  if (color && !args.ForceColor) {
    if ((getenv("NO_COLOR") != NULL) || (StdoutIsTty() == 0)) {
      color = false;
    }
  }
  UseColor = color;
  Print_EnableVirtualTerminal();

  (void)signal(SIGINT, SignalHandler);
#ifdef SIGTERM
  (void)signal(SIGTERM, SignalHandler);
#endif

  Stats_Init(&stats);
  Diag_Init(&diag, args.Quiet || args.Json);
  SetPortAndType(args.Port, args.Type, &host);

  /* Time DNS resolution so slow/failed name lookups can be diagnosed. */
  Timer_Start(&dns_timer);
  resolve_result = Resolve(args.Destination, &host);
  dns_ms = Timer_Stop(&dns_timer) * 1000.0;
  Diag_SetDns(&diag, dns_ms, resolve_result != SUCCESS);
  if (resolve_result != SUCCESS) {
    PrintError(GetFriendlyTypeName(resolve_result));
    return EXIT_FAILURE;
  }

  /* Authorized load testing / resilience: separate path from the probe loop. */
  if (args.LoadTest || args.Resilience) {
    int rc = RunLoadTestMode(&args, &host);
    Stats_Free(&stats);
    return rc;
  }

  if (args.CSVOutput) {
    char *csv_filename = GenerateCSVFilename(&args);
    csv_file = fopen(csv_filename, "w");
    if (csv_file == NULL) {
      PrintError("Failed to create CSV file");
      return EXIT_FAILURE;
    }
    (void)WriteCSVHeader(csv_file, &host);
    if (!args.Json) {
      FormattedPrint(PRINT_YELLOW, "CSV logging: ");
      FormattedPrint(PRINT_GREEN, csv_filename);
      FormattedPrint(PRINT_YELLOW, "\n");
    }
  }

  /* Header */
  {
    bool is_icmp = (host.Type == IPPROTO_ICMP);
    pcc_t proto = ProtoLabel(host.Type, host.IPAddress);

    if (args.Json) {
      char host_esc[280U] = {0};
      char ip_esc[80U] = {0};
      JsonEscape(host.Hostname, host_esc, sizeof(host_esc));
      JsonEscape(host.IPAddress, ip_esc, sizeof(ip_esc));
      (void)printf("{\"target\":{\"host\":\"%s\",\"ip\":\"%s\",", host_esc,
                   ip_esc);
      if (is_icmp) {
        (void)printf("\"port\":null,");
      } else {
        (void)printf("\"port\":%u,", (unsigned)host.Port);
      }
      (void)printf("\"proto\":\"%s\"}}\n", proto);
    } else {
      char line[512U] = {0};
      char ts[64U] = {0};

      /* Version banner (name/author removed). */
      (void)snprintf(line, sizeof(line), "%s v%s\n", NAME, PINGDD_VERSION_FULL);
      FormattedPrint(PRINT_BLUE, line);

      /* Classic "Connecting to <host> on <PROTO> [<port>] on <timestamp>:" */
      GetTimestampString(ts, sizeof(ts));
      FormattedPrint(PRINT_YELLOW, "Connecting to ");
      FormattedPrint(PRINT_GREEN, host.Hostname);
      FormattedPrint(PRINT_YELLOW, " on ");
      FormattedPrint(PRINT_GREEN, proto);
      if (!is_icmp) {
        FormattedPrint(PRINT_YELLOW, " ");
        (void)snprintf(line, sizeof(line), "%u", (unsigned)host.Port);
        FormattedPrint(PRINT_GREEN, line);
      }
      FormattedPrint(PRINT_YELLOW, " on ");
      FormattedPrint(PRINT_GREEN, ts);
      FormattedPrint(PRINT_YELLOW, ":\n");
      ResetColor();
    }
  }
  (void)fflush(stdout);

  Timer_Start(&total);

  while ((g_interrupted == 0) && ((args.Count == -1) || (i < args.Count))) {
    int32_t connect_result = 0;
    double rtt = 0.0;
    char ip[64U] = {0};
    char datetime[64U] = {0};

    if ((args.Deadline > 0U) && (ElapsedMs(&total) >= (double)args.Deadline)) {
      break;
    }

    connect_result = Connect(&host, args.Timeout, &rtt, ip, sizeof(ip));

    if ((g_interrupted != 0) || (connect_result == PINGDD_INTERRUPTED)) {
      break;
    }

    GetTimestampString(datetime, sizeof(datetime));

    {
      pcc_t proto = ProtoLabel(host.Type, ip);
      bool show_port = (host.Type != IPPROTO_ICMP);

      if (connect_result == SUCCESS) {
        Stats_AddSample(&stats, rtt);
        stats.Connects++;
        if (args.Audible) {
          (void)printf("\a");
        }
        if (args.CSVOutput && (csv_file != NULL)) {
          (void)WriteCSVRow(csv_file, &host, rtt, datetime, ip, proto);
        }
      } else {
        stats.Failures++;
      }
      stats.Attempts++;

      if (!args.Quiet) {
        if (args.Json) {
          PrintProbeJson(connect_result, seq, ip, rtt, proto, show_port,
                         host.Port, datetime);
        } else {
          PrintProbeHuman(connect_result, seq, ip, rtt, proto, show_port,
                          host.Port, datetime);
        }
      }

      /* Feed the diagnostics engine; it raises live alerts on real
       * transitions (outage, recovery, sustained latency) and ignores
       * isolated drops as noise. */
      Diag_Observe(&diag, connect_result == SUCCESS, rtt);
    }
    (void)fflush(stdout);

    seq++;
    i++;

    if ((args.Count != -1) && (i >= args.Count)) {
      break; /* no trailing delay after the final probe */
    }
    if ((args.Deadline > 0U) && (ElapsedMs(&total) >= (double)args.Deadline)) {
      break;
    }

    delay_ms((args.Rate > 0U) ? args.Rate : 50U);
  }

  /* Graceful Ctrl-C: terminate the in-flight line cleanly and note the cause
   * before the summary, rather than aborting mid-output. */
  if ((g_interrupted != 0) && !args.Json) {
    FormattedPrint(PRINT_YELLOW, "\n^C interrupted\n");
    ResetColor();
  }

  /* Summary + diagnostics */
  if (args.Json) {
    PrintSummaryJson(&stats, host.Hostname);
    Diag_PrintJson(&diag, &stats);
  } else {
    PrintSummaryHuman(&stats);
    Diag_PrintSummary(&diag, &stats);
    if (args.Monitor) {
      char line[96U] = {0};
      double avail = (stats.Attempts > 0U)
                         ? ((double)stats.Connects / (double)stats.Attempts) *
                               100.0
                         : 0.0;
      (void)snprintf(line, sizeof(line),
                     "        Availability   = %.3f%% over %lu probes\n", avail,
                     (unsigned long)stats.Attempts);
      FormattedPrint(PRINT_BLUE, line);
    }
  }
  (void)fflush(stdout);

  if ((args.CSVOutput != 0U) && (csv_file != NULL)) {
    (void)fclose(csv_file);
    csv_file = NULL;
  }
  Stats_Free(&stats);

  /* Exit non-zero when attempts were made but none succeeded, so scripts and
   * monitoring can detect an unreachable service (mirrors classic ping). */
  if ((stats.Attempts > 0U) && (stats.Connects == 0U)) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
