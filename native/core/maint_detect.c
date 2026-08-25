/* core/maint_detect.c — see maint_detect.h. Inline sqlite3 prepared statements
 * via db->h (pattern: scheduler.c:is_search_only). */
#include "maint_detect.h"
#include "../third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int env_int(const char *k, int dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  int n = atoi(v);
  return n > 0 ? n : dflt;
}
static double env_double(const char *k, double dflt) {
  const char *v = getenv(k);
  if (!v || !*v) return dflt;
  double d = atof(v);
  return d > 0.0 ? d : dflt;
}

long fetch_log_write(db_handle *db, const char *source_id, const char *status,
                     int records, long duration_ms, const char *error) {
  if (!db || !db->h || !source_id) return -1;
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(db->h,
        "INSERT INTO fetch_log (source_id,status,records_fetched,duration_ms,error)"
        " VALUES (?1,?2,?3,?4,?5)", -1, &s, NULL) != SQLITE_OK)
    return -1;
  sqlite3_bind_text(s, 1, source_id, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 2, status ? status : "ok", -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(s, 3, records);
  sqlite3_bind_int64(s, 4, (sqlite3_int64)duration_ms);
  if (error && *error) sqlite3_bind_text(s, 5, error, -1, SQLITE_TRANSIENT);
  else sqlite3_bind_null(s, 5);
  long id = (sqlite3_step(s) == SQLITE_DONE)
              ? (long)sqlite3_last_insert_rowid(db->h) : -1;
  sqlite3_finalize(s);
  return id;
}

/* Mean records over recent healthy fetch_log rows. Returns 1 and sets *out when
 * a baseline exists (>=1 ok row with records>0), else 0. */
static int records_baseline(sqlite3 *h, const char *source_id, double *out) {
  sqlite3_stmt *s; int have = 0;
  if (sqlite3_prepare_v2(h,
        "SELECT AVG(records_fetched) FROM (SELECT records_fetched FROM fetch_log"
        " WHERE source_id=?1 AND status='ok' AND records_fetched>0"
        " ORDER BY id DESC LIMIT 20)", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, source_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s,0) != SQLITE_NULL) {
      *out = sqlite3_column_double(s, 0);
      have = (*out > 0.0);
    }
    sqlite3_finalize(s);
  }
  return have;
}

/* How many runs this source has logged, and whether ANY of them ever produced
 * a record. Returns the run count; sets *ever to 1 if a record was ever seen.
 *
 * This is the gap records_baseline() cannot see. That function needs at least
 * one ok row with records>0 to compute a mean, so the records_drop verdict —
 * the one check designed to catch "used to return rows, now returns none" — is
 * structurally blind to a source that never returned rows in the FIRST place.
 * Measured on the live runtime DB: 443 sources had 10+ runs and zero records
 * ever, and 249 of those reported status='ok' on 100% of runs (certstream-jp
 * 84/84, dam-water-level 78/78, flightradar-jp 77/77, censys-japan 67/67).
 * They are invisible to /api/status and to anomaly detection simultaneously,
 * which is the whole population of credential-gated, dead-path and
 * empty-by-construction collectors. */
static long run_history(sqlite3 *h, const char *source_id, int *ever) {
  sqlite3_stmt *s;
  long runs = 0;
  *ever = 0;
  if (sqlite3_prepare_v2(h,
        "SELECT COUNT(*), COALESCE(MAX(records_fetched),0) FROM fetch_log"
        " WHERE source_id=?1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, source_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW) {
      runs  = (long)sqlite3_column_int64(s, 0);
      *ever = sqlite3_column_int(s, 1) > 0;
    }
    sqlite3_finalize(s);
  }
  return runs;
}

/* Mean duration over recent healthy runs of THIS source. A flat threshold
 * treats "slow" as a defect, but slowness is a property of the source: a
 * collector that walks 47 prefectures or 40 open-data portals is legitimately
 * 60-280 s every single run, and flagging it hourly is noise that buries the
 * sources that really did change. Returns 1 and sets *out when a baseline
 * exists (>=3 ok rows, so one cold start can't set it). */
static int duration_baseline(sqlite3 *h, const char *source_id, double *out) {
  sqlite3_stmt *s; int have = 0;
  if (sqlite3_prepare_v2(h,
        "SELECT AVG(duration_ms), COUNT(*) FROM (SELECT duration_ms FROM fetch_log"
        " WHERE source_id=?1 AND status='ok' AND duration_ms>0"
        " ORDER BY id DESC LIMIT 20)", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, source_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s,0) != SQLITE_NULL) {
      *out = sqlite3_column_double(s, 0);
      have = (*out > 0.0 && sqlite3_column_int(s, 1) >= 3);
    }
    sqlite3_finalize(s);
  }
  return have;
}

static int has_open_anomaly(sqlite3 *h, const char *source_id) {
  sqlite3_stmt *s; int open = 0;
  if (sqlite3_prepare_v2(h,
        "SELECT 1 FROM collector_anomaly WHERE source_id=?1 AND resolved_at IS NULL"
        " LIMIT 1", -1, &s, NULL) == SQLITE_OK) {
    sqlite3_bind_text(s, 1, source_id, -1, SQLITE_TRANSIENT);
    open = (sqlite3_step(s) == SQLITE_ROW);
    sqlite3_finalize(s);
  }
  return open;
}

static void insert_anomaly(sqlite3 *h, const char *source_id, long fetch_log_id,
                           const char *verdict, const char *reason,
                           const char *evidence) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h,
        "INSERT INTO collector_anomaly (source_id,fetch_log_id,verdict,reason,evidence)"
        " VALUES (?1,?2,?3,?4,?5)", -1, &s, NULL) != SQLITE_OK)
    return;
  sqlite3_bind_text(s, 1, source_id, -1, SQLITE_TRANSIENT);
  if (fetch_log_id >= 0) sqlite3_bind_int64(s, 2, (sqlite3_int64)fetch_log_id);
  else sqlite3_bind_null(s, 2);
  sqlite3_bind_text(s, 3, verdict, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 4, reason, -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(s, 5, evidence, -1, SQLITE_TRANSIENT);
  sqlite3_step(s);
  sqlite3_finalize(s);
  fprintf(stderr, "[detect] anomaly opened: %s verdict=%s — %s\n",
          source_id, verdict, reason ? reason : "");
}

void anomaly_detect(db_handle *db, const char *source_id, long fetch_log_id,
                    const char *status, int records, long duration_ms) {
  if (!db || !db->h || !source_id) return;
  sqlite3 *h = db->h;

  const char *verdict = NULL, *reason = NULL;
  char reason_buf[256], evidence_buf[256];

  int errored = !(status && strcmp(status, "ok") == 0);
  if (errored) {
    verdict = "status_bad";
    snprintf(reason_buf, sizeof reason_buf, "run errored (status=%s)",
             status ? status : "?");
    reason = reason_buf;
    snprintf(evidence_buf, sizeof evidence_buf,
             "{\"status\":\"%s\",\"duration_ms\":%ld}",
             status ? status : "?", duration_ms);
  } else {
    double mean = 0.0;
    int have_baseline = records_baseline(h, source_id, &mean);
    double drop_frac = env_double("DETECT_DROP_FRAC", 0.5);
    long dur_outlier = env_int("DETECT_DURATION_OUTLIER_MS", 10000);
    if (have_baseline && (double)records < mean * (1.0 - drop_frac)) {
      verdict = "records_drop";
      snprintf(reason_buf, sizeof reason_buf,
               "records %d below %.0f%% of baseline mean %.1f",
               records, (1.0 - drop_frac) * 100.0, mean);
      reason = reason_buf;
      snprintf(evidence_buf, sizeof evidence_buf,
               "{\"records\":%d,\"baseline_mean\":%.2f,\"drop_frac\":%.2f}",
               records, mean, drop_frac);
    } else if (!have_baseline && records == 0) {
      /* The cohort records_drop cannot reach: never produced a record, so
       * there is no baseline to fall below. Reported once the run count passes
       * a threshold, so a genuinely new source is not flagged on its first few
       * ticks. Reporting "ok" forever for a source that has never returned
       * anything is the same silent-empty the house rules forbid, one level up:
       * the FLEET asserting health it never verified. */
      int ever = 0;
      long runs = run_history(h, source_id, &ever);
      long need = env_int("DETECT_BARREN_RUNS", 10);
      if (!ever && runs >= need) {
        verdict = "never_produced";
        snprintf(reason_buf, sizeof reason_buf,
                 "%ld runs, never produced a record (no baseline can form)", runs);
        reason = reason_buf;
        snprintf(evidence_buf, sizeof evidence_buf,
                 "{\"runs\":%ld,\"records_ever\":0,\"threshold\":%ld}",
                 runs, need);
      }
    }
    if (!verdict) {
      /* Prefer the source's OWN history: flag a run that is far slower than
       * this collector normally is. Only fall back to the flat threshold while
       * no baseline exists yet. */
      double dmean = 0.0;
      double factor = env_double("DETECT_DURATION_FACTOR", 3.0);
      int have_dur = duration_baseline(h, source_id, &dmean);
      long limit = have_dur ? (long)(dmean * factor) : dur_outlier;
      if (have_dur && limit < dur_outlier) limit = dur_outlier;
      if (duration_ms > limit) {
        verdict = "duration_outlier";
        if (have_dur)
          snprintf(reason_buf, sizeof reason_buf,
                   "duration %ldms exceeds %.1fx its own %.0fms baseline",
                   duration_ms, factor, dmean);
        else
          snprintf(reason_buf, sizeof reason_buf,
                   "duration %ldms exceeds %ldms threshold (no baseline yet)",
                   duration_ms, limit);
        reason = reason_buf;
        snprintf(evidence_buf, sizeof evidence_buf,
                 "{\"duration_ms\":%ld,\"limit_ms\":%ld,\"baseline_ms\":%.1f}",
                 duration_ms, limit, dmean);
      }
    }
  }

  if (!verdict) return;
  /* Dedup: never pile a second open anomaly on a source already flagged. */
  if (has_open_anomaly(h, source_id)) return;
  insert_anomaly(h, source_id, fetch_log_id, verdict, reason, evidence_buf);
}
