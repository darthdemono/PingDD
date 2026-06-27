/**
 * @file main.c
 * @brief PingDD application entry point.
 *
 * Contains the program main loop, signal handling, timestamp formatting,
 * optional CSV/JSON logging, and final statistics printing.
 */

#include "arguments.h"
#include "cpool.h"
#include "csv.h"
#include "diag.h"
#include "http.h"
#include "loadtest.h"
#include "print.h"
#include "resolve.h"
#include "socket.h"
#include "standard.h"
#include "stats.h"
#include "targets.h"
#include "timer.h"
#include "traceroute.h"
#include "version.h"

#include <math.h>
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

/** @brief Global NDJSON log file handle (`--json-file`). */
static FILE *json_file = NULL;

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

/**
 * @brief Format a reverse-DNS + ASN annotation for a resolved host.
 *
 * @return true when at least one annotation field was present.
 */
static bool FormatAnnotation(const host_t *const h, char *const out,
                             size_t const out_size) {
  bool has_rdns = (h->ReverseName[0] != '\0');
  bool has_asn = (h->Asn[0] != '\0');

  out[0] = '\0';
  if (!has_rdns && !has_asn) {
    return false;
  }
  if (has_rdns && has_asn) {
    if (h->AsnOrg[0] != '\0') {
      (void)snprintf(out, out_size, "%s [%s %s]", h->ReverseName, h->Asn,
                     h->AsnOrg);
    } else {
      (void)snprintf(out, out_size, "%s [%s]", h->ReverseName, h->Asn);
    }
  } else if (has_rdns) {
    (void)snprintf(out, out_size, "%s", h->ReverseName);
  } else if (h->AsnOrg[0] != '\0') {
    (void)snprintf(out, out_size, "[%s %s]", h->Asn, h->AsnOrg);
  } else {
    (void)snprintf(out, out_size, "[%s]", h->Asn);
  }
  return true;
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
                            uint16_t port, pcc_t datetime, pcc_t prefix,
                            int32_t ttl) {
  char buf[96U] = {0};

  if (prefix != NULL) {
    (void)snprintf(buf, sizeof(buf), "[%s] ", prefix);
    FormattedPrint(PRINT_WHITE, buf);
  }

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
    if (ttl >= 0) {
      FormattedPrint(PRINT_WHITE, " ttl=");
      (void)snprintf(buf, sizeof(buf), "%d", (int)ttl);
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
static void PrintProbeJson(FILE *const out, int32_t result, unsigned long seq,
                           pcc_t ip, double rtt, pcc_t proto, bool show_port,
                           uint16_t port, pcc_t datetime, pcc_t host,
                           int32_t ttl) {
  char ip_esc[80U] = {0};
  char host_esc[280U] = {0};
  JsonEscape(ip, ip_esc, sizeof(ip_esc));
  JsonEscape(host, host_esc, sizeof(host_esc));

  (void)fprintf(out,
                "{\"seq\":%lu,\"timestamp\":\"%s\",\"host\":\"%s\",\"ip\":\"%s\","
                "\"proto\":\"%s\",",
                seq, datetime, host_esc, ip_esc, proto);
  if (show_port) {
    (void)fprintf(out, "\"port\":%u,", (unsigned)port);
  } else {
    (void)fprintf(out, "\"port\":null,");
  }
  (void)fprintf(out, "\"status\":\"%s\",", StatusToken(result));
  if (ttl >= 0) {
    (void)fprintf(out, "\"ttl\":%d,", (int)ttl);
  }
  if (result == SUCCESS) {
    (void)fprintf(out, "\"rtt_ms\":%.4f}\n", rtt * 1000.0);
  } else {
    (void)fprintf(out, "\"rtt_ms\":null}\n");
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
static void PrintSummaryJson(FILE *const out, const stats_t *const stats,
                             pcc_t host) {
  char host_esc[280U] = {0};
  double fail_percent = 0.0;

  if (stats->Attempts > 0U) {
    fail_percent = ((double)stats->Failures / (double)stats->Attempts) * 100.0;
  }
  JsonEscape(host, host_esc, sizeof(host_esc));

  (void)fprintf(out,
                "{\"summary\":{\"host\":\"%s\",\"sent\":%lu,\"received\":%lu,"
                "\"lost\":%lu,\"loss_pct\":%.2f,",
                host_esc, (unsigned long)stats->Attempts,
                (unsigned long)stats->Connects, (unsigned long)stats->Failures,
                fail_percent);

  if (stats->Connects == 0U) {
    (void)fprintf(out, "\"min_ms\":null,\"avg_ms\":null,\"max_ms\":null,"
                       "\"mdev_ms\":null,\"p50_ms\":null,\"p95_ms\":null,"
                       "\"p99_ms\":null}}\n");
    return;
  }

  (void)fprintf(
      out, "\"min_ms\":%.4f,\"avg_ms\":%.4f,\"max_ms\":%.4f,\"mdev_ms\":%.4f,"
           "\"p50_ms\":%.4f,\"p95_ms\":%.4f,\"p99_ms\":%.4f}}\n",
      stats->Minimum * 1000.0, Stats_Average(stats) * 1000.0,
      stats->Maximum * 1000.0, Stats_StdDev(stats) * 1000.0,
      Stats_Percentile(stats, 50.0) * 1000.0,
      Stats_Percentile(stats, 95.0) * 1000.0,
      Stats_Percentile(stats, 99.0) * 1000.0);
}

/** @brief Print a target's header line as a JSON object. */
static void PrintTargetJson(FILE *const out, const probe_target_t *const t) {
  char he[280] = {0};
  char ie[80] = {0};
  JsonEscape(t->Host, he, sizeof(he));
  JsonEscape(t->Resolved.IPAddress, ie, sizeof(ie));
  (void)fprintf(out, "{\"target\":{\"host\":\"%s\",\"ip\":\"%s\",", he, ie);
  if (t->Type == IPPROTO_ICMP) {
    (void)fprintf(out, "\"port\":null,");
  } else {
    (void)fprintf(out, "\"port\":%u,", (unsigned)t->Port);
  }
  (void)fprintf(out, "\"proto\":\"%s\",\"resolved\":%s",
                ProtoLabel(t->Type, t->Resolved.IPAddress),
                t->ResolveOk ? "true" : "false");
  if (t->Resolved.ReverseName[0] != '\0') {
    char re[520] = {0};
    JsonEscape(t->Resolved.ReverseName, re, sizeof(re));
    (void)fprintf(out, ",\"rdns\":\"%s\"", re);
  }
  if (t->Resolved.Asn[0] != '\0') {
    char ae[260] = {0};
    JsonEscape(t->Resolved.AsnOrg, ae, sizeof(ae));
    (void)fprintf(out, ",\"asn\":\"%s\",\"asn_org\":\"%s\"", t->Resolved.Asn,
                  ae);
  }
  (void)fprintf(out, "}}\n");
}

/**
 * @brief Run a load-test or resilience sweep.
 *
 * @return EXIT_SUCCESS on completion, EXIT_FAILURE if it fails.
 */
static int RunLoadTestMode(const arguments_t *const args, host_t *const host) {
  loadtest_cfg_t cfg;
  char line[256] = {0};
  pcc_t proto = ProtoLabel(host->Type, host->IPAddress);
  bool is_icmp = (host->Type == IPPROTO_ICMP);

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

/**
 * @brief Run the HTTP(S) probe mode: repeatedly fetch a URL and report status.
 *
 * @return EXIT_SUCCESS if at least one probe met the success criterion.
 */
static int RunHttpMode(const arguments_t *const args) {
  http_cfg_t cfg;
  stats_t stats;
  unsigned long cycle = 0;
  unsigned long ok = 0;
  char line[512] = {0};

  (void)snprintf(cfg.Method, sizeof(cfg.Method), "%s", args->HttpMethod);
  cfg.ExpectStatus = args->HttpStatus;
  cfg.TimeoutMs = args->Timeout;
  cfg.Quiet = args->Quiet;
  Stats_Init(&stats);

  if (!args->Json && !args->Prometheus) {
    (void)snprintf(line, sizeof(line), "%s v%s\n", NAME, PINGDD_VERSION_FULL);
    FormattedPrint(PRINT_BLUE, line);
    (void)snprintf(line, sizeof(line), "HTTP probe %s %s\n", cfg.Method,
                   args->HttpUrl);
    FormattedPrint(PRINT_YELLOW, line);
    ResetColor();
  }

  while ((g_interrupted == 0) &&
         ((args->Count == -1) || (cycle < (unsigned long)args->Count))) {
    http_result_t res;
    int32_t r = Http_Probe(args->HttpUrl, &cfg, &res);
    bool good = (r == (int32_t)SUCCESS) &&
                ((cfg.ExpectStatus == 0)
                     ? ((res.Status >= 200) && (res.Status < 400))
                     : (res.Status == cfg.ExpectStatus));

    stats.Attempts++;
    if (good) {
      ok++;
      stats.Connects++;
      Stats_AddSample(&stats, res.TotalMs / 1000.0);
    } else {
      stats.Failures++;
    }

    if (args->Json) {
      char ipe[80] = {0};
      JsonEscape(res.Ip, ipe, sizeof(ipe));
      (void)printf("{\"seq\":%lu,\"url\":\"", cycle);
      {
        char ue[1100] = {0};
        JsonEscape(args->HttpUrl, ue, sizeof(ue));
        (void)printf("%s\",\"ip\":\"%s\",\"status\":%d,\"tls\":%s,"
                     "\"tls_version\":\"%s\",\"cipher\":\"%s\","
                     "\"connect_ms\":%.3f,\"total_ms\":%.3f,\"ok\":%s",
                     ue, ipe, res.Status, res.Tls ? "true" : "false",
                     res.TlsVersion, res.Cipher, res.ConnectMs, res.TotalMs,
                     good ? "true" : "false");
      }
      if (res.Error[0] != '\0') {
        char ee[200] = {0};
        JsonEscape(res.Error, ee, sizeof(ee));
        (void)printf(",\"error\":\"%s\"", ee);
      }
      (void)printf("}\n");
    } else if (!args->Quiet && !args->Prometheus) {
      if (r == (int32_t)SUCCESS) {
        (void)snprintf(line, sizeof(line),
                       "seq=%lu %s status=%d time=%.2fms connect=%.2fms",
                       cycle, res.Ip, res.Status, res.TotalMs, res.ConnectMs);
        FormattedPrint(good ? PRINT_GREEN : PRINT_YELLOW, line);
        if (res.Tls) {
          (void)snprintf(line, sizeof(line), " [%s %s]", res.TlsVersion,
                         res.Cipher);
          FormattedPrint(PRINT_BLUE, line);
        }
        (void)printf("\n");
      } else {
        (void)snprintf(line, sizeof(line), "seq=%lu failed: %s\n", cycle,
                       res.Error);
        FormattedPrint(PRINT_RED, line);
      }
      ResetColor();
    }

    cycle++;
    if ((args->Count == -1) || (cycle < (unsigned long)args->Count)) {
      delay_ms((args->Rate > 0U) ? args->Rate : 50U);
    }
  }

  if (!args->Json && !args->Prometheus) {
    double avg = Stats_Average(&stats) * 1000.0;
    (void)snprintf(line, sizeof(line),
                   "\n%lu sent, %lu ok, %lu failed; avg %.2f ms\n",
                   (unsigned long)stats.Attempts, ok,
                   (unsigned long)stats.Failures, avg);
    FormattedPrint(PRINT_BLUE, line);
    ResetColor();
  }
  Stats_Free(&stats);
  return (ok > 0UL) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/** @brief Print the combined (all-targets) human roll-up. */
static void PrintCombinedHuman(target_list_t *list) {
  char buf[160U] = {0};
  unsigned long long att = 0, con = 0, fail = 0;
  double total = 0.0, totalsq = 0.0, mn = 0.0, mx = 0.0;
  int have = 0;
  unsigned long outages = 0;
  bool any_down = false;
  size_t i = 0;
  double loss = 0.0;
  double avg = 0.0, var = 0.0;
  pcc_t verdict = "HEALTHY";
  int32_t vcolor = PRINT_GREEN;

  for (i = 0; i < list->Count; i++) {
    stats_t *s = &list->Items[i].Stats;
    if (!list->Items[i].ResolveOk) {
      continue;
    }
    att += s->Attempts;
    con += s->Connects;
    fail += s->Failures;
    total += s->Total;
    totalsq += s->TotalSq;
    if (s->Connects > 0U) {
      if ((have == 0) || (s->Minimum < mn)) {
        mn = s->Minimum;
      }
      if (s->Maximum > mx) {
        mx = s->Maximum;
      }
      have = 1;
    }
    outages += list->Items[i].Diag.Outages;
    if (list->Items[i].Diag.IsDown) {
      any_down = true;
    }
  }

  if (att > 0ULL) {
    loss = ((double)fail / (double)att) * 100.0;
  }
  if (con > 0ULL) {
    avg = total / (double)con;
    var = (totalsq / (double)con) - (avg * avg);
    if (var < 0.0) {
      var = 0.0;
    }
  }
  if (any_down || (loss >= 100.0)) {
    verdict = "DOWN";
    vcolor = PRINT_RED;
  } else if ((loss > 5.0) || (outages > 0U)) {
    verdict = "DEGRADED";
    vcolor = PRINT_YELLOW;
  }

  (void)snprintf(buf, sizeof(buf), "\n== combined (%zu targets) ==\n",
                 list->Count);
  FormattedPrint(PRINT_YELLOW, buf);
  ResetColor();
  (void)snprintf(buf, sizeof(buf),
                 "        %llu sent, %llu received, %llu lost (%.2f%% loss)\n",
                 att, con, fail, loss);
  FormattedPrint(PRINT_BLUE, buf);
  if (have != 0) {
    (void)snprintf(buf, sizeof(buf),
                   "        rtt min/avg/max/mdev = %.4f/%.4f/%.4f/%.4f ms\n",
                   mn * 1000.0, avg * 1000.0, mx * 1000.0, sqrt(var) * 1000.0);
    FormattedPrint(PRINT_BLUE, buf);
  }
  FormattedPrint(PRINT_BLUE, "        Verdict        = ");
  (void)snprintf(buf, sizeof(buf), "%s\n", verdict);
  FormattedPrint(vcolor, buf);
  ResetColor();
}

/** @brief Write the per-target label set for a Prometheus series. */
static void PromLabels(const probe_target_t *const t, char *const out,
                       size_t const out_size) {
  char le[400] = {0};
  char he[280] = {0};
  char ie[80] = {0};
  char port[16] = {0};
  JsonEscape(t->Label, le, sizeof(le));
  JsonEscape(t->Host, he, sizeof(he));
  JsonEscape(t->Resolved.IPAddress, ie, sizeof(ie));
  if (t->Type != IPPROTO_ICMP) {
    (void)snprintf(port, sizeof(port), "%u", (unsigned)t->Port);
  }
  (void)snprintf(out, out_size,
                 "target=\"%s\",host=\"%s\",ip=\"%s\",proto=\"%s\",port=\"%s\"",
                 le, he, ie, ProtoLabel(t->Type, t->Resolved.IPAddress), port);
}

/**
 * @brief Emit a Prometheus / OpenMetrics text exposition for all targets.
 *
 * One block of HELP/TYPE per metric family, then one series per target, so the
 * output is valid for scraping by node_exporter's textfile collector or a
 * Pushgateway.
 */
static void WritePrometheus(FILE *const out, target_list_t *const list) {
  size_t i = 0;
  char lbl[900] = {0};

  (void)fprintf(out, "# HELP pingdd_up Last-probe reachability (1=reachable).\n");
  (void)fprintf(out, "# TYPE pingdd_up gauge\n");
  for (i = 0; i < list->Count; i++) {
    probe_target_t *t = &list->Items[i];
    int up = (t->ResolveOk && (t->LastResult == (int32_t)SUCCESS)) ? 1 : 0;
    PromLabels(t, lbl, sizeof(lbl));
    (void)fprintf(out, "pingdd_up{%s} %d\n", lbl, up);
  }

  (void)fprintf(out, "# HELP pingdd_probes_total Total probes attempted.\n");
  (void)fprintf(out, "# TYPE pingdd_probes_total counter\n");
  for (i = 0; i < list->Count; i++) {
    probe_target_t *t = &list->Items[i];
    PromLabels(t, lbl, sizeof(lbl));
    (void)fprintf(out, "pingdd_probes_total{%s} %lu\n", lbl,
                  (unsigned long)t->Stats.Attempts);
  }

  (void)fprintf(out, "# HELP pingdd_failures_total Total failed probes.\n");
  (void)fprintf(out, "# TYPE pingdd_failures_total counter\n");
  for (i = 0; i < list->Count; i++) {
    probe_target_t *t = &list->Items[i];
    PromLabels(t, lbl, sizeof(lbl));
    (void)fprintf(out, "pingdd_failures_total{%s} %lu\n", lbl,
                  (unsigned long)t->Stats.Failures);
  }

  (void)fprintf(out, "# HELP pingdd_loss_ratio Fraction of probes that failed.\n");
  (void)fprintf(out, "# TYPE pingdd_loss_ratio gauge\n");
  for (i = 0; i < list->Count; i++) {
    probe_target_t *t = &list->Items[i];
    double loss = (t->Stats.Attempts > 0U)
                      ? (double)t->Stats.Failures / (double)t->Stats.Attempts
                      : 0.0;
    PromLabels(t, lbl, sizeof(lbl));
    (void)fprintf(out, "pingdd_loss_ratio{%s} %.6f\n", lbl, loss);
  }

  (void)fprintf(out,
                "# HELP pingdd_rtt_seconds Round-trip time aggregates.\n");
  (void)fprintf(out, "# TYPE pingdd_rtt_seconds gauge\n");
  for (i = 0; i < list->Count; i++) {
    probe_target_t *t = &list->Items[i];
    PromLabels(t, lbl, sizeof(lbl));
    if (t->Stats.Connects > 0U) {
      (void)fprintf(out, "pingdd_rtt_seconds{%s,stat=\"min\"} %.6f\n", lbl,
                    t->Stats.Minimum);
      (void)fprintf(out, "pingdd_rtt_seconds{%s,stat=\"avg\"} %.6f\n", lbl,
                    Stats_Average(&t->Stats));
      (void)fprintf(out, "pingdd_rtt_seconds{%s,stat=\"max\"} %.6f\n", lbl,
                    t->Stats.Maximum);
      (void)fprintf(out, "pingdd_rtt_seconds{%s,quantile=\"0.95\"} %.6f\n", lbl,
                    Stats_Percentile(&t->Stats, 95.0));
    }
  }
}

/** @brief Run the multi-target probe loop (sequential or concurrent). */
static int ProbeLoop(const arguments_t *args, target_list_t *list,
                     double dns_ms) {
  bool multi = (list->Count > 1U);
  bool silent_diag = args->Quiet || args->Json || multi;
  pingdd_timer_t total = {0};
  probe_target_t **rt = NULL;
  cpool_t *pool = NULL;
  size_t rc = 0;
  size_t i = 0;
  unsigned long cycle = 0U;
  /* JSON sinks: stdout (--json) and/or the log file (--json-file). */
  FILE *jsink[2];
  size_t nsink = 0U;
  size_t s = 0U;

  if (args->Json) {
    jsink[nsink++] = stdout;
  }
  if (args->JsonFile && (json_file != NULL)) {
    jsink[nsink++] = json_file;
  }

  rt = (probe_target_t **)calloc(list->Count, sizeof(*rt));
  if (rt == NULL) {
    return EXIT_FAILURE;
  }
  for (i = 0; i < list->Count; i++) {
    probe_target_t *t = &list->Items[i];
    Diag_Init(&t->Diag, silent_diag);
    Diag_SetDns(&t->Diag, dns_ms, !t->ResolveOk);
    if (t->ResolveOk) {
      rt[rc++] = t;
    }
  }

  /* Persistent worker pool: created once, reused for every cycle. */
  if (args->Concurrent && (rc > 0U)) {
    pool = Cpool_Create(rt, rc, args->Timeout);
    /* If the pool can't start, fall back to sequential scheduling. */
  }

  /* Header */
  if (nsink > 0U) {
    for (i = 0; i < list->Count; i++) {
      for (s = 0; s < nsink; s++) {
        PrintTargetJson(jsink[s], &list->Items[i]);
      }
    }
  }
  if (!args->Json && !args->Prometheus) {
    char line[420] = {0};
    (void)snprintf(line, sizeof(line), "%s v%s\n", NAME, PINGDD_VERSION_FULL);
    FormattedPrint(PRINT_BLUE, line);
    if (multi) {
      (void)snprintf(line, sizeof(line), "Probing %zu targets:\n", list->Count);
      FormattedPrint(PRINT_YELLOW, line);
      for (i = 0; i < list->Count; i++) {
        probe_target_t *t = &list->Items[i];
        (void)snprintf(line, sizeof(line), "  [%s]%s\n", t->Label,
                       t->ResolveOk ? "" : "  (unresolved)");
        FormattedPrint(t->ResolveOk ? PRINT_GREEN : PRINT_RED, line);
        if (args->Resolve && t->ResolveOk) {
          char ann[420] = {0};
          char annline[460] = {0};
          if (FormatAnnotation(&t->Resolved, ann, sizeof(ann))) {
            (void)snprintf(annline, sizeof(annline), "      %s\n", ann);
            FormattedPrint(PRINT_BLUE, annline);
          }
        }
      }
    } else {
      probe_target_t *t = &list->Items[0];
      char ts[64] = {0};
      pcc_t proto = ProtoLabel(t->Type, t->Resolved.IPAddress);
      GetTimestampString(ts, sizeof(ts));
      FormattedPrint(PRINT_YELLOW, "Connecting to ");
      FormattedPrint(PRINT_GREEN, t->Host);
      FormattedPrint(PRINT_YELLOW, " on ");
      FormattedPrint(PRINT_GREEN, proto);
      if (t->Type != IPPROTO_ICMP) {
        (void)snprintf(line, sizeof(line), " %u", (unsigned)t->Port);
        FormattedPrint(PRINT_GREEN, line);
      }
      FormattedPrint(PRINT_YELLOW, " on ");
      FormattedPrint(PRINT_GREEN, ts);
      FormattedPrint(PRINT_YELLOW, ":\n");
      if (args->Resolve && t->ResolveOk) {
        char ann[420] = {0};
        char annline[500] = {0};
        if (FormatAnnotation(&t->Resolved, ann, sizeof(ann))) {
          (void)snprintf(annline, sizeof(annline), "  %s -> %s\n",
                         t->Resolved.IPAddress, ann);
          FormattedPrint(PRINT_BLUE, annline);
        }
      }
    }
    ResetColor();
  }
  (void)fflush(stdout);

  Timer_Start(&total);

  while ((g_interrupted == 0) && ((args->Count == -1) || (cycle < (unsigned long)args->Count))) {
    bool interrupted_now = false;
    size_t k = 0;

    if ((args->Deadline > 0U) && (ElapsedMs(&total) >= (double)args->Deadline)) {
      break;
    }

    if (pool != NULL) {
      Cpool_RunCycle(pool); /* all targets probed in parallel, blocks to done */
    } else {
      for (k = 0; k < rc; k++) {
        Probe_One(rt[k], args->Timeout);
        if (g_interrupted != 0) {
          break;
        }
      }
    }

    /* Process results in target order. */
    for (k = 0; k < rc; k++) {
      probe_target_t *t = rt[k];
      int32_t r = t->LastResult;
      char datetime[64] = {0};
      pcc_t proto = ProtoLabel(t->Type, t->LastIp);
      bool show_port = (t->Type != IPPROTO_ICMP);
      unsigned long seq = t->Stats.Attempts;
      bool success = (r == SUCCESS);

      if (r == PINGDD_INTERRUPTED) {
        interrupted_now = true;
        continue;
      }

      GetTimestampString(datetime, sizeof(datetime));

      if (success) {
        Stats_AddSample(&t->Stats, t->LastRtt);
        t->Stats.Connects++;
        if (args->Audible) {
          (void)printf("\a");
        }
        if (args->CSVOutput && (csv_file != NULL)) {
          (void)WriteCSVRow(csv_file, &t->Resolved, t->LastRtt, datetime,
                            t->LastIp, proto);
        }
      } else {
        t->Stats.Failures++;
      }
      t->Stats.Attempts++;

      /* NDJSON: the file sink logs every probe (like CSV); the stdout sink
       * still honors --quiet. */
      for (s = 0; s < nsink; s++) {
        if ((jsink[s] == stdout) && args->Quiet) {
          continue;
        }
        PrintProbeJson(jsink[s], r, seq, t->LastIp, t->LastRtt, proto, show_port,
                       t->Port, datetime, t->Host, t->LastTtl);
      }
      if (!args->Quiet && !args->Json && !args->Prometheus) {
        PrintProbeHuman(r, seq, t->LastIp, t->LastRtt, proto, show_port,
                        t->Port, datetime, multi ? t->Label : NULL, t->LastTtl);
      }
      Diag_Observe(&t->Diag, success, t->LastRtt);
    }
    (void)fflush(stdout);

    if ((g_interrupted != 0) || interrupted_now) {
      break;
    }

    cycle++;
    if ((args->Count != -1) && (cycle >= (unsigned long)args->Count)) {
      break;
    }
    if ((args->Deadline > 0U) && (ElapsedMs(&total) >= (double)args->Deadline)) {
      break;
    }
    delay_ms((args->Rate > 0U) ? args->Rate : 50U);
  }

  if ((g_interrupted != 0) && !args->Json && !args->Prometheus) {
    FormattedPrint(PRINT_YELLOW, "\n^C interrupted\n");
    ResetColor();
  }

  /* Per-target summaries. */
  for (i = 0; i < list->Count; i++) {
    probe_target_t *t = &list->Items[i];
    if (multi && !args->Json && !args->Prometheus) {
      char hdr[360] = {0};
      (void)snprintf(hdr, sizeof(hdr), "\n-- %s --\n", t->Label);
      FormattedPrint(PRINT_YELLOW, hdr);
      ResetColor();
    }
    if (!t->ResolveOk) {
      if (!args->Json && !args->Prometheus) {
        FormattedPrint(PRINT_RED, "        unresolved\n");
      }
      continue;
    }
    for (s = 0; s < nsink; s++) {
      PrintSummaryJson(jsink[s], &t->Stats, t->Host);
      Diag_PrintJson(jsink[s], &t->Diag, &t->Stats);
    }
    if (!args->Json && !args->Prometheus) {
      PrintSummaryHuman(&t->Stats);
      Diag_PrintSummary(&t->Diag, &t->Stats);
      if (args->Monitor) {
        char line[96] = {0};
        double avail = (t->Stats.Attempts > 0U)
                           ? ((double)t->Stats.Connects /
                              (double)t->Stats.Attempts) * 100.0
                           : 0.0;
        (void)snprintf(line, sizeof(line),
                       "        Availability   = %.3f%% over %lu probes\n",
                       avail, (unsigned long)t->Stats.Attempts);
        FormattedPrint(PRINT_BLUE, line);
      }
    }
  }

  if (multi && !args->Json && !args->Prometheus) {
    PrintCombinedHuman(list);
  }
  if (args->Prometheus) {
    WritePrometheus(stdout, list);
  }
  (void)fflush(stdout);

  Cpool_Destroy(pool);
  free(rt);

  /* Exit non-zero if every probe failed across all targets. */
  {
    unsigned long long att = 0, con = 0;
    for (i = 0; i < list->Count; i++) {
      att += list->Items[i].Stats.Attempts;
      con += list->Items[i].Stats.Connects;
    }
    if ((att > 0ULL) && (con == 0ULL)) {
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}

/** @brief Program entry point. */
int main(int argc, char *const *argv) {
  arguments_t args = {0};
  target_list_t targets = {0};
  pingdd_timer_t dns_timer = {0};
  double dns_ms = 0.0;
  char err[256] = {0};
  size_t resolved = 0;
  bool color = false;
  int rc = EXIT_SUCCESS;

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

  /* Optional source-interface binding. */
  if (args.Interface != NULL) {
    if (SetSourceInterface(args.Interface, err, sizeof(err)) != (int32_t)SUCCESS) {
      char msg[320] = {0};
      (void)snprintf(msg, sizeof(msg), "Error: interface '%s': %s",
                     args.Interface, err);
      PrintError(msg);
      return EXIT_FAILURE;
    }
  }

  /* Optional IP ToS/DSCP marking on every probe socket. */
  SetTos(args.Tos);

  /* HTTP(S) probe mode is self-contained (URL carries the destination). */
  if (args.HttpUrl != NULL) {
    return RunHttpMode(&args);
  }

  /* Build the target matrix (hosts x ports x protocols + specs + file). */
  if (Targets_Build(&args, &targets, err, sizeof(err)) != 0) {
    char msg[320] = {0};
    (void)snprintf(msg, sizeof(msg), "Error: %s", err);
    PrintError(msg);
    return EXIT_FAILURE;
  }

  /* Resolve all targets (timed, for DNS diagnostics). */
  Timer_Start(&dns_timer);
  resolved = Targets_Resolve(&targets);
  dns_ms = Timer_Stop(&dns_timer) * 1000.0;

  if (resolved == 0U) {
    PrintError("Error: no targets could be resolved");
    Targets_Free(&targets);
    return EXIT_FAILURE;
  }

  /* Optional reverse-DNS + ASN annotation of each resolved address. */
  if (args.Resolve) {
    size_t ti = 0;
    for (ti = 0; ti < targets.Count; ti++) {
      if (targets.Items[ti].ResolveOk) {
        Resolve_Annotate(&targets.Items[ti].Resolved);
      }
    }
  }

  /* Traceroute: path discovery to a single target. */
  if (args.Traceroute) {
    trace_cfg_t tcfg;
    int32_t tr = 0;
    if (targets.Count != 1U) {
      PrintError("Error: --traceroute supports a single target");
      Targets_Free(&targets);
      return EXIT_FAILURE;
    }
    tcfg.MaxHops = args.MaxHops;
    tcfg.Queries = args.Queries;
    tcfg.TimeoutMs = args.Timeout;
    tcfg.Resolve = args.Resolve;
    tr = Traceroute_Run(&targets.Items[0].Resolved, &tcfg);
    Targets_Free(&targets);
    return (tr == (int32_t)SUCCESS) ? EXIT_SUCCESS : EXIT_FAILURE;
  }

  /* Authorized load testing / resilience: single-target only. */
  if (args.LoadTest || args.Resilience) {
    if (targets.Count != 1U) {
      PrintError("Error: --load-test/--resilience support a single target");
      Targets_Free(&targets);
      return EXIT_FAILURE;
    }
    rc = RunLoadTestMode(&args, &targets.Items[0].Resolved);
    Targets_Free(&targets);
    return rc;
  }

  /* CSV setup (filename derived from the first destination). */
  if (args.CSVOutput) {
    char *csv_filename = GenerateCSVFilename(&args);
    csv_file = fopen(csv_filename, "w");
    if (csv_file == NULL) {
      PrintError("Failed to create CSV file");
      Targets_Free(&targets);
      return EXIT_FAILURE;
    }
    (void)WriteCSVHeader(csv_file, &targets.Items[0].Resolved);
    if (!args.Json) {
      FormattedPrint(PRINT_YELLOW, "CSV logging: ");
      FormattedPrint(PRINT_GREEN, csv_filename);
      FormattedPrint(PRINT_YELLOW, "\n");
    }
  }

  /* JSON file setup (filename derived from the first destination). */
  if (args.JsonFile) {
    char *json_filename = GenerateJsonFilename(&args);
    json_file = fopen(json_filename, "w");
    if (json_file == NULL) {
      PrintError("Failed to create JSON file");
      if (csv_file != NULL) {
        (void)fclose(csv_file);
        csv_file = NULL;
      }
      Targets_Free(&targets);
      return EXIT_FAILURE;
    }
    if (!args.Json) {
      FormattedPrint(PRINT_YELLOW, "JSON logging: ");
      FormattedPrint(PRINT_GREEN, json_filename);
      FormattedPrint(PRINT_YELLOW, "\n");
    }
  }

  rc = ProbeLoop(&args, &targets, dns_ms);

  if ((args.CSVOutput != 0U) && (csv_file != NULL)) {
    (void)fclose(csv_file);
    csv_file = NULL;
  }
  if (args.JsonFile && (json_file != NULL)) {
    (void)fclose(json_file);
    json_file = NULL;
  }
  Targets_Free(&targets);
  return rc;
}
