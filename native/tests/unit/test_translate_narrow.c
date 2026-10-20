/* test_translate_narrow.c — the background translate pod's narrowing
 * (core/translate.c, translate_run_ex / narrow_plan, M3).
 *
 *   1. without an active embedding index nothing changes: every pending
 *      Japanese row is attempted;
 *   2. with one, the pod reaches only items a user opened recently and items
 *      of sources an English keyword alert rule watches;
 *   3. an English keyword rule with no source_ids watches everything, so the
 *      tick is not narrowed, and says why;
 *   4. JO_TRANSLATE_NARROW=0 restores the old behaviour;
 *   5. the explicit path (translate_run, what the backfill loops) is never
 *      narrowed;
 *   6. the scope is reported by translate_backfill_status.
 *
 * Driven against tests/unit/fake_llm.h, which answers every generation with
 * text that is not a translation: an ATTEMPTED row ends with
 * translate_failed=1, an untouched one with 0 — that is what is counted. */
#include "../../core/translate.c"
#include "core/db.h"
#include "core/httpclient.h"
#include "fake_llm.h"

#include <assert.h>

static db_handle g_db;

static void sql(const char *q) {
  char *e = NULL;
  if (sqlite3_exec(g_db.h, q, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", q);
    assert(0);
  }
}
static long count(const char *q) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(g_db.h, q, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}
static void reset(void) { sql("UPDATE intel_items SET translate_failed=0, translated_at=NULL"); }
static int attempted(const char *uid) {
  char q[160];
  snprintf(q, sizeof q, "SELECT translate_failed FROM intel_items WHERE uid='%s'", uid);
  return count(q) == 1;
}
static void tick(int narrow) {
  http_client *http = http_client_new();
  llm_client llm; llm_init(&llm, http);
  translate_run_ex(&g_db, &llm, 10, NULL, narrow);
  http_client_free(http);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  assert(db_open(&g_db, NULL, NULL) == 0);
  setenv("LLM_ENABLED", "true", 1);
  setenv("TRANSLATE_TIMEOUT_MS", "3000", 1);
  unsetenv("JO_EMBED_URL");
  unsetenv("JO_TRANSLATE_NARROW");
  assert(fake_llm_start() > 0);
  fake_llm_set_content("not a translation");
  translate_migrate(&g_db);
  sql("INSERT INTO tenants(id,slug,name) VALUES ('t1','t1','T1')");
  sql("INSERT INTO intel_items(uid,source_id,title,fetched_at) VALUES"
      " ('v1','OTHER','東京で地震が発生しました',strftime('%Y-%m-%dT%H:%M:%fZ','now')),"
      " ('r1','WATCHED','大阪の港で火災が発生しました',strftime('%Y-%m-%dT%H:%M:%fZ','now')),"
      " ('x1','OTHER','名古屋で新しい駅が開業しました',strftime('%Y-%m-%dT%H:%M:%fZ','now'))");

  /* 1 */
  tick(1);
  assert(attempted("v1") && attempted("r1") && attempted("x1"));
  printf("  no embedding index: all 3 pending rows attempted: ok\n");

  /* 2 */
  reset();
  setenv("JO_EMBED_URL", "http://127.0.0.1:9", 1);   /* never contacted */
  vecindex_ensure_meta(&g_db, &embed_intel_spec);
  assert(vecindex_create(&g_db, &embed_intel_spec, 8) == 0);
  sql("INSERT INTO alert_rules(id,tenant_id,name,predicate_json) VALUES"
      " ('en1','t1','fire','{\"q\":\"fire port\",\"source_ids\":[\"watched\"]}'),"
      " ('ja1','t1','jp','{\"q\":\"地震\"}')");
  translate_note_view(&g_db, "v1");
  tick(1);
  assert(attempted("v1") && attempted("r1") && !attempted("x1"));
  printf("  narrowed: the opened item and the English rule's source attempted, "
         "the rest left for the backfill: ok\n");

  /* 6 */
  {
    char *js = translate_backfill_status(&g_db);
    assert(js && strstr(js, "\"background_scope\":{\"narrowed\":true"));
    assert(strstr(js, "WATCHED") || strstr(js, "watched"));
    free(js);
  }

  /* 3 */
  reset();
  sql("INSERT INTO alert_rules(id,tenant_id,name,predicate_json) VALUES"
      " ('en2','t1','all','{\"q\":\"earthquake\"}')");
  tick(1);
  assert(attempted("x1"));
  {
    char *js = translate_backfill_status(&g_db);
    assert(strstr(js, "every source"));
    free(js);
  }
  sql("DELETE FROM alert_rules WHERE id='en2'");
  printf("  an English rule over every source: not narrowed, reason stated: ok\n");

  /* 4 */
  reset();
  setenv("JO_TRANSLATE_NARROW", "0", 1);
  tick(1);
  assert(attempted("x1"));
  unsetenv("JO_TRANSLATE_NARROW");
  printf("  JO_TRANSLATE_NARROW=0: old behaviour: ok\n");

  /* 5 */
  reset();
  {
    http_client *http = http_client_new();
    llm_client llm; llm_init(&llm, http);
    translate_run(&g_db, &llm, 10, NULL);
    http_client_free(http);
  }
  assert(attempted("x1"));
  printf("  explicit translate_run (the backfill's loop) never narrowed: ok\n");

  db_close(&g_db);
  printf("test_translate_narrow: ok\n");
  return 0;
}
