/**
 * @file loadtest.c
 * @brief Authorized load testing and resilience measurement implementation.
 */

#include "loadtest.h"

#include "print.h"
#include "socket.h"

#include <float.h>
#include <math.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <time.h>
#endif

/* --------------------------------------------------------------------------
 * Portable primitives
 * ------------------------------------------------------------------------ */

/** @brief Monotonic clock in milliseconds. */
static double NowMs(void) {
#ifdef _WIN32
  LARGE_INTEGER freq;
  LARGE_INTEGER counter;
  (void)QueryPerformanceFrequency(&freq);
  (void)QueryPerformanceCounter(&counter);
  if (freq.QuadPart == 0) {
    return 0.0;
  }
  return ((double)counter.QuadPart * 1000.0) / (double)freq.QuadPart;
#else
  struct timespec ts;
  (void)clock_gettime(CLOCK_MONOTONIC, &ts);
  return ((double)ts.tv_sec * 1000.0) + ((double)ts.tv_nsec / 1.0e6);
#endif
}

static void SleepMs(uint32_t ms) {
#ifdef _WIN32
  Sleep(ms);
#else
  struct timespec ts;
  ts.tv_sec = (time_t)(ms / 1000U);
  ts.tv_nsec = (long)(ms % 1000U) * 1000000L;
  (void)nanosleep(&ts, NULL);
#endif
}

typedef struct {
  const loadtest_cfg_t *Cfg;
  double DeadlineMs;
  loadtest_result_t Res;
} worker_arg_t;

/** @brief One worker: hammer connect() until the deadline or interrupt. */
#ifdef _WIN32
static DWORD WINAPI WorkerMain(LPVOID param)
#else
static void *WorkerMain(void *param)
#endif
{
  worker_arg_t *w = (worker_arg_t *)param;
  const loadtest_cfg_t *cfg = w->Cfg;

  w->Res.MinRtt = DBL_MAX;
  w->Res.MaxRtt = 0.0;

  while ((NowMs() < w->DeadlineMs) && (g_interrupted == 0)) {
    double rtt = 0.0;
    char ip[64];
    int32_t r = Connect(cfg->Host, cfg->TimeoutMs, &rtt, ip, sizeof(ip));

    w->Res.Attempts++;
    if (r == SUCCESS) {
      w->Res.Success++;
      w->Res.TotalRtt += rtt;
      w->Res.TotalRttSq += rtt * rtt;
      if (rtt < w->Res.MinRtt) {
        w->Res.MinRtt = rtt;
      }
      if (rtt > w->Res.MaxRtt) {
        w->Res.MaxRtt = rtt;
      }
    } else if (r == PINGDD_INTERRUPTED) {
      break;
    } else {
      w->Res.Failed++;
    }
  }

#ifdef _WIN32
  return 0;
#else
  return NULL;
#endif
}

/* --------------------------------------------------------------------------
 * Derived metrics
 * ------------------------------------------------------------------------ */

double LoadTest_Rps(const loadtest_result_t *const r) {
  if ((r == NULL) || (r->DurationSec <= 0.0)) {
    return 0.0;
  }
  return (double)r->Success / r->DurationSec;
}

double LoadTest_AvgRtt(const loadtest_result_t *const r) {
  if ((r == NULL) || (r->Success == 0ULL)) {
    return 0.0;
  }
  return r->TotalRtt / (double)r->Success;
}

double LoadTest_StdDevRtt(const loadtest_result_t *const r) {
  double mean = 0.0;
  double var = 0.0;
  if ((r == NULL) || (r->Success == 0ULL)) {
    return 0.0;
  }
  mean = r->TotalRtt / (double)r->Success;
  var = (r->TotalRttSq / (double)r->Success) - (mean * mean);
  if (var < 0.0) {
    var = 0.0;
  }
  return sqrt(var);
}

static double LossPct(const loadtest_result_t *const r) {
  if ((r == NULL) || (r->Attempts == 0ULL)) {
    return 0.0;
  }
  return ((double)r->Failed / (double)r->Attempts) * 100.0;
}

/* --------------------------------------------------------------------------
 * Core run
 * ------------------------------------------------------------------------ */

int LoadTest_Run(const loadtest_cfg_t *const cfg, loadtest_result_t *const out) {
  worker_arg_t *args = NULL;
#ifdef _WIN32
  HANDLE *threads = NULL;
#else
  pthread_t *threads = NULL;
#endif
  uint32_t conc = 0;
  uint32_t n = 0;
  uint32_t started = 0;
  double start_ms = 0.0;
  double deadline_ms = 0.0;

  if ((cfg == NULL) || (out == NULL) || (cfg->Host == NULL)) {
    return -1;
  }

  conc = cfg->Concurrency;
  if (conc < 1U) {
    conc = 1U;
  }
  if (conc > LOADTEST_MAX_CONCURRENCY) {
    conc = LOADTEST_MAX_CONCURRENCY;
  }

  args = (worker_arg_t *)calloc(conc, sizeof(worker_arg_t));
#ifdef _WIN32
  threads = (HANDLE *)calloc(conc, sizeof(HANDLE));
#else
  threads = (pthread_t *)calloc(conc, sizeof(pthread_t));
#endif
  if ((args == NULL) || (threads == NULL)) {
    free(args);
    free(threads);
    return -1;
  }

  start_ms = NowMs();
  deadline_ms = start_ms + (double)cfg->DurationMs;

  for (n = 0; n < conc; n++) {
    args[n].Cfg = cfg;
    args[n].DeadlineMs = deadline_ms;
#ifdef _WIN32
    threads[n] = CreateThread(NULL, 0, WorkerMain, &args[n], 0, NULL);
    if (threads[n] == NULL) {
      break;
    }
#else
    if (pthread_create(&threads[n], NULL, WorkerMain, &args[n]) != 0) {
      break;
    }
#endif
    started++;
  }

  /* Progress: tick once a second without touching worker counters (avoids
   * data races); the real numbers are merged after the join. */
  if (!cfg->Silent) {
    while ((NowMs() < deadline_ms) && (g_interrupted == 0)) {
      double elapsed = (NowMs() - start_ms) / 1000.0;
      char buf[96];
      (void)snprintf(buf, sizeof(buf), "  load: %u workers  %.0f/%.0fs\r", conc,
                     elapsed, (double)cfg->DurationMs / 1000.0);
      FormattedPrint(PRINT_YELLOW, buf);
      (void)fflush(stdout);
      SleepMs(500);
    }
    (void)printf("\r                                             \r");
  }

  for (n = 0; n < started; n++) {
#ifdef _WIN32
    (void)WaitForSingleObject(threads[n], INFINITE);
    (void)CloseHandle(threads[n]);
#else
    (void)pthread_join(threads[n], NULL);
#endif
  }

  /* Merge per-worker results. */
  memset(out, 0, sizeof(*out));
  out->Concurrency = conc;
  out->DurationSec = (NowMs() - start_ms) / 1000.0;
  out->MinRtt = DBL_MAX;
  out->MaxRtt = 0.0;
  for (n = 0; n < started; n++) {
    out->Attempts += args[n].Res.Attempts;
    out->Success += args[n].Res.Success;
    out->Failed += args[n].Res.Failed;
    out->TotalRtt += args[n].Res.TotalRtt;
    out->TotalRttSq += args[n].Res.TotalRttSq;
    if ((args[n].Res.Success > 0ULL) && (args[n].Res.MinRtt < out->MinRtt)) {
      out->MinRtt = args[n].Res.MinRtt;
    }
    if (args[n].Res.MaxRtt > out->MaxRtt) {
      out->MaxRtt = args[n].Res.MaxRtt;
    }
  }
  if (out->Success == 0ULL) {
    out->MinRtt = 0.0;
  }

  free(args);
  free(threads);
  return 0;
}

/* --------------------------------------------------------------------------
 * Resilience sweep
 * ------------------------------------------------------------------------ */

int LoadTest_Resilience(const loadtest_cfg_t *const cfg) {
  uint32_t levels[16];
  uint32_t level_count = 0;
  uint32_t max_conc = 0;
  uint32_t lvl = 0;
  uint32_t idx = 0;
  uint32_t phase_ms = 0;
  double baseline_avg = 0.0;
  double baseline_loss = 0.0;
  int knee = -1;
  char buf[160];

  if ((cfg == NULL) || (cfg->Host == NULL)) {
    return -1;
  }

  max_conc = cfg->Concurrency;
  if (max_conc < 1U) {
    max_conc = 1U;
  }
  if (max_conc > LOADTEST_MAX_CONCURRENCY) {
    max_conc = LOADTEST_MAX_CONCURRENCY;
  }

  /* Build the ramp 1,2,4,... up to and including max_conc. */
  for (lvl = 1U; (lvl < max_conc) && (level_count < 15U); lvl *= 2U) {
    levels[level_count++] = lvl;
  }
  levels[level_count++] = max_conc;

  phase_ms = cfg->DurationMs / level_count;
  if (phase_ms < 500U) {
    phase_ms = 500U;
  }

  FormattedPrint(PRINT_YELLOW,
                 "Resilience sweep (ramping concurrency)\n");
  FormattedPrint(PRINT_WHITE,
                 "  level  conn/s   avg ms   max ms   loss%   status\n");
  ResetColor();

  for (idx = 0; (idx < level_count) && (g_interrupted == 0); idx++) {
    loadtest_cfg_t phase = *cfg;
    loadtest_result_t r;
    double avg_ms = 0.0;
    double loss = 0.0;
    pcc_t status = "ok";
    int32_t color = PRINT_GREEN;

    phase.Concurrency = levels[idx];
    phase.DurationMs = phase_ms;
    phase.Silent = true;

    if (LoadTest_Run(&phase, &r) != 0) {
      return -1;
    }

    avg_ms = LoadTest_AvgRtt(&r) * 1000.0;
    loss = LossPct(&r);

    if (idx == 0U) {
      baseline_avg = avg_ms;
      baseline_loss = loss;
    }

    /* Degradation: latency more than doubled vs baseline, or loss climbing. */
    if ((loss > (baseline_loss + 2.0)) ||
        ((baseline_avg > 0.0) && (avg_ms > (baseline_avg * 2.0)))) {
      status = "DEGRADED";
      color = PRINT_RED;
      if (knee < 0) {
        knee = (int)levels[idx];
      }
    }

    (void)snprintf(buf, sizeof(buf),
                   "  %5u  %7.0f  %7.2f  %7.2f  %6.2f   ", levels[idx],
                   LoadTest_Rps(&r), avg_ms, r.MaxRtt * 1000.0, loss);
    FormattedPrint(PRINT_BLUE, buf);
    FormattedPrint(color, status);
    (void)printf("\n");
    (void)fflush(stdout);
  }

  if (knee >= 0) {
    (void)snprintf(buf, sizeof(buf),
                   "Verdict: degradation starts at ~%d concurrent "
                   "connections.\n",
                   knee);
    FormattedPrint(PRINT_RED, buf);
  } else {
    (void)snprintf(
        buf, sizeof(buf),
        "Verdict: held steady up to %u concurrent connections.\n", max_conc);
    FormattedPrint(PRINT_GREEN, buf);
  }
  ResetColor();

  return 0;
}
