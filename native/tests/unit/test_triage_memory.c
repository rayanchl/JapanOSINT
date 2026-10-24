/* test_triage_memory.c — the anomaly triage pod's memory
 * (collectors/pod/anomaly_triage.c, anomaly_vec): past triaged anomalies are
 * indexed, and the nearest ones — with their class and the repair outcome —
 * are offered to the model as fenced examples.
 *
 *   1. inert: without JO_EMBED_URL nothing is synced and no examples exist,
 *      so the prompt is what it always was;
 *   2. only TRIAGED anomalies are indexed;
 *   3. the nearest past anomaly comes first, carrying its triage class and
 *      the repair pod's outcome, inside a fence;
 *   4. a second sync with nothing newly triaged sends nothing. */
#include "../../collectors/pod/anomaly_triage.c"
#include "core/db.h"
#include "embed_stub.h"

#include <assert.h>

static db_handle g_db;

static void sql(const char *q) {
  char *e = NULL;
  if (sqlite3_exec(g_db.h, q, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", q);
    assert(0);
  }
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  assert(db_open(&g_db, NULL, NULL) == 0);
  unsetenv("JO_EMBED_URL");
  sql("INSERT OR IGNORE INTO sources(id,name,type,category) VALUES"
      " ('UNIT_MEM_A','a','api','test'),('UNIT_MEM_B','b','api','test')");
  sql("INSERT INTO collector_anomaly(id,source_id,verdict,reason,evidence,"
      "triage_class,triage_confidence,triage_evidence,triaged_at) VALUES"
      " (1,'UNIT_MEM_A','status_bad','http 429 too many requests quota exceeded',"
      "  'status 429','rate_limit',0.9,'429 with retry-after',datetime('now','-2 days')),"
      " (2,'UNIT_MEM_B','status_bad','http 404 not found endpoint moved',"
      "  'status 404','url_move',0.8,'404 on the old path',datetime('now','-1 days'))");
  sql("INSERT INTO collector_anomaly(id,source_id,verdict,reason,evidence) VALUES"
      " (3,'UNIT_MEM_A','status_bad','http 429 too many requests','status 429')");
  sql("INSERT INTO collector_repair(anomaly_id,source_id,status,action) VALUES"
      " (1,'UNIT_MEM_A','rejected','backoff')");
  anomaly_row an = { .id = 3, .source_id = "UNIT_MEM_A", .verdict = "status_bad",
                     .reason = "http 429 too many requests", .evidence = "status 429" };

  /* 1 */
  assert(memory_sync(&g_db, NULL) == 0);
  assert(!vecindex_exists(&g_db, &anomaly_vec_spec));
  assert(memory_examples(&g_db, NULL, &an) == NULL);
  printf("  inert: no sync, no examples, prompt unchanged: ok\n");

  if (estub_start(18500, 64, "stub-bow") != 0) {
    printf("  SKIP: embedding stub did not start (python3?)\n");
    return 0;
  }
  estub_use();

  /* 2 */
  assert(memory_sync(&g_db, NULL) == 0);
  sqlite3_stmt *s;
  assert(sqlite3_prepare_v2(g_db.h, "SELECT count(*) FROM anomaly_vec_done", -1, &s, NULL) == SQLITE_OK);
  assert(sqlite3_step(s) == SQLITE_ROW && sqlite3_column_int(s, 0) == 2);
  sqlite3_finalize(s);
  printf("  2 triaged anomalies indexed, the untriaged one not: ok\n");

  /* 3 */
  char *ex = memory_examples(&g_db, NULL, &an);
  assert(ex);
  assert(strstr(ex, "PAST_TRIAGES"));
  const char *rl = strstr(ex, "rate_limit"), *um = strstr(ex, "url_move");
  assert(rl && (!um || rl < um));
  assert(strstr(ex, "rejected (backoff)"));
  printf("  nearest past anomaly first, with its class and repair outcome, "
         "fenced: ok\n");
  free(ex);

  /* 4 */
  estub_log_reset();
  assert(memory_sync(&g_db, NULL) == 0);
  assert(estub_posts() == 0);
  printf("  nothing newly triaged: the sync sends nothing: ok\n");

  estub_stop();
  db_close(&g_db);
  printf("test_triage_memory: ok\n");
  return 0;
}
