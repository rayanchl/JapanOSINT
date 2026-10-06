/* collectors/pod/retention_pod.c — age out OPERATIONAL LOGS, in small bites.
 *
 * WHY. Five tables only ever grew. fetch_log gets a row per collector run —
 * ~216k a day at the current fleet, so ~79 M a year — and nothing deleted any
 * of it; neither were resolved anomalies, the repair attempts behind them,
 * terminal alert deliveries, or alert events from years ago. Every one of
 * them is read by an index that gets deeper by the day (source_trust's 30-day
 * GROUP BY, the anomaly baselines, /api/status), and the WAL pod has more to
 * fold back every time one is scanned.
 *
 * WHAT IT NEVER TOUCHES. intel_items, or anything else that is collected
 * data. House rule 2 is about what a source gave back; these tables are the
 * platform's record of its own runs, and only their age-out is configurable
 * here. Every table also has an opt-out: a value of 0 keeps it forever.
 *
 *   JO_RETAIN_FETCH_LOG_DAYS       35   floor 31 — source_trust.c scores the
 *                                       trailing 30 days of fetch_log, so a
 *                                       shorter window would quietly re-grade
 *                                       every source on less evidence
 *   JO_RETAIN_FETCH_LOG_KEEP       20   newest rows kept per source whatever
 *                                       their age: the records/duration
 *                                       baselines in maint_detect.c and the
 *                                       repair pod read the last 20 runs, and a
 *                                       weekly source has only ~5 in 35 days
 *   JO_RETAIN_ANOMALY_DAYS         90   resolved anomalies (and their repair
 *                                       rows), by resolved_at; floor 31 for
 *                                       the same 30-day trust window. Open
 *                                       anomalies are never deleted.
 *   JO_RETAIN_ALERT_DELIVERY_DAYS  30   deliveries in a terminal state
 *                                       (ok/dead/skipped/failed); pending ones
 *                                       are never deleted
 *   JO_RETAIN_ALERT_EVENT_DAYS    365   alert events (the inbox's history)
 *                                       with no delivery still pending
 *   JO_RETAIN_BATCH              5000   rows per DELETE statement
 *
 * WHY BATCHED. Each DELETE is its own autocommit transaction over at most
 * JO_RETAIN_BATCH rows (fetch_log: an id window of that width), with a short
 * pause between them, so the write lock is held for milliseconds at a time and
 * a collector's emit() never waits behind a retention pass for longer than one
 * batch. The first pass over a year of history is many statements, not one
 * long transaction. The loop stops early on ctx->cancel (shutdown, or the
 * scheduler's deadline) and the next hourly run picks up where it left off,
 * because the predicates are the state.
 *
 * FOREIGN KEYS. collector_anomaly.fetch_log_id references fetch_log and
 * collector_repair.anomaly_id references collector_anomaly, with
 * foreign_keys=ON. So fetch_log rows an anomaly still cites are kept (they are
 * the evidence the anomaly points at), repair rows go before their anomaly,
 * and the child-side indexes the FK check needs are created below — without
 * idx_anomaly_fetch_log every deleted fetch_log row would scan
 * collector_anomaly to prove nothing references it.
 *
 * `_maint`: emits no intel_items. It runs in the scheduler's maintenance lane,
 * never behind (or in front of) a collector. */
#include "source.h"
#include "core/db.h"
#include "third_party/sqlite3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RETAIN_PAUSE_MS 20       /* writers queued behind a batch run here */

static int env_days(const char *k, int dflt, int floor_days) {
  const char *v = getenv(k);
  int n = (v && *v) ? atoi(v) : dflt;
  if (n <= 0) return 0;                        /* 0 (or nonsense): keep forever */
  if (n < floor_days) {
    fprintf(stderr, "[retention] %s=%d is below the %d-day floor; using %d\n",
            k, n, floor_days, floor_days);
    n = floor_days;
  }
  return n;
}

static int env_pos(const char *k, int dflt) {
  const char *v = getenv(k);
  int n = (v && *v) ? atoi(v) : dflt;
  return n > 0 ? n : dflt;
}

static int cancelled(const volatile int *c) {
  return c && __atomic_load_n((const int *)c, __ATOMIC_ACQUIRE);
}

static void pause_ms(int ms) {
  struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
  nanosleep(&ts, NULL);
}

static void ensure_indexes(sqlite3 *h) {
  static const char *IDX[] = {
    "CREATE INDEX IF NOT EXISTS idx_anomaly_fetch_log ON collector_anomaly(fetch_log_id)"
    " WHERE fetch_log_id IS NOT NULL",
    "CREATE INDEX IF NOT EXISTS idx_anomaly_resolved_at ON collector_anomaly(resolved_at)"
    " WHERE resolved_at IS NOT NULL",
    "CREATE INDEX IF NOT EXISTS idx_alert_deliveries_attempted ON alert_deliveries(attempted_at)",
    "CREATE INDEX IF NOT EXISTS idx_alert_events_matched ON alert_events(matched_at)",
  };
  for (size_t i = 0; i < sizeof IDX / sizeof IDX[0]; i++) {
    char *err = NULL;
    if (sqlite3_exec(h, IDX[i], NULL, NULL, &err) != SQLITE_OK) {
      /* A missing table (a fixture without schema.sql) is not this pod's
       * problem; the DELETE against it will fail the same way and count 0. */
      sqlite3_free(err);
    }
  }
}

/* Run `sql` (binds: ?1 = "-N days", ?2 = batch) until a statement deletes
 * fewer than `batch` rows. Returns rows deleted, or -1 if the statement could
 * not be prepared (table absent). */
static long delete_batched(sqlite3 *h, const char *sql, int days, int batch,
                           const volatile int *cancel) {
  sqlite3_stmt *s;
  if (sqlite3_prepare_v2(h, sql, -1, &s, NULL) != SQLITE_OK) return -1;
  char mod[32];
  snprintf(mod, sizeof mod, "-%d days", days);
  long total = 0;
  for (;;) {
    sqlite3_bind_text(s, 1, mod, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 2, batch);
    int rc = sqlite3_step(s);
    sqlite3_reset(s);
    if (rc != SQLITE_DONE) {
      fprintf(stderr, "[retention] delete failed: %s\n", sqlite3_errmsg(h));
      break;
    }
    int n = sqlite3_changes(h);
    total += n;
    if (n < batch || cancelled(cancel)) break;
    pause_ms(RETAIN_PAUSE_MS);
  }
  sqlite3_finalize(s);
  return total;
}

/* fetch_log, walked in id windows of `batch` ids from the oldest row up to
 * the highest id older than the cutoff. fetch_log ids are AUTOINCREMENT and
 * timestamp defaults to datetime('now'), so id order is (nearly) time order
 * and the walk is short; the predicate still checks the timestamp itself. The
 * window bounds each statement by construction even where most rows in it are
 * protected (cited by an anomaly, or among a source's newest `keep`), which a
 * `LIMIT batch` over the candidates could not — those protected rows would be
 * re-scanned by every batch. */
static long retain_fetch_log(sqlite3 *h, int days, int keep, int batch,
                             const volatile int *cancel) {
  if (days <= 0) return 0;
  if (keep < 1) keep = 1;
  char mod[32];
  snprintf(mod, sizeof mod, "-%d days", days);
  sqlite3_stmt *s;
  long long lo = 0, hi = 0;
  /* MAX(id) over the old rows, not "the id of the newest old row": rows
   * written in the same second tie on timestamp, and a backfilled row can
   * carry an old timestamp under a new id. An index-only walk of idx_log_ts. */
  if (sqlite3_prepare_v2(h, "SELECT (SELECT MIN(id) FROM fetch_log),"
        " (SELECT MAX(id) FROM fetch_log WHERE timestamp < datetime('now',?1))",
        -1, &s, NULL) != SQLITE_OK)
    return -1;
  sqlite3_bind_text(s, 1, mod, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 1) != SQLITE_NULL) {
    lo = sqlite3_column_int64(s, 0) - 1;
    hi = sqlite3_column_int64(s, 1);
  }
  sqlite3_finalize(s);
  if (hi <= lo) return 0;

  if (sqlite3_prepare_v2(h,
        "DELETE FROM fetch_log WHERE id IN ("
        " SELECT f.id FROM fetch_log f"
        " WHERE f.id > ?1 AND f.id <= ?2 AND f.timestamp < datetime('now',?3)"
        "   AND NOT EXISTS (SELECT 1 FROM collector_anomaly a WHERE a.fetch_log_id=f.id)"
        "   AND f.id < COALESCE((SELECT g.id FROM fetch_log g WHERE g.source_id=f.source_id"
        "                        ORDER BY g.id DESC LIMIT 1 OFFSET ?4), 0))",
        -1, &s, NULL) != SQLITE_OK) {
    /* No collector_anomaly (a bare fixture): nothing can cite a row. */
    if (sqlite3_prepare_v2(h,
          "DELETE FROM fetch_log WHERE id IN ("
          " SELECT f.id FROM fetch_log f"
          " WHERE f.id > ?1 AND f.id <= ?2 AND f.timestamp < datetime('now',?3)"
          "   AND f.id < COALESCE((SELECT g.id FROM fetch_log g WHERE g.source_id=f.source_id"
          "                        ORDER BY g.id DESC LIMIT 1 OFFSET ?4), 0))",
          -1, &s, NULL) != SQLITE_OK)
      return -1;
  }
  long total = 0;
  for (long long a = lo; a < hi; a += batch) {
    long long b = a + batch < hi ? a + batch : hi;
    sqlite3_bind_int64(s, 1, a);
    sqlite3_bind_int64(s, 2, b);
    sqlite3_bind_text(s, 3, mod, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s, 4, keep - 1);
    int rc = sqlite3_step(s);
    sqlite3_reset(s);
    if (rc != SQLITE_DONE) {
      fprintf(stderr, "[retention] fetch_log delete failed: %s\n", sqlite3_errmsg(h));
      break;
    }
    int n = sqlite3_changes(h);
    total += n;
    if (cancelled(cancel)) break;
    if (n > 0) pause_ms(RETAIN_PAUSE_MS);
  }
  sqlite3_finalize(s);
  return total;
}

typedef struct {
  long fetch_log, repairs, anomalies, deliveries, events;
} retain_counts;

static void retain_all(sqlite3 *h, const volatile int *cancel, retain_counts *c) {
  memset(c, 0, sizeof *c);
  int batch = env_pos("JO_RETAIN_BATCH", 5000);
  int fl_days = env_days("JO_RETAIN_FETCH_LOG_DAYS", 35, 31);
  int fl_keep = env_pos("JO_RETAIN_FETCH_LOG_KEEP", 20);
  int an_days = env_days("JO_RETAIN_ANOMALY_DAYS", 90, 31);
  int dl_days = env_days("JO_RETAIN_ALERT_DELIVERY_DAYS", 30, 1);
  int ev_days = env_days("JO_RETAIN_ALERT_EVENT_DAYS", 365, 1);
  ensure_indexes(h);

  if (an_days > 0 && !cancelled(cancel)) {
    /* Repair rows first: they reference the anomaly. */
    c->repairs = delete_batched(h,
      "DELETE FROM collector_repair WHERE id IN ("
      " SELECT r.id FROM collector_repair r JOIN collector_anomaly a ON a.id=r.anomaly_id"
      " WHERE a.resolved_at IS NOT NULL AND a.resolved_at < datetime('now',?1)"
      " LIMIT ?2)", an_days, batch, cancel);
    if (!cancelled(cancel))
      c->anomalies = delete_batched(h,
        "DELETE FROM collector_anomaly WHERE id IN ("
        " SELECT a.id FROM collector_anomaly a"
        " WHERE a.resolved_at IS NOT NULL AND a.resolved_at < datetime('now',?1)"
        "   AND NOT EXISTS (SELECT 1 FROM collector_repair r WHERE r.anomaly_id=a.id)"
        " LIMIT ?2)", an_days, batch, cancel);
  }
  /* After the anomalies, so rows only a now-deleted anomaly cited can go. */
  if (!cancelled(cancel))
    c->fetch_log = retain_fetch_log(h, fl_days, fl_keep, batch, cancel);
  if (ev_days > 0 && !cancelled(cancel))
    c->events = delete_batched(h,
      "DELETE FROM alert_events WHERE id IN ("
      " SELECT e.id FROM alert_events e WHERE e.matched_at < datetime('now',?1)"
      "   AND NOT EXISTS (SELECT 1 FROM alert_deliveries d"
      "                   WHERE d.event_id=e.id AND d.status='pending')"
      " LIMIT ?2)", ev_days, batch, cancel);
  if (dl_days > 0 && !cancelled(cancel))
    /* Terminal deliveries past the window, and terminal deliveries of an
     * event the step above (or a rule delete) removed. */
    c->deliveries = delete_batched(h,
      "DELETE FROM alert_deliveries WHERE id IN ("
      " SELECT d.id FROM alert_deliveries d"
      " WHERE d.status IN ('ok','dead','skipped','failed')"
      "   AND ((d.attempted_at IS NOT NULL AND d.attempted_at < datetime('now',?1))"
      "        OR NOT EXISTS (SELECT 1 FROM alert_events e WHERE e.id=d.event_id))"
      " LIMIT ?2)", dl_days, batch, cancel);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  if (!ctx || !ctx->db || !ctx->db->h) return -1;
  retain_counts c;
  retain_all(ctx->db->h, ctx->cancel, &c);
  if (c.fetch_log > 0 || c.repairs > 0 || c.anomalies > 0 || c.deliveries > 0 ||
      c.events > 0 || cancelled(ctx->cancel))
    fprintf(stderr, "[retention] deleted fetch_log=%ld collector_repair=%ld "
                    "collector_anomaly=%ld alert_deliveries=%ld alert_events=%ld%s\n",
            c.fetch_log > 0 ? c.fetch_log : 0, c.repairs > 0 ? c.repairs : 0,
            c.anomalies > 0 ? c.anomalies : 0, c.deliveries > 0 ? c.deliveries : 0,
            c.events > 0 ? c.events : 0,
            cancelled(ctx->cancel) ? " (cancelled; the next run continues)" : "");
  return 0;
}

static const source_def retention_def = {
  .id = "log-retention", .collector = "_maint",
  .name = "Operational Log Retention (fetch_log, anomalies, alert deliveries)",
  .name_ja = "運用ログの保持期間管理",
  .update_interval_sec = 3600, .run = run,
  .category = "maintenance" };
REGISTER_SOURCE(retention_def)
