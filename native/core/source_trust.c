#include "source_trust.h"
#include "source_registry.h"
#include "../third_party/sqlite3.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <pthread.h>

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

/* ONE QUERY, NOT A QUERY PLUS A NESTED LOOP. This used to run the fetch_log
 * GROUP BY, then the anomaly GROUP BY into a second array, then join the two
 * with a strcmp loop over every (fetch_log row, anomaly row) pair — O(fn x an)
 * — and then a per-source SELECT against `sources`. The join is SQLite's job:
 * both aggregates and the sources row arrive on one row per source. Rows are
 * sorted by id below so source_trust_find() can bsearch.
 *
 * collector_anomaly may be absent (a fixture database booted without
 * schema.sql); the old code tolerated that by skipping the anomaly half, so
 * this degrades the same way: without the anomaly join every anomalies_30d is
 * 0. */
#define TQ_FETCH_AGG                                                          \
  "(SELECT source_id,COUNT(*) n,"                                             \
  "        SUM(CASE WHEN status='ok' THEN 1 ELSE 0 END) ok,"                  \
  "        SUM(COALESCE(records_fetched,0)) sm,"                              \
  "        SUM(CAST(COALESCE(records_fetched,0) AS REAL)"                     \
  "            *COALESCE(records_fetched,0)) sq"                              \
  " FROM fetch_log WHERE timestamp >= datetime('now','-30 days')"             \
  " GROUP BY source_id) f "
static const char *TQ_FULL =
  "SELECT f.source_id,f.n,f.ok,f.sm,f.sq,COALESCE(a.n,0),"
  "       s.last_success,s.quarantined_until FROM " TQ_FETCH_AGG
  "LEFT JOIN (SELECT source_id,COUNT(*) n FROM collector_anomaly"
  "           WHERE created_at >= datetime('now','-30 days')"
  "           GROUP BY source_id) a ON a.source_id=f.source_id "
  "LEFT JOIN sources s ON s.id=f.source_id";
static const char *TQ_NO_ANOMALY =
  "SELECT f.source_id,f.n,f.ok,f.sm,f.sq,0,"
  "       s.last_success,s.quarantined_until FROM " TQ_FETCH_AGG
  "LEFT JOIN sources s ON s.id=f.source_id";

static int trust_cmp(const void *a, const void *b) {
  return strcmp(((const source_trust *)a)->source_id,
                ((const source_trust *)b)->source_id);
}

source_trust *source_trust_load(db_handle *db, int *out_n) {
  if (out_n) *out_n = 0;
  if (!db || !db->h) return NULL;

  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h, TQ_FULL, -1, &s, NULL) != SQLITE_OK &&
      sqlite3_prepare_v2(db->h, TQ_NO_ANOMALY, -1, &s, NULL) != SQLITE_OK)
    return NULL;
  int cap = 64, n = 0;
  source_trust *T = calloc((size_t)cap, sizeof *T);
  if (!T) { sqlite3_finalize(s); return NULL; }
  time_t now = time(NULL);

  while (sqlite3_step(s) == SQLITE_ROW) {
    const unsigned char *sid = sqlite3_column_text(s, 0);
    if (!sid) continue;
    if (n == cap) {
      int nc = cap * 2;
      source_trust *nt = realloc(T, (size_t)nc * sizeof *T);
      if (!nt) { free(T); sqlite3_finalize(s); return NULL; }
      memset(nt + cap, 0, (size_t)(nc - cap) * sizeof *T);
      T = nt; cap = nc;
    }
    source_trust *t = &T[n++];
    snprintf(t->source_id, sizeof t->source_id, "%s", (const char *)sid);
    long   runs  = (long)sqlite3_column_int64(s, 1);
    long   ok    = (long)sqlite3_column_int64(s, 2);
    double sum   = sqlite3_column_double(s, 3);
    double sumsq = sqlite3_column_double(s, 4);
    t->runs_30d      = runs;
    t->anomalies_30d = (long)sqlite3_column_int64(s, 5);

    const char *last_success = NULL;
    char lsbuf[64] = {0};
    if (sqlite3_column_type(s, 6) != SQLITE_NULL) {
      snprintf(lsbuf, sizeof lsbuf, "%s", (const char *)sqlite3_column_text(s, 6));
      last_success = lsbuf;
    }
    if (sqlite3_column_type(s, 7) != SQLITE_NULL) {
      time_t qu = parse_ts((const char *)sqlite3_column_text(s, 7));
      if (qu > now) t->quarantined = 1;
    }

    const src_meta *m = src_meta_get(t->source_id);
    int interval = (m && m->update_interval > 0) ? m->update_interval : 0;

    t->success_rate     = runs > 0 ? (double)ok / (double)runs : 0.0;
    t->freshness        = freshness_of(last_success, interval, now);
    t->anomaly_rate     = runs > 0
                          ? clamp01((double)t->anomalies_30d / (double)runs)
                          : 0.0;
    t->volume_stability = stability_of(runs, sum, sumsq);

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
  sqlite3_finalize(s);
  qsort(T, (size_t)n, sizeof *T, trust_cmp);
  if (out_n) *out_n = n;
  return T;
}

const source_trust *source_trust_find(const source_trust *tbl, int n,
                                      const char *source_id) {
  if (!tbl || !source_id || n <= 0) return NULL;
  source_trust key;
  snprintf(key.source_id, sizeof key.source_id, "%s", source_id);
  return bsearch(&key, tbl, (size_t)n, sizeof *tbl, trust_cmp);
}

/* ── cached scalar for the scheduler ─────────────────────────────────────
 *
 * The dispatcher asks for a score once per due source per second. The table
 * behind it is a 30-day GROUP BY over fetch_log (~216k rows a day, so ~6.5 M
 * rows in the window), and it used to be REBUILT ON THE DISPATCHER THREAD,
 * inside the lock every lookup takes: once per TTL the whole fleet's
 * scheduling stopped for as long as that query ran.
 *
 * Now the dispatcher only reads. A stale (or missing) table starts ONE
 * background rebuild on its own connection, which swaps the finished table in
 * with a pointer exchange under the lock; lookups — a bsearch whose answer is
 * copied out under the same lock — never wait on SQL. Until the first rebuild
 * lands every source reads as unrated (-1), which sched_priority() places
 * mid-band: no evidence yet, rather than invented evidence.
 *
 * A failed load keeps the previous table (stale is better than
 * unrated-for-everyone); the TTL clock restarts either way, so a failing query
 * is retried once per TTL rather than every second. */
static pthread_mutex_t g_score_mu = PTHREAD_MUTEX_INITIALIZER;
static source_trust   *g_score_tbl;
static int             g_score_n;
static time_t          g_score_at;          /* when the last rebuild STARTED */
static int             g_score_busy;        /* a rebuild is in flight        */
static long            g_score_builds;      /* tables installed (tests)      */

typedef struct { char path[1024]; } trust_job;

static void trust_install(source_trust *t, int n) {
  pthread_mutex_lock(&g_score_mu);
  source_trust *old = NULL;
  if (t) { old = g_score_tbl; g_score_tbl = t; g_score_n = n; g_score_builds++; }
  g_score_busy = 0;
  pthread_mutex_unlock(&g_score_mu);
  free(old);           /* no reader holds it: lookups copy out under the lock */
}

static void *trust_rebuild_thread(void *arg) {
  trust_job *j = arg;
  db_handle own = {0};
  source_trust *t = NULL;
  int n = 0;
  if (db_attach(&own, j->path) == 0) {
    t = source_trust_load(&own, &n);
    db_close(&own);
  } else {
    fprintf(stderr, "[trust] rebuild: cannot open %s; keeping the previous "
                    "table\n", j->path);
  }
  free(j);
  trust_install(t, n);
  return NULL;
}

/* Start a rebuild from the database `db` is attached to. The caller has
 * already claimed g_score_busy. */
static void trust_rebuild_async(db_handle *db) {
  const char *f = (db && db->h) ? sqlite3_db_filename(db->h, "main") : NULL;
  trust_job *j = (f && *f) ? malloc(sizeof *j) : NULL;
  if (j) {
    snprintf(j->path, sizeof j->path, "%s", f);
    pthread_t th;
    if (pthread_create(&th, NULL, trust_rebuild_thread, j) == 0) {
      pthread_detach(th);
      return;
    }
    free(j);
  }
  /* An in-memory database has no file a second connection could open, and a
   * failed thread spawn has nowhere else to run: build it here, once, rather
   * than leave every source unrated for good. */
  int n = 0;
  source_trust *t = source_trust_load(db, &n);
  trust_install(t, n);
}

double source_trust_score(db_handle *db, const char *source_id) {
  if (!source_id) return -1.0;
  long ttl = 300;
  const char *e = getenv("JO_TRUST_CACHE_SEC");
  if (e && *e) { ttl = atol(e); if (ttl < 1) ttl = 1; }
  time_t now = time(NULL);
  int kick = 0;
  pthread_mutex_lock(&g_score_mu);
  if (!g_score_busy && (!g_score_tbl || now - g_score_at >= ttl)) {
    g_score_busy = 1;
    g_score_at = now;
    kick = 1;
  }
  const source_trust *t = source_trust_find(g_score_tbl, g_score_n, source_id);
  double r = (t && t->rated) ? t->reliability : -1.0;
  pthread_mutex_unlock(&g_score_mu);
  if (kick) trust_rebuild_async(db);
  return r;
}

long source_trust_builds(void) {
  pthread_mutex_lock(&g_score_mu);
  long n = g_score_builds;
  pthread_mutex_unlock(&g_score_mu);
  return n;
}
