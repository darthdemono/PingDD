/**
 * @file diag.c
 * @brief Network-quality diagnostics implementation.
 *
 * Implements the functions declared in diag.h.
 */

#include "diag.h"

#include "print.h"

#include <float.h>
#include <stdio.h>

/* --------------------------------------------------------------------------
 * Derived metrics
 * ------------------------------------------------------------------------ */

static double LossPercent(const stats_t *const stats) {
  if ((stats == NULL) || (stats->Attempts == 0U)) {
    return 0.0;
  }
  return ((double)stats->Failures / (double)stats->Attempts) * 100.0;
}

/** @brief Bufferbloat estimate: spread between p95 and the baseline minimum. */
static double BufferbloatMs(const stats_t *const stats) {
  if ((stats == NULL) || (stats->Connects == 0U) ||
      (stats->SampleCount < 5U)) {
    return -1.0; /* not enough samples to judge */
  }
  return (Stats_Percentile(stats, 95.0) - stats->Minimum) * 1000.0;
}

/** @brief Overall link grade: 0 = GOOD, 1 = FAIR, 2 = POOR. */
static int QualityGrade(const stats_t *const stats) {
  double loss = LossPercent(stats);
  double jitter_ms = Stats_StdDev(stats) * 1000.0;
  double avg_ms = Stats_Average(stats) * 1000.0;

  if ((loss > 5.0) || (jitter_ms > 50.0) || (avg_ms > 300.0)) {
    return 2;
  }
  if ((loss > 1.0) || (jitter_ms > 20.0) || (avg_ms > 150.0)) {
    return 1;
  }
  return 0;
}

static pcc_t QualityName(int grade) {
  switch (grade) {
  case 2:
    return "POOR";
  case 1:
    return "FAIR";
  default:
    return "GOOD";
  }
}

static pcc_t BloatName(double bloat_ms) {
  if (bloat_ms < 0.0) {
    return "n/a";
  }
  if (bloat_ms > 100.0) {
    return "severe";
  }
  if (bloat_ms > 50.0) {
    return "mild";
  }
  return "none";
}

static pcc_t Verdict(const diag_t *const diag, const stats_t *const stats) {
  if (diag->IsDown || (LossPercent(stats) >= 100.0)) {
    return "DOWN";
  }
  if ((QualityGrade(stats) == 2) || (diag->Outages > 0U) || diag->DnsFailed) {
    return "DEGRADED";
  }
  return "HEALTHY";
}

/* --------------------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------------------ */

void Diag_Init(diag_t *const diag, bool const silent) {
  if (diag == NULL) {
    return;
  }
  diag->DnsMs = 0.0;
  diag->DnsFailed = false;
  diag->ConsecFails = 0U;
  diag->LongestFailStreak = 0U;
  diag->Outages = 0U;
  diag->IsDown = false;
  diag->BaselineSec = DBL_MAX;
  diag->EwmaSec = 0.0;
  diag->EwmaInit = false;
  diag->HighLatency = false;
  diag->Silent = silent;
}

void Diag_SetDns(diag_t *const diag, double const dns_ms, bool const failed) {
  if (diag == NULL) {
    return;
  }
  diag->DnsMs = dns_ms;
  diag->DnsFailed = failed;
}

/* --------------------------------------------------------------------------
 * Live observation + alerts
 * ------------------------------------------------------------------------ */

static void Alert(const diag_t *const diag, int32_t color, pcc_t text) {
  if (diag->Silent) {
    return;
  }
  FormattedPrint(color, text);
  (void)printf("\n");
}

void Diag_Observe(diag_t *const diag, bool const success,
                  double const rtt_sec) {
  char msg[160U] = {0};

  if (diag == NULL) {
    return;
  }

  if (success) {
    /* Recovery from a declared outage is a real, reportable transition. */
    if (diag->IsDown) {
      (void)snprintf(msg, sizeof(msg),
                     "  + RECOVERED: link back up after %lu consecutive "
                     "failures",
                     diag->ConsecFails);
      Alert(diag, PRINT_GREEN, msg);
      diag->IsDown = false;
    }
    diag->ConsecFails = 0U;

    if (rtt_sec < diag->BaselineSec) {
      diag->BaselineSec = rtt_sec;
    }
    if (!diag->EwmaInit) {
      diag->EwmaSec = rtt_sec;
      diag->EwmaInit = true;
    } else {
      diag->EwmaSec =
          (DIAG_EWMA_ALPHA * rtt_sec) + ((1.0 - DIAG_EWMA_ALPHA) * diag->EwmaSec);
    }

    /* Sustained latency well above baseline is an issue, not noise. */
    {
      double base_ms = diag->BaselineSec * 1000.0;
      double ewma_ms = diag->EwmaSec * 1000.0;
      double spike_ms = base_ms * DIAG_LATENCY_SPIKE_FACTOR;
      double floor_ms = base_ms + DIAG_LATENCY_SPIKE_FLOOR_MS;
      double threshold = (spike_ms > floor_ms) ? spike_ms : floor_ms;

      if (!diag->HighLatency && (ewma_ms > threshold)) {
        (void)snprintf(msg, sizeof(msg),
                       "  ~ HIGH LATENCY: ~%.1f ms (baseline %.1f ms)", ewma_ms,
                       base_ms);
        Alert(diag, PRINT_YELLOW, msg);
        diag->HighLatency = true;
      } else if (diag->HighLatency && (ewma_ms < (base_ms * 1.5))) {
        (void)snprintf(msg, sizeof(msg),
                       "  + LATENCY NORMAL: ~%.1f ms", ewma_ms);
        Alert(diag, PRINT_GREEN, msg);
        diag->HighLatency = false;
      }
    }
  } else {
    diag->ConsecFails++;
    if (diag->ConsecFails > diag->LongestFailStreak) {
      diag->LongestFailStreak = diag->ConsecFails;
    }
    /* Below the threshold a drop is treated as noise (no alert). Crossing it
     * marks the start of a real outage episode. */
    if (diag->ConsecFails == DIAG_DOWN_THRESHOLD) {
      diag->IsDown = true;
      diag->Outages++;
      (void)snprintf(msg, sizeof(msg),
                     "  ! DOWN: %u consecutive failures (outage)",
                     (unsigned)DIAG_DOWN_THRESHOLD);
      Alert(diag, PRINT_RED, msg);
    }
  }
}

/* --------------------------------------------------------------------------
 * Final reports
 * ------------------------------------------------------------------------ */

void Diag_PrintSummary(const diag_t *const diag, const stats_t *const stats) {
  char buf[160U] = {0};
  double loss = 0.0;
  double jitter_ms = 0.0;
  double bloat_ms = 0.0;
  int grade = 0;
  pcc_t verdict = NULL;

  if ((diag == NULL) || (stats == NULL)) {
    return;
  }

  loss = LossPercent(stats);
  jitter_ms = Stats_StdDev(stats) * 1000.0;
  bloat_ms = BufferbloatMs(stats);
  grade = QualityGrade(stats);
  verdict = Verdict(diag, stats);

  FormattedPrint(PRINT_YELLOW, "Diagnostics:\n");
  ResetColor();

  if (diag->DnsFailed) {
    FormattedPrint(PRINT_BLUE, "        DNS resolution = ");
    FormattedPrint(PRINT_RED, "FAILED\n");
  } else {
    (void)snprintf(buf, sizeof(buf), "        DNS resolution = %.3f ms (%s)\n",
                   diag->DnsMs,
                   (diag->DnsMs > DIAG_SLOW_DNS_MS) ? "SLOW" : "ok");
    FormattedPrint(PRINT_BLUE, buf);
  }

  (void)snprintf(buf, sizeof(buf), "        Link quality   = %s\n",
                 QualityName(grade));
  FormattedPrint(PRINT_BLUE, buf);

  (void)snprintf(buf, sizeof(buf), "        Packet loss    = %.2f%% (%s)\n",
                 loss,
                 (loss > 5.0) ? "severe" : (loss > 1.0) ? "elevated" : "none");
  FormattedPrint(PRINT_BLUE, buf);

  (void)snprintf(buf, sizeof(buf), "        Jitter (mdev)  = %.4f ms (%s)\n",
                 jitter_ms,
                 (jitter_ms > 50.0) ? "high"
                                    : (jitter_ms > 20.0) ? "moderate" : "low");
  FormattedPrint(PRINT_BLUE, buf);

  if (bloat_ms < 0.0) {
    FormattedPrint(PRINT_BLUE,
                   "        Bufferbloat    = n/a (insufficient samples)\n");
  } else {
    (void)snprintf(buf, sizeof(buf),
                   "        Bufferbloat    = %s (+%.1f ms over baseline)\n",
                   BloatName(bloat_ms), bloat_ms);
    FormattedPrint(PRINT_BLUE, buf);
  }

  (void)snprintf(buf, sizeof(buf),
                 "        Stability      = %lu outage(s), longest fail "
                 "streak %lu\n",
                 diag->Outages, diag->LongestFailStreak);
  FormattedPrint(PRINT_BLUE, buf);

  FormattedPrint(PRINT_BLUE, "        Verdict        = ");
  {
    int32_t vcolor = (verdict[0] == 'H')   ? PRINT_GREEN
                     : (verdict[0] == 'D' && verdict[1] == 'E') ? PRINT_YELLOW
                                                                : PRINT_RED;
    (void)snprintf(buf, sizeof(buf), "%s\n", verdict);
    FormattedPrint(vcolor, buf);
  }
}

void Diag_PrintJson(const diag_t *const diag, const stats_t *const stats) {
  double bloat_ms = 0.0;

  if ((diag == NULL) || (stats == NULL)) {
    return;
  }
  bloat_ms = BufferbloatMs(stats);

  (void)printf("{\"diagnostics\":{");
  (void)printf("\"dns_ms\":%.3f,\"dns_ok\":%s,", diag->DnsMs,
               diag->DnsFailed ? "false" : "true");
  (void)printf("\"quality\":\"%s\",", QualityName(QualityGrade(stats)));
  (void)printf("\"loss_pct\":%.2f,", LossPercent(stats));
  (void)printf("\"jitter_ms\":%.4f,", Stats_StdDev(stats) * 1000.0);
  if (bloat_ms < 0.0) {
    (void)printf("\"bufferbloat\":\"n/a\",\"bufferbloat_ms\":null,");
  } else {
    (void)printf("\"bufferbloat\":\"%s\",\"bufferbloat_ms\":%.1f,",
                 BloatName(bloat_ms), bloat_ms);
  }
  (void)printf("\"outages\":%lu,\"longest_fail_streak\":%lu,\"down\":%s,",
               diag->Outages, diag->LongestFailStreak,
               diag->IsDown ? "true" : "false");
  (void)printf("\"verdict\":\"%s\"}}\n", Verdict(diag, stats));
}
