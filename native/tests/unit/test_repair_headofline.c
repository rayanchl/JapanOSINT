/* test_repair_headofline.c — rows the repair pod cannot progress must not
 * hide the ones it can.
 *
 * collector_repair.c takes the oldest REPAIR_BATCH (3) triaged-open
 * anomalies, and attempt_repair() used to RETURN without resolving three
 * kinds of row: the source is quarantined, a verified fix is already staged
 * for a human, or the LLM gave no usable proposal. Three such rows at the
 * head and no newer anomaly was ever looked at again. Now the first two are
 * excluded in SQL, a model failure parks the row, and an unreachable server
 * ends the tick without charging anything.
 *
 * LLM via tests/unit/fake_llm.h; no source has a URL, so the pod's own
 * re-fetch is a "no url" node and nothing leaves the machine. */
#include "../../collectors/pod/collector_repair.c"
#include "core/db.h"
#include "fake_llm.h"

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
static void tick(db_handle *db) {
  http_client *http = http_client_new();
  llm_client llm; llm_init(&llm, http);
  volatile int cancel = 0;
  source_ctx ctx; memset(&ctx, 0, sizeof ctx);
  ctx.source_id = "collector-repair";
  ctx.db = db; ctx.http = http; ctx.llm = &llm; ctx.cancel = &cancel;
  run(&ctx, NULL);
  http_client_free(http);
}
static void add(db_handle *db, int id, const char *src, int minutes_ago) {
  char sql[400];
  snprintf(sql, sizeof sql,
           "INSERT INTO collector_anomaly(id,source_id,verdict,created_at,"
           "triaged_at,triage_class) VALUES (%d,'%s','status_bad',"
           "datetime('now','-%d minutes'),datetime('now'),'url_move')",
           id, src, minutes_ago);
  exec_ok(db, sql);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  setenv("LLM_ENABLED", "true", 1);
  setenv("LLM_REPAIR_TIMEOUT_MS", "4000", 1);
  exec_ok(&db,
    "INSERT OR IGNORE INTO sources(id,name,type,category) VALUES"
    " ('UNIT_REP_Q','q','api','test'),('UNIT_REP_OK','o','api','test');"
    "UPDATE sources SET quarantined_until=datetime('now','+1 day') WHERE id='UNIT_REP_Q';");
  add(&db, 1, "UNIT_REP_Q", 90);       /* quarantined source              */
  add(&db, 2, "UNIT_REP_OK", 80);      /* verified fix staged for a human */
  exec_ok(&db, "INSERT INTO collector_repair(anomaly_id,source_id,status,action)"
               " VALUES (2,'UNIT_REP_OK','verified','url_swap')");
  add(&db, 3, "UNIT_REP_OK", 70);
  add(&db, 4, "UNIT_REP_OK", 60);

  /* transport: the tick stops at the first candidate, charging nothing. */
  fake_llm_point_at_nothing();
  tick(&db);
  assert(count(&db, "SELECT COUNT(*) FROM collector_anomaly WHERE repair_attempts>0"
                    " OR repair_next_at IS NOT NULL") == 0);
  assert(count(&db, "SELECT COUNT(*) FROM collector_repair") == 1);
  printf("  llm down: nothing parked, no repair rows written\n");

  /* model failure: the batch of 3 reaches 3 and 4 — past the two rows that
   * cannot progress — and parks them. */
  assert(fake_llm_start() > 0);
  fake_llm_set_content("certainly! here is no json");
  tick(&db);
  assert(count(&db, "SELECT COUNT(*) FROM collector_anomaly WHERE id IN (3,4)"
                    " AND repair_attempts=1 AND repair_next_at>datetime('now')") == 2);
  assert(count(&db, "SELECT COUNT(*) FROM collector_anomaly WHERE id IN (1,2)"
                    " AND repair_attempts=0") == 2);
  /* parking writes no collector_repair row: the breaker counts those */
  assert(count(&db, "SELECT COUNT(*) FROM collector_repair") == 1);
  printf("  quarantined + staged rows skipped in SQL; 3 and 4 parked\n");

  /* the next new anomaly is reached although 4 older ones are open */
  add(&db, 5, "UNIT_REP_OK", 1);
  int c0 = atomic_load(&fake_llm_completions);
  tick(&db);
  assert(atomic_load(&fake_llm_completions) - c0 == 1);
  assert(count(&db, "SELECT repair_attempts FROM collector_anomaly WHERE id=5") == 1);
  printf("  newest anomaly reached past 4 that cannot progress\n");

  db_close(&db);
  printf("test_repair_headofline: ok\n");
  return 0;
}
