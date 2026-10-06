/* test_retention.c — the log-retention pod deletes what it is configured to
 * and NOTHING else.
 *
 * fetch_log (~216k rows a day), resolved anomalies, their repair rows,
 * terminal alert deliveries and old alert events had no retention at all.
 * The pod that adds it is a DELETE loop on a live database, so every row it
 * must keep is pinned here as carefully as every row it may remove:
 *   - fetch_log: rows past the window go; a source's newest JO_RETAIN_
 *     FETCH_LOG_KEEP rows stay whatever their age (the detection baselines
 *     read the last 20); a row an anomaly still cites stays (FK, and it is
 *     the anomaly's evidence); the window can never drop below the 31 days
 *     source_trust scores on;
 *   - anomalies: only RESOLVED ones past the window, their repair rows first;
 *   - alert deliveries: only terminal ones; alert events: only those with no
 *     delivery still pending;
 *   - intel_items: untouched, always;
 *   - batches: a small JO_RETAIN_BATCH still gets everything (many
 *     statements, not one), and cancel stops it between statements. */
#include "../../collectors/pod/retention_pod.c"

#include <assert.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *err = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &err) != SQLITE_OK) {
    fprintf(stderr, "exec failed: %s\n  %s\n", err ? err : "?", sql);
    assert(0);
  }
}
static long count(db_handle *db, const char *sql) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}

static void seed(db_handle *db) {
  exec_ok(db,
    "DELETE FROM collector_repair; DELETE FROM collector_anomaly;"
    "DELETE FROM fetch_log; DELETE FROM alert_deliveries; DELETE FROM alert_events;"
    "INSERT OR IGNORE INTO sources(id,name,type,category) VALUES"
    " ('UNIT_RET_BUSY','b','api','test'),('UNIT_RET_RARE','r','api','test');");
  /* BUSY: 60 runs 100 days ago, 30 runs 20 days ago, 5 today. */
  exec_ok(db,
    "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM n WHERE i<60)"
    " INSERT INTO fetch_log(source_id,timestamp,status,records_fetched)"
    " SELECT 'UNIT_RET_BUSY',datetime('now','-100 days'),'ok',i FROM n;"
    "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM n WHERE i<30)"
    " INSERT INTO fetch_log(source_id,timestamp,status,records_fetched)"
    " SELECT 'UNIT_RET_BUSY',datetime('now','-20 days'),'ok',i FROM n;"
    "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM n WHERE i<5)"
    " INSERT INTO fetch_log(source_id,status,records_fetched)"
    " SELECT 'UNIT_RET_BUSY','ok',i FROM n;");
  /* RARE: 8 runs, all 200 days old — fewer than KEEP, so all stay. */
  exec_ok(db,
    "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM n WHERE i<8)"
    " INSERT INTO fetch_log(source_id,timestamp,status,records_fetched)"
    " SELECT 'UNIT_RET_RARE',datetime('now','-200 days'),'ok',i FROM n;");
  /* Anomalies: an OPEN one citing BUSY's oldest run; one resolved 200 days
   * ago with a repair row (both go); one resolved 10 days ago (stays). */
  exec_ok(db,
    "INSERT INTO collector_anomaly(id,source_id,fetch_log_id,verdict,created_at)"
    " VALUES (1,'UNIT_RET_BUSY',(SELECT MIN(id) FROM fetch_log),'status_bad',"
    "         datetime('now','-100 days'));"
    "INSERT INTO collector_anomaly(id,source_id,verdict,created_at,resolved_at)"
    " VALUES (2,'UNIT_RET_BUSY','records_drop',datetime('now','-210 days'),"
    "         datetime('now','-200 days')),"
    "        (3,'UNIT_RET_BUSY','records_drop',datetime('now','-12 days'),"
    "         datetime('now','-10 days'));"
    "INSERT INTO collector_repair(anomaly_id,source_id,status,created_at)"
    " VALUES (2,'UNIT_RET_BUSY','rejected',datetime('now','-205 days')),"
    "        (3,'UNIT_RET_BUSY','rejected',datetime('now','-11 days'));");
  /* Alerts. */
  exec_ok(db,
    "INSERT INTO alert_events(id,tenant_id,rule_id,item_uid,matched_at) VALUES"
    " ('ev-old','t','r','u',datetime('now','-400 days')),"
    " ('ev-old-pending','t','r','u',datetime('now','-400 days')),"
    " ('ev-new','t','r','u',datetime('now','-10 days'));"
    "INSERT INTO alert_deliveries(event_id,channel_idx,channel_type,status,attempted_at) VALUES"
    " ('ev-new',0,'webhook','ok',datetime('now','-60 days')),"
    " ('ev-new',1,'webhook','dead',datetime('now','-5 days')),"
    " ('ev-new',2,'webhook','pending',datetime('now','-60 days')),"
    " ('ev-old-pending',0,'webhook','pending',datetime('now','-399 days')),"
    " ('ev-old',0,'webhook','ok',datetime('now','-2 days'));");
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "INSERT INTO intel_items(uid,source_id,title,fetched_at) VALUES"
               " ('UNIT_RET_BUSY|x','UNIT_RET_BUSY','kept',datetime('now','-900 days'))");
  long intel0 = count(&db, "SELECT COUNT(*) FROM intel_items");

  setenv("JO_RETAIN_BATCH", "7", 1);           /* many statements per table */
  seed(&db);
  retain_counts c;
  retain_all(db.h, NULL, &c);
  printf("  deleted fetch_log=%ld repair=%ld anomaly=%ld deliveries=%ld events=%ld\n",
         c.fetch_log, c.repairs, c.anomalies, c.deliveries, c.events);

  /* BUSY had 95 rows: the 60 old ones go except the one the open anomaly
   * cites (the 35 recent ones are inside the window, and include the newest
   * 20). RARE's 8 stay: they ARE its newest 20. */
  assert(count(&db, "SELECT COUNT(*) FROM fetch_log WHERE source_id='UNIT_RET_BUSY'") == 36);
  assert(count(&db, "SELECT COUNT(*) FROM fetch_log WHERE source_id='UNIT_RET_RARE'") == 8);
  assert(count(&db, "SELECT COUNT(*) FROM fetch_log f JOIN collector_anomaly a"
                    " ON a.fetch_log_id=f.id WHERE a.id=1") == 1);
  assert(c.fetch_log == 59);
  /* Anomalies: only #2 (resolved 200 d) and its repair row. */
  assert(count(&db, "SELECT COUNT(*) FROM collector_anomaly WHERE id IN (1,3)") == 2);
  assert(count(&db, "SELECT COUNT(*) FROM collector_anomaly WHERE id=2") == 0);
  assert(count(&db, "SELECT COUNT(*) FROM collector_repair WHERE anomaly_id=3") == 1);
  assert(c.repairs == 1 && c.anomalies == 1);
  /* Alerts: ev-old goes (and its terminal delivery with it), ev-old-pending
   * stays with its pending delivery; ev-new keeps its recent dead delivery
   * and its pending one, loses the 60-day-old ok. */
  assert(count(&db, "SELECT COUNT(*) FROM alert_events WHERE id='ev-old'") == 0);
  assert(count(&db, "SELECT COUNT(*) FROM alert_events WHERE id IN ('ev-old-pending','ev-new')") == 2);
  assert(count(&db, "SELECT COUNT(*) FROM alert_deliveries WHERE event_id='ev-new'") == 2);
  assert(count(&db, "SELECT COUNT(*) FROM alert_deliveries WHERE status='pending'") == 2);
  assert(count(&db, "SELECT COUNT(*) FROM alert_deliveries WHERE event_id='ev-old'") == 0);
  assert(c.events == 1 && c.deliveries == 2);
  assert(count(&db, "SELECT COUNT(*) FROM intel_items") == intel0 && "intel_items touched");
  /* A second pass finds nothing more. */
  retain_all(db.h, NULL, &c);
  assert(c.fetch_log == 0 && c.repairs == 0 && c.anomalies == 0 &&
         c.deliveries == 0 && c.events == 0);
  printf("  defaults: kept everything they must, idempotent on a second pass\n");

  /* The 31-day floor: asking for 5 days must not touch the 20-day-old rows. */
  seed(&db);
  setenv("JO_RETAIN_FETCH_LOG_DAYS", "5", 1);
  setenv("JO_RETAIN_FETCH_LOG_KEEP", "1", 1);
  retain_all(db.h, NULL, &c);
  long busy = count(&db, "SELECT COUNT(*) FROM fetch_log WHERE source_id='UNIT_RET_BUSY'");
  printf("  JO_RETAIN_FETCH_LOG_DAYS=5 -> floored to 31: BUSY keeps %ld\n", busy);
  assert(busy == 36);
  /* KEEP=1: RARE keeps only its newest row. */
  assert(count(&db, "SELECT COUNT(*) FROM fetch_log WHERE source_id='UNIT_RET_RARE'") == 1);

  /* 0 = keep forever. */
  seed(&db);
  setenv("JO_RETAIN_FETCH_LOG_DAYS", "0", 1);
  setenv("JO_RETAIN_ANOMALY_DAYS", "0", 1);
  setenv("JO_RETAIN_ALERT_DELIVERY_DAYS", "0", 1);
  setenv("JO_RETAIN_ALERT_EVENT_DAYS", "0", 1);
  retain_all(db.h, NULL, &c);
  assert(c.fetch_log == 0 && c.repairs == 0 && c.anomalies == 0 &&
         c.deliveries == 0 && c.events == 0);
  assert(count(&db, "SELECT COUNT(*) FROM fetch_log") == 103);

  /* Cancel stops between statements: at most one window of fetch_log. */
  unsetenv("JO_RETAIN_FETCH_LOG_DAYS");
  unsetenv("JO_RETAIN_FETCH_LOG_KEEP");
  volatile int cancel = 1;
  long fl = retain_fetch_log(db.h, 35, 20, 7, &cancel);
  printf("  cancelled pass deleted %ld fetch_log row(s) (window 7)\n", fl);
  assert(fl >= 0 && fl <= 7);

  db_close(&db);
  printf("test_retention: ok\n");
  return 0;
}
