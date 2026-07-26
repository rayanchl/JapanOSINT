#include "source_trust.h"
#include "source_registry.h"
#include "../third_party/sqlite3.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/* Parse "YYYY-MM-DD HH:MM:SS" or ISO-8601 "YYYY-MM-DDTHH:MM:SS[.mmm][Z]" into
 * epoch seconds (UTC). Both spellings occur: SQLite datetime('now') writes the
 * space form, collectors write the T/Z form. Returns 0 if unparseable. */
static time_t parse_ts(const char *s) {
  if (!s || !*s) return 0;
  struct tm tm;
  memset(&tm, 0, sizeof tm);
  int y, mo, d, h, mi, se;
  if (sscanf(s, "%4d-%2d-%2d%*c%2d:%2d:%2d", &y, &mo, &d, &h, &mi, &se) != 6) {
    if (sscanf(s, "%4d-%2d-%2d", &y, &mo, &d) != 3) return 0;
    h = mi = se = 0;
  }
  tm.tm_year = y - 1900; tm.tm_mon = mo - 1; tm.tm_mday = d;
  tm.tm_hour = h; tm.tm_min = mi; tm.tm_sec = se;
  return timegm(&tm);
}

static double clamp01(double v) {
  if (!isfinite(v)) return 0.0;
  return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v);
}

/* Grade bands. Deliberately generous at the top and unforgiving below 0.5 —
 * the badge exists to steer an analyst away from a source they should not
 * lean on, so the interesting resolution is in the lower half. */
static char grade_of(double r) {
  if (r >= 0.90) return 'A';
  if (r >= 0.80) return 'B';
  if (r >= 0.65) return 'C';
  if (r >= 0.50) return 'D';
  if (r >= 0.30) return 'E';
  return 'F';
}

/* Freshness from last_success against the source's declared cadence.
 * Scheduled sources are judged against 3× their interval (one missed cycle is
 * normal, three is a problem). On-demand sources (interval <= 0) have no
 * cadence to violate, so they are judged against a flat 7-day horizon. */
static double freshness_of(const char *last_success, int interval_sec,
                           time_t now) {
  time_t ls = parse_ts(last_success);
  if (ls <= 0) return 0.0;                 /* never succeeded */
  double age = difftime(now, ls);
  if (age < 0) age = 0;
  double horizon = interval_sec > 0 ? (double)interval_sec * 3.0
                                    : 7.0 * 86400.0;
  if (horizon <= 0) return 0.0;
  return clamp01(1.0 - age / horizon);
}

/* Coefficient of variation over records_fetched → stability in 0..1.
 * Fewer than 3 samples is not enough to call a source unstable, so it scores
 * a neutral 0.5 rather than being punished for being new. A source that
 * consistently returns zero rows is not "stable" — it is broken — so mean<=0
 * scores 0. */
static double stability_of(long n, double sum, double sumsq) {
  if (n < 3) return 0.5;
  double mean = sum / (double)n;
  if (mean <= 0.0) return 0.0;
  double var = (sumsq / (double)n) - (mean * mean);
  if (var < 0) var = 0;                    /* float noise near zero variance */
  double cv = sqrt(var) / mean;
  return clamp01(1.0 - cv);
}

typedef struct { char id[256]; long n, ok; double sum, sumsq; } flrow;
typedef struct { char id[256]; long n; } anrow;

source_trust *source_trust_load(db_handle *db, int *out_n) {
  if (out_n) *out_n = 0;
  if (!db || !db->h) return NULL;

  /* ---- fetch_log, trailing 30 days ---------------------------------- */
  static const char *FQ =
    "SELECT source_id,COUNT(*),"
    "SUM(CASE WHEN status='ok' THEN 1 ELSE 0 END),"
    "SUM(COALESCE(records_fetched,0)),"
    "SUM(CAST(COALESCE(records_fetched,0) AS REAL)*COALESCE(records_fetched,0)) "
    "FROM fetch_log WHERE timestamp >= datetime('now','-30 days') "
    "GROUP BY source_id";
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, FQ, -1, &s, NULL) != SQLITE_OK) return NULL;
  int fcap = 64, fn = 0;
  flrow *F = malloc((size_t)fcap * sizeof *F);
  if (!F) { sqlite3_finalize(s); return NULL; }
  while (sqlite3_step(s) == SQLITE_ROW) {
    const unsigned char *sid = sqlite3_column_text(s, 0);
    if (!sid) continue;
    if (fn == fcap) {
      int nc = fcap * 2;
      flrow *nf = realloc(F, (size_t)nc * sizeof *F);
      if (!nf) { free(F); sqlite3_finalize(s); return NULL; }
      F = nf; fcap = nc;
    }
    flrow *r = &F[fn++];
    snprintf(r->id, sizeof r->id, "%s", (const char *)sid);
    r->n     = sqlite3_column_int64(s, 1);
    r->ok    = sqlite3_column_int64(s, 2);
    r->sum   = sqlite3_column_double(s, 3);
    r->sumsq = sqlite3_column_double(s, 4);
  }
  sqlite3_finalize(s);

  /* ---- anomalies, trailing 30 days ---------------------------------- */
  static const char *AQ =
    "SELECT source_id,COUNT(*) FROM collector_anomaly "
    "WHERE created_at >= datetime('now','-30 days') GROUP BY source_id";
  int acap = 64, an = 0;
  anrow *A = malloc((size_t)acap * sizeof *A);
  if (!A) { free(F); return NULL; }
  if (sqlite3_prepare_v2(db->h, AQ, -1, &s, NULL) == SQLITE_OK) {
    while (sqlite3_step(s) == SQLITE_ROW) {
      const unsigned char *sid = sqlite3_column_text(s, 0);
      if (!sid) continue;
      if (an == acap) {
        int nc = acap * 2;
        anrow *na2 = realloc(A, (size_t)nc * sizeof *A);
        if (!na2) break;
        A = na2; acap = nc;
      }
      snprintf(A[an].id, sizeof A[an].id, "%s", (const char *)sid);
      A[an].n = sqlite3_column_int64(s, 1);
      an++;
    }
    sqlite3_finalize(s);
  }

  /* ---- join against sources for last_success + quarantine ----------- */
  source_trust *T = calloc((size_t)(fn > 0 ? fn : 1), sizeof *T);
  if (!T) { free(F); free(A); return NULL; }
  time_t now = time(NULL);

  sqlite3_stmt *ss = NULL;
  sqlite3_prepare_v2(db->h,
      "SELECT last_success,quarantined_until FROM sources WHERE id=?1",
      -1, &ss, NULL);

  for (int i = 0; i < fn; i++) {
    source_trust *t = &T[i];
    snprintf(t->source_id, sizeof t->source_id, "%s", F[i].id);
    t->runs_30d = F[i].n;

    for (int k = 0; k < an; k++)
      if (strcmp(A[k].id, F[i].id) == 0) { t->anomalies_30d = A[k].n; break; }

    const char *last_success = NULL;
    char lsbuf[64] = {0};
    if (ss) {
      sqlite3_reset(ss);
      sqlite3_clear_bindings(ss);
      sqlite3_bind_text(ss, 1, F[i].id, -1, SQLITE_TRANSIENT);
      if (sqlite3_step(ss) == SQLITE_ROW) {
        if (sqlite3_column_type(ss, 0) != SQLITE_NULL) {
          snprintf(lsbuf, sizeof lsbuf, "%s",
                   (const char *)sqlite3_column_text(ss, 0));
          last_success = lsbuf;
        }
        if (sqlite3_column_type(ss, 1) != SQLITE_NULL) {
          time_t qu = parse_ts((const char *)sqlite3_column_text(ss, 1));
          if (qu > now) t->quarantined = 1;
        }
      }
    }

    const src_meta *m = src_meta_get(F[i].id);
    int interval = (m && m->update_interval > 0) ? m->update_interval : 0;

    t->success_rate     = F[i].n > 0 ? (double)F[i].ok / (double)F[i].n : 0.0;
    t->freshness        = freshness_of(last_success, interval, now);
    t->anomaly_rate     = F[i].n > 0
                          ? clamp01((double)t->anomalies_30d / (double)F[i].n)
                          : 0.0;
    t->volume_stability = stability_of(F[i].n, F[i].sum, F[i].sumsq);

    double r = 0.40 * t->success_rate
             + 0.30 * t->freshness
             + 0.20 * (1.0 - t->anomaly_rate)
             + 0.10 * t->volume_stability;
    if (t->quarantined) r = 0.0;

    t->rated       = 1;
    t->reliability = clamp01(r);
    t->grade[0]    = grade_of(t->reliability);
    t->grade[1]    = 0;
  }

  if (ss) sqlite3_finalize(ss);
  free(F);
  free(A);
  if (out_n) *out_n = fn;
  return T;
}

const source_trust *source_trust_find(const source_trust *tbl, int n,
                                      const char *source_id) {
  if (!tbl || !source_id) return NULL;
  for (int i = 0; i < n; i++)
    if (strcmp(tbl[i].source_id, source_id) == 0) return &tbl[i];
  return NULL;
}
