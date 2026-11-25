/* tests/unit/test_ops_db_modules.c — core/maint_detect (Stage 0/1 anomaly
 * detection), core/metricsapi (Prometheus exposition) and core/miscapi
 * (source row / schedule / logs / layers / follow) driven against the scratch
 * database, no socket.  Reaches them through their headers, so run.sh links
 * the common object (nothing here is excluded). */
#include "../../core/db.h"
#include "../../core/maint_detect.h"
#include "../../core/metricsapi.h"
#include "../../core/miscapi.h"
#include "../../third_party/cJSON.h"
#include "../../third_party/sqlite3.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}
static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", sql);
    exit(1);
  }
}
static long scalar(db_handle *db, const char *sql) {
  sqlite3_stmt *s;
  long v = -999;
  if (sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK && sqlite3_step(s) == SQLITE_ROW)
    v = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}
static void add_source(db_handle *db, const char *id) {
  char sql[300];
  snprintf(sql, sizeof sql,
           "INSERT OR IGNORE INTO sources(id,name,type,category) VALUES('%s','n-%s','api','c')", id, id);
  exec_ok(db, sql);
}
static long anomalies(db_handle *db, const char *sid, const char *verdict) {
  char sql[300];
  snprintf(sql, sizeof sql,
           "SELECT COUNT(*) FROM collector_anomaly WHERE source_id='%s' AND verdict='%s'", sid, verdict);
  return scalar(db, sql);
}
static long anomalies_any(db_handle *db, const char *sid) {
  char sql[300];
  snprintf(sql, sizeof sql, "SELECT COUNT(*) FROM collector_anomaly WHERE source_id='%s'", sid);
  return scalar(db, sql);
}

static void t_maint(db_handle *db) {
  printf("-- maint_detect\n");
  ok(fetch_log_write(NULL, "x", "ok", 1, 1, NULL) == -1, "fetch_log_write(NULL db) -> -1");
  ok(fetch_log_write(db, NULL, "ok", 1, 1, NULL) == -1, "fetch_log_write(NULL source) -> -1");
  add_source(db, "md-a");
  long id1 = fetch_log_write(db, "md-a", NULL, 7, 123, "");
  long id2 = fetch_log_write(db, "md-a", "error", 0, 5, "boom");
  ok(id1 > 0 && id2 == id1 + 1, "rowids returned");
  ok(scalar(db, "SELECT COUNT(*) FROM fetch_log WHERE source_id='md-a' AND status='ok' AND error IS NULL") == 1,
     "NULL status defaults to ok, empty error stored as NULL");
  ok(scalar(db, "SELECT COUNT(*) FROM fetch_log WHERE source_id='md-a' AND error='boom' AND records_fetched=0") == 1,
     "error text stored");

  /* never throws on bad args */
  anomaly_detect(NULL, "x", 1, "ok", 1, 1);
  anomaly_detect(db, NULL, 1, "ok", 1, 1);

  /* status_bad, with dedupe against the open one */
  add_source(db, "md-bad");
  anomaly_detect(db, "md-bad", -1, "error", 0, 10);
  ok(anomalies(db, "md-bad", "status_bad") == 1, "errored run opens status_bad");
  ok(scalar(db, "SELECT COUNT(*) FROM collector_anomaly WHERE source_id='md-bad' AND fetch_log_id IS NULL") == 1,
     "fetch_log_id < 0 stored as NULL");
  anomaly_detect(db, "md-bad", -1, "error", 0, 10);
  anomaly_detect(db, "md-bad", -1, NULL, 0, 10);
  ok(anomalies_any(db, "md-bad") == 1, "no second open anomaly for the same source");
  exec_ok(db, "UPDATE collector_anomaly SET resolved_at=datetime('now') WHERE source_id='md-bad'");
  anomaly_detect(db, "md-bad", -1, "error", 0, 10);
  ok(anomalies_any(db, "md-bad") == 2, "after resolution a new anomaly may open");

  /* a healthy run opens nothing */
  add_source(db, "md-ok");
  for (int i = 0; i < 5; i++) fetch_log_write(db, "md-ok", "ok", 100, 200, NULL);
  anomaly_detect(db, "md-ok", -1, "ok", 100, 200);
  ok(anomalies_any(db, "md-ok") == 0, "healthy run: no anomaly");
  anomaly_detect(db, "md-ok", -1, "ok", 60, 200);
  ok(anomalies_any(db, "md-ok") == 0, "60 vs mean 100 is above the 50% line: no anomaly");

  /* records_drop */
  anomaly_detect(db, "md-ok", -1, "ok", 10, 200);
  ok(anomalies(db, "md-ok", "records_drop") == 1, "10 vs baseline mean 100 -> records_drop");

  /* no baseline + zero records on a brand-new source: not flagged */
  add_source(db, "md-new");
  fetch_log_write(db, "md-new", "ok", 0, 100, NULL);
  anomaly_detect(db, "md-new", -1, "ok", 0, 100);
  ok(anomalies_any(db, "md-new") == 0, "a new source with one empty run is not flagged");

  /* duration_outlier, flat threshold with no baseline */
  add_source(db, "md-slow");
  anomaly_detect(db, "md-slow", -1, "ok", 5, 9000);
  ok(anomalies_any(db, "md-slow") == 0, "9 s under the 10 s flat threshold");
  anomaly_detect(db, "md-slow", -1, "ok", 5, 10001);
  ok(anomalies(db, "md-slow", "duration_outlier") == 1, "10.001 s with no baseline -> duration_outlier");

  /* a legitimately slow source judges itself against its own history */
  add_source(db, "md-longrun");
  for (int i = 0; i < 4; i++) fetch_log_write(db, "md-longrun", "ok", 10, 60000, NULL);
  anomaly_detect(db, "md-longrun", -1, "ok", 10, 120000);
  ok(anomalies_any(db, "md-longrun") == 0, "2x its own 60 s baseline is not an outlier");
  anomaly_detect(db, "md-longrun", -1, "ok", 10, 200000);
  ok(anomalies(db, "md-longrun", "duration_outlier") == 1, ">3x its own baseline is");

  /* the never-produced cohort: records_drop cannot see a source that never had rows */
  add_source(db, "md-barren");
  for (int i = 0; i < 9; i++) fetch_log_write(db, "md-barren", "ok", 0, 100, NULL);
  anomaly_detect(db, "md-barren", -1, "ok", 0, 100);
  ok(anomalies_any(db, "md-barren") == 0, "9 empty runs: below the barren-run threshold");
  fetch_log_write(db, "md-barren", "ok", 0, 100, NULL);
  anomaly_detect(db, "md-barren", -1, "ok", 0, 100);
  ok(anomalies_any(db, "md-barren") == 1,
     "10 empty runs, none ever with a record: a never_produced anomaly is STORED "
     "(the schema CHECK must admit the verdict)");
}

static double metric(const char *txt, const char *name) {
  size_t n = strlen(name);
  for (const char *p = txt; (p = strstr(p, name)); p += n) {
    if ((p == txt || p[-1] == '\n') && p[n] == ' ') return atof(p + n + 1);
  }
  return -1;
}
static void t_metrics(db_handle *db) {
  printf("-- metricsapi\n");
  exec_ok(db, "DELETE FROM fetch_log");
  char *t = metricsapi_render(db);
  ok(t != NULL, "renders");
  ok(metric(t, "japanosint_sources_registered") > 0, "registered gauge > 0");
  double reg = metric(t, "japanosint_sources_registered"), sch = metric(t, "japanosint_sources_scheduled"),
         piv = metric(t, "japanosint_sources_pivots"), pod = metric(t, "japanosint_sources_pods");
  ok(sch >= 0 && piv >= 0 && pod >= 0 && sch + piv + pod <= reg, "partition never exceeds the registry");
  ok(metric(t, "japanosint_collector_runs_1h") == 0 && metric(t, "japanosint_collector_failures_1h") == 0,
     "empty fetch_log -> 0 / 0");
  ok(strstr(t, "# TYPE japanosint_sources_pods gauge\n") != NULL, "TYPE lines present");
  free(t);

  add_source(db, "mx-a");
  exec_ok(db, "INSERT INTO fetch_log(source_id,status,records_fetched) VALUES('mx-a','ok',1),('mx-a','error',0),('mx-a','error',0)");
  exec_ok(db, "INSERT INTO fetch_log(source_id,status,records_fetched,timestamp) VALUES('mx-a','error',0,datetime('now','-3 hours'))");
  t = metricsapi_render(db);
  ok(metric(t, "japanosint_collector_runs_1h") == 3, "rows older than an hour are excluded");
  ok(metric(t, "japanosint_collector_failures_1h") == 2, "failures counted by status=error");
  free(t);

  t = metricsapi_render(NULL);
  ok(t && metric(t, "japanosint_sources_registered") > 0 && strstr(t, "collector_runs_1h") == NULL,
     "NULL db: registry gauges only, the unreadable gauges are omitted not guessed");
  free(t);
}

static cJSON *pj(char *s) {
  assert(s);
  cJSON *j = cJSON_Parse(s);
  free(s);
  assert(j);
  return j;
}
static void t_misc(db_handle *db) {
  printf("-- miscapi\n");
  add_source(db, "mi-a");
  char *s = miscapi_source_by_id(db, "mi-a");
  cJSON *j = pj(s);
  ok(!strcmp(cJSON_GetObjectItem(j, "id")->valuestring, "mi-a"), "source row by id");
  cJSON_Delete(j);
  ok(miscapi_source_by_id(db, "no-such-source") == NULL, "unknown id -> NULL (404)");
  ok(miscapi_source_by_id(db, "") == NULL, "empty id -> NULL");

  s = miscapi_set_schedule(db, "mi-a", "{\"mode\":\"search_only\"}");
  j = pj(s);
  ok(!strcmp(cJSON_GetObjectItem(j, "schedule_mode")->valuestring, "search_only"), "schedule updated, row returned");
  cJSON_Delete(j);
  ok(miscapi_set_schedule(db, "no-such-source", "{\"mode\":\"map_cron\"}") == NULL, "unknown id -> NULL");
  const char *bad[] = { NULL, "", "not json", "{}", "{\"mode\":5}", "{\"mode\":\"weekly\"}", "{\"mode\":\"MAP_CRON\"}" };
  int allbad = 1;
  for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
    s = miscapi_set_schedule(db, "mi-a", bad[i]);
    if (!s || strcmp(s, "\1bad")) allbad = 0;
    free(s);
  }
  ok(allbad, "every malformed/invalid mode answers the \\1bad sentinel");
  ok(scalar(db, "SELECT COUNT(*) FROM sources WHERE id='mi-a' AND schedule_mode='search_only'") == 1,
     "a rejected mode leaves the row alone");
  ok(miscapi_set_schedule(db, "mi-a", "{\"mode\":\"map_cron\"}") != NULL, "map_cron accepted");

  /* logs: envelope, newest first, paging, clamps */
  exec_ok(db, "DELETE FROM fetch_log WHERE source_id='mi-a'");
  char sql[200];
  for (int i = 0; i < 7; i++) {
    snprintf(sql, sizeof sql,
             "INSERT INTO fetch_log(source_id,status,records_fetched,timestamp) VALUES('mi-a','ok',%d,'2026-01-0%dT00:00:00')",
             i, i + 1);
    exec_ok(db, sql);
  }
  ok(miscapi_source_logs(db, "no-such-source", 10, 0) == NULL, "logs: unknown source -> NULL");
  j = pj(miscapi_source_logs(db, "mi-a", 3, 0));
  cJSON *data = cJSON_GetObjectItem(j, "data"), *page = cJSON_GetObjectItem(j, "page");
  ok(cJSON_GetArraySize(data) == 3 && cJSON_GetObjectItem(page, "total")->valueint == 7,
     "3 of 7 and the total says 7 (disclosed, not silently sliced)");
  ok(cJSON_GetObjectItem(cJSON_GetArrayItem(data, 0), "records_fetched")->valueint == 6, "newest first");
  ok(!strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "meta"), "filters"), "source_id")->valuestring,
             "mi-a"), "filters echo the source id");
  cJSON_Delete(j);
  j = pj(miscapi_source_logs(db, "mi-a", 3, 6));
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(j, "data")) == 1, "offset reaches the tail row");
  cJSON_Delete(j);
  j = pj(miscapi_source_logs(db, "mi-a", 0, -5));
  ok(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "page"), "limit")->valueint == 50 &&
         cJSON_GetObjectItem(cJSON_GetObjectItem(j, "page"), "offset")->valueint == 0,
     "limit 0 -> default 50, negative offset -> 0");
  cJSON_Delete(j);
  j = pj(miscapi_source_logs(db, "mi-a", 100000, 0));
  ok(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "page"), "limit")->valueint == 500, "limit clamped to 500 and reported");
  cJSON_Delete(j);
  j = pj(miscapi_source_logs(db, "mi-a", 5, 1000));
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(j, "data")) == 0 &&
         cJSON_GetObjectItem(cJSON_GetObjectItem(j, "page"), "total")->valueint == 7,
     "offset past the end: empty page, true total");
  cJSON_Delete(j);

  /* layers */
  exec_ok(db, "DELETE FROM intel_items");
  exec_ok(db, "INSERT INTO intel_items(uid,source_id,fetched_at,record_type,tenant_id,lat,lon) VALUES"
              "('ly1','zz-not-registered','2026-01-01','exotic type','legacy',35.0,139.0),"
              "('ly2','zz-not-registered','2026-01-01','exotic type','legacy',35.1,139.1),"
              "('ly3','zz-not-registered','2026-01-01','','legacy',35.2,139.2),"
              "('ly4','zz-not-registered','2026-01-01','exotic type','legacy',NULL,NULL)");
  j = pj(miscapi_list_layers(db));
  ok(cJSON_IsArray(j) && cJSON_GetArraySize(j) > 3, "layers: bare array of the taxonomy");
  double total = 0;
  int gen_rt = 0, gen_un = 0, nullcounts = 0;
  cJSON *l;
  cJSON_ArrayForEach(l, j) {
    cJSON *r = cJSON_GetObjectItem(l, "records_geocoded");
    if (cJSON_IsNumber(r)) total += r->valuedouble; else nullcounts++;
    const char *id = cJSON_GetObjectItem(l, "id")->valuestring;
    if (!strncmp(id, "rt-", 3)) { gen_rt++; ok(!strcmp(cJSON_GetObjectItem(l, "kind")->valuestring, "generated"), "rt-* layer is kind generated"); }
    if (!strcmp(id, "unassigned-geocoded")) gen_un++;
  }
  ok(nullcounts == 0, "counts are numbers when the tally ran");
  ok(total == 3, "geocoded rows are all credited exactly once; the un-geocoded row is not");
  ok(gen_rt == 1 && gen_un == 1, "catch-all layers for the unregistered source (typed + untyped)");
  cJSON_Delete(j);
  j = pj(miscapi_list_layers(NULL));
  int nn = 1;
  cJSON_ArrayForEach(l, j) if (!cJSON_IsNull(cJSON_GetObjectItem(l, "records_geocoded"))) nn = 0;
  ok(nn, "NULL db: counts are null (unmeasured), never 0");
  cJSON_Delete(j);

  j = pj(miscapi_follow_recent(10));
  ok(!strcmp(cJSON_GetObjectItem(j, "error")->valuestring, "not_implemented"), "follow/recent: honest not_implemented");
  cJSON_Delete(j);
}

/* A deployed database keeps the CHECK list it was created with. Recreate the
 * pre-fix table, reopen, and the boot migration must widen it in place. */
static void t_migrate(db_handle *db) {
  printf("-- collector_anomaly verdict migration on a deployed table\n");
  exec_ok(db, "DROP TABLE collector_anomaly");
  exec_ok(db, "CREATE TABLE collector_anomaly (id INTEGER PRIMARY KEY AUTOINCREMENT,"
      " source_id TEXT NOT NULL REFERENCES sources(id), fetch_log_id INTEGER REFERENCES fetch_log(id),"
      " verdict TEXT NOT NULL CHECK(verdict IN ('records_drop','status_bad','sanity_failed','duration_outlier','manual')),"
      " reason TEXT, evidence TEXT, escalation_level INTEGER NOT NULL DEFAULT 0,"
      " created_at TEXT NOT NULL DEFAULT (datetime('now')), resolved_at TEXT, resolution TEXT,"
      " triage_class TEXT, triage_confidence REAL, triage_evidence TEXT, triage_suggested_fix TEXT,"
      " triaged_at TEXT, triage_model TEXT)");
  ok(sqlite3_exec(db->h, "INSERT INTO collector_anomaly(source_id,verdict) VALUES('md-a','never_produced')",
                  NULL, NULL, NULL) != SQLITE_OK, "precondition: the old CHECK refuses never_produced");
  exec_ok(db, "INSERT INTO collector_anomaly(source_id,verdict,reason) VALUES('md-a','status_bad','keep me')");
  db_close(db);
  memset(db, 0, sizeof *db);
  assert(db_open(db, NULL, NULL) == 0);
  ok(scalar(db, "SELECT COUNT(*) FROM sqlite_master WHERE name='collector_anomaly' AND sql LIKE '%never_produced%'") == 1,
     "reopen widened the stored CHECK");
  ok(scalar(db, "SELECT COUNT(*) FROM collector_anomaly WHERE reason='keep me'") == 1, "existing rows survive");
  add_source(db, "md-mig");
  for (int i = 0; i < 10; i++) fetch_log_write(db, "md-mig", "ok", 0, 100, NULL);
  anomaly_detect(db, "md-mig", -1, "ok", 0, 100);
  ok(anomalies(db, "md-mig", "never_produced") == 1, "never_produced now stores on the migrated table");
  db_close(db);
  memset(db, 0, sizeof *db);
  assert(db_open(db, NULL, NULL) == 0);
  ok(scalar(db, "SELECT COUNT(*) FROM collector_anomaly") == 2, "a second reopen is a no-op (idempotent)");
}

int main(void) {
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  t_maint(&db);
  t_metrics(&db);
  t_misc(&db);
  t_migrate(&db);
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
