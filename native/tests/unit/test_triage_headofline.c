/* test_triage_headofline.c — an anomaly the model cannot classify must not
 * block the ones behind it, and an LLM outage must not be held against the
 * work it interrupted.
 *
 * anomaly_triage.c takes `ORDER BY created_at ASC LIMIT 5` untriaged
 * anomalies per tick and, on an unusable answer, left the row untouched — so
 * five rows the model choked on were re-asked every minute and every newer
 * anomaly waited behind them for good. Entity extraction
 * (core/entity_enrich.c, linked here) counted a llama-server outage against
 * each ITEM it was processing, and an item at failed_count 5 is never
 * retried: one restart retired a whole batch.
 *
 * Driven against tests/unit/fake_llm.h: a server that is up and answers
 * garbage (model failure) and a port nothing listens on (transport). */
#include "../../collectors/pod/anomaly_triage.c"
#include "core/entity_enrich.h"
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

/* One pod tick with a fresh client, so a changed LLM_BASE_URL is seen. */
static void tick(db_handle *db) {
  http_client *http = http_client_new();
  llm_client llm; llm_init(&llm, http);
  volatile int cancel = 0;
  source_ctx ctx; memset(&ctx, 0, sizeof ctx);
  ctx.source_id = "anomaly-triage";
  ctx.db = db; ctx.http = http; ctx.llm = &llm; ctx.cancel = &cancel;
  run(&ctx, NULL);
  http_client_free(http);
}

static void add_anomaly(db_handle *db, int id, int minutes_ago) {
  char sql[256];
  snprintf(sql, sizeof sql,
           "INSERT INTO collector_anomaly(id,source_id,verdict,created_at)"
           " VALUES (%d,'UNIT_TRI_SRC','status_bad',datetime('now','-%d minutes'))",
           id, minutes_ago);
  exec_ok(db, sql);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  setenv("LLM_ENABLED", "true", 1);
  setenv("LLM_TRIAGE_TIMEOUT_MS", "4000", 1);
  setenv("LLM_TIMEOUT_MS", "4000", 1);
  exec_ok(&db, "INSERT OR IGNORE INTO sources(id,name,type,category)"
               " VALUES ('UNIT_TRI_SRC','t','api','test')");
  for (int i = 1; i <= 7; i++) add_anomaly(&db, i, 100 - i);   /* 1 oldest */

  /* 1. transport: nothing is counted against any anomaly. */
  fake_llm_point_at_nothing();
  tick(&db);
  assert(count(&db, "SELECT COUNT(*) FROM collector_anomaly WHERE triage_attempts>0"
                    " OR triage_next_at IS NOT NULL OR triaged_at IS NOT NULL") == 0);
  printf("  llm down: 0 anomalies charged, 0 parked\n");

  /* 2. model failure: the batch's 5 are parked, so the next tick reaches 6, 7. */
  assert(fake_llm_start() > 0);
  fake_llm_set_content("I am not JSON at all");
  int c0 = atomic_load(&fake_llm_completions);
  tick(&db);
  assert(atomic_load(&fake_llm_completions) - c0 == 5);
  assert(count(&db, "SELECT COUNT(*) FROM collector_anomaly WHERE id<=5"
                    " AND triage_attempts=1 AND triage_next_at>datetime('now')") == 5);
  tick(&db);
  assert(count(&db, "SELECT COUNT(*) FROM collector_anomaly WHERE id IN (6,7)"
                    " AND triage_attempts=1") == 2 && "rows 6,7 stuck behind parked rows");
  assert(atomic_load(&fake_llm_completions) - c0 == 7);
  printf("  unparseable: parked 5, the next tick reached the 2 behind them\n");

  /* 3. a new anomaly is triaged although 7 older ones are unresolved. */
  add_anomaly(&db, 8, 1);
  fake_llm_set_content("{\"class\":\"transient\",\"confidence\":0.9,"
                       "\"evidence\":\"unit\",\"suggested_fix\":null}");
  tick(&db);
  assert(count(&db, "SELECT COUNT(*) FROM collector_anomaly WHERE id=8"
                    " AND triage_class='transient' AND triaged_at IS NOT NULL") == 1);
  assert(count(&db, "SELECT COUNT(*) FROM collector_anomaly WHERE triaged_at IS NOT NULL") == 1);
  printf("  newest anomaly triaged past 7 parked ones\n");

  /* 4. entity extraction: an outage does not touch failed_count. */
  exec_ok(&db, "INSERT INTO intel_items(uid,source_id,title,fetched_at) VALUES"
               " ('UNIT_TRI_SRC|1','UNIT_TRI_SRC','alpha',datetime('now')),"
               " ('UNIT_TRI_SRC|2','UNIT_TRI_SRC','beta',datetime('now'))");
  fake_llm_point_at_nothing();
  {
    http_client *http = http_client_new();
    llm_client llm; llm_init(&llm, http);
    entity_enrich_extract(&db, &llm, 10);
    http_client_free(http);
  }
  assert(count(&db, "SELECT COUNT(*) FROM entity_extraction_state"
                    " WHERE item_uid LIKE 'UNIT_TRI_SRC|%'") == 0 &&
         "an LLM outage was counted against the items");
  printf("  entity extraction, llm down: no item charged\n");
  /* ...while an answer the model really gave still is. */
  fake_llm_point_back();
  fake_llm_set_content("no entities here, and no JSON either");
  {
    http_client *http = http_client_new();
    llm_client llm; llm_init(&llm, http);
    entity_enrich_extract(&db, &llm, 10);
    http_client_free(http);
  }
  assert(count(&db, "SELECT COUNT(*) FROM entity_extraction_state"
                    " WHERE item_uid LIKE 'UNIT_TRI_SRC|%' AND failed_count=1") == 2);
  printf("  entity extraction, bad answer: both items charged once\n");

  db_close(&db);
  printf("test_triage_headofline: ok\n");
  return 0;
}
