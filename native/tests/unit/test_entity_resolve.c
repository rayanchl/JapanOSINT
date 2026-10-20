/* test_entity_resolve.c — the tier-2 entity resolver's candidate walk
 * (core/entity_enrich.c, entity_enrich_resolve).
 *
 * WHAT IS BEING PROTECTED. The port's candidate SQL was a self-join on
 * substr(norm_key,1,4) with LIMIT, no ORDER BY and no exclusion of decided
 * pairs. Every tick read the same first pairs, skipped the decided ones in C,
 * and once those were all decided attempted NOTHING while undecided pairs
 * further down the join were never reached — rc 0, "attempted=0", forever.
 *
 *   1. decided pairs are excluded in the query, so a tick after all of the
 *      first batch were decided still reaches the next undecided pair;
 *   2. the walk starts at the most-mentioned entity;
 *   3. a pair the model could not answer does not stay at the head: the
 *      keyset advances past it, and it is re-asked only on the next lap;
 *   4. an LLM outage moves nothing — the pair is asked again next tick;
 *   5. the 0.7 confidence rule of the union pass is unchanged.
 *
 * Driven against tests/unit/fake_llm.h. */
#include "../../core/entity_enrich.c"
#include "core/entitystore.h"
#include "core/db.h"
#include "fake_llm.h"

#include <assert.h>

static db_handle g_db;

static void exec_ok(const char *sql) {
  char *err = NULL;
  if (sqlite3_exec(g_db.h, sql, NULL, NULL, &err) != SQLITE_OK) {
    fprintf(stderr, "exec failed: %s\n  %s\n", err ? err : "?", sql);
    assert(0);
  }
}
static long count(const char *sql) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(g_db.h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}

static int resolve(int batch) {
  http_client *http = http_client_new();
  llm_client llm; llm_init(&llm, http);
  int c0 = atomic_load(&fake_llm_completions);
  entity_enrich_resolve(&g_db, &llm, batch);
  http_client_free(http);
  return atomic_load(&fake_llm_completions) - c0;
}

static char *ent(const char *type, const char *v, int mentions) {
  char *id = es_upsert_entity(&g_db, type, v);
  assert(id);
  char sql[256];
  snprintf(sql, sizeof sql, "UPDATE entities SET mention_count=%d"
                            " WHERE entity_id='%s'", mentions, id);
  exec_ok(sql);
  return id;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  assert(db_open(&g_db, NULL, NULL) == 0);
  setenv("LLM_ENABLED", "true", 1);
  setenv("LLM_TIMEOUT_MS", "4000", 1);
  assert(fake_llm_start() > 0);

  /* Four companies sharing a prefix (6 pairs) and one that shares none. */
  char *top = ent("company", "Kobe Steel Works", 50);
  char *b   = ent("company", "Kobe Steel Ltd", 20);
  char *c   = ent("company", "Kobe Steel Trading", 10);
  char *d   = ent("company", "Kobe Steel Holdings", 5);
  char *lone = ent("company", "Osaka Gas", 99);
  (void)lone;

  /* 3 + 4. garbage answers: each tick reaches NEW pairs, never re-asks. */
  fake_llm_set_content("I cannot decide");
  assert(resolve(2) == 2);
  /* 2: the walk started at the most-mentioned entity of the prefix group */
  char cur[256];
  {
    sqlite3_stmt *s;
    assert(sqlite3_prepare_v2(g_db.h, "SELECT v FROM entity_resolve_state"
                              " WHERE k='prefix_e1'", -1, &s, NULL) == SQLITE_OK);
    assert(sqlite3_step(s) == SQLITE_ROW);
    snprintf(cur, sizeof cur, "%s", (const char *)sqlite3_column_text(s, 0));
    sqlite3_finalize(s);
  }
  assert(!strcmp(cur, top));
  assert(resolve(2) == 2);
  assert(resolve(2) == 2);            /* six pairs: one lap is three ticks  */
  assert(count("SELECT count(*) FROM entity_merges") == 0);
  printf("  unanswerable pairs: 6 distinct pairs over 3 ticks, none re-asked "
         "within the lap, walk started at the most-mentioned: ok\n");

  /* 4. outage: nothing advances. */
  fake_llm_point_at_nothing();
  {
    http_client *http = http_client_new();
    llm_client llm; llm_init(&llm, http);
    entity_enrich_resolve(&g_db, &llm, 2);
    http_client_free(http);
  }
  fake_llm_point_back();
  assert(count("SELECT count(*) FROM entity_merges") == 0);
  printf("  llm down: cursor untouched, nothing recorded: ok\n");

  /* 1. decided pairs are excluded: once every pair is decided a tick asks
   * nothing, and a NEW entity's pairs are reached at once. */
  fake_llm_set_content("{\"same\":false,\"confidence\":0.9,\"reason\":\"unit\"}");
  int asked = 0;
  for (int t = 0; t < 6; t++) asked += resolve(2);
  assert(asked == 6);
  assert(count("SELECT count(*) FROM entity_merges") == 6);
  assert(resolve(2) == 0);
  assert(resolve(2) == 0);
  char *e = ent("company", "Kobe Steel Engineering", 1);
  assert(resolve(10) == 4);             /* e pairs with the four, nothing else */
  assert(count("SELECT count(*) FROM entity_merges") == 10);
  printf("  decided pairs excluded in SQL: 6 decided once, then 0 asked; a new "
         "entity's 4 pairs reached in one tick: ok\n");

  /* 5. the union rule: same=1 below 0.7 is recorded, never merged. */
  char *f = ent("person", "Yamada Taro", 3);
  char *g = ent("person", "Yamada Tarou", 2);
  fake_llm_set_content("{\"same\":true,\"confidence\":0.6,\"reason\":\"unit\"}");
  assert(resolve(10) == 1);
  assert(count("SELECT count(*) FROM entities WHERE type='person'") == 2);
  printf("  same=1 at 0.6 recorded, not merged (0.7 rule): ok\n");

  free(top); free(b); free(c); free(d); free(lone); free(e); free(f); free(g);
  db_close(&g_db);
  printf("test_entity_resolve: ok\n");
  return 0;
}
