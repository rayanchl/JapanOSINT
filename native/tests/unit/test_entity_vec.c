/* test_entity_vec.c — the entity name index (core/entity_vec.c) and the
 * resolver's vector arm (core/entity_enrich.c), against the bag-of-words
 * embedding stub (embed_stub.h) and the fake judge (fake_llm.h).
 *
 * WHAT IS BEING PROTECTED.
 *   1. inert: with JO_EMBED_URL unset nothing is created and nothing is sent;
 *   2. a breach-quarantined entity is never embedded;
 *   3. the KNN is partitioned by type and floored by cosine: a company never
 *      neighbours a person, and a weak neighbour is not a candidate;
 *   4. a cross-script pair (a katakana canonical and its Latin spelling, which
 *      share no norm_key prefix) reaches the judge through the romaji reading;
 *   5. a vector candidate is a QUESTION: same=0 is recorded, nothing merges;
 *   6. a folded-away entity's vector is pruned.
 *
 * Includes entity_vec.c directly, so run.sh links every object except
 * obj/core/entity_vec.o. */
#include "../../core/entity_vec.c"
#include "core/entity_enrich.h"
#include "core/httpclient.h"
#include "embed_stub.h"
#include "fake_llm.h"

#include <assert.h>

static db_handle g_db;

static long count(const char *sql) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(g_db.h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}

static void resolve(int batch) {
  http_client *http = http_client_new();
  llm_client llm; llm_init(&llm, http);
  entity_enrich_resolve(&g_db, &llm, batch);
  http_client_free(http);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  assert(db_open(&g_db, NULL, NULL) == 0);
  setenv("LLM_ENABLED", "true", 1);
  setenv("LLM_TIMEOUT_MS", "4000", 1);
  unsetenv("JO_EMBED_URL");

  char *ja  = es_upsert_entity(&g_db, "company", "トヨタ");
  char *en  = es_upsert_entity(&g_db, "company", "Toyota");
  char *hol = es_upsert_entity(&g_db, "company", "Toyota Holdings Group");
  char *per = es_upsert_entity(&g_db, "person", "Toyota");
  char *br  = es_upsert_entity_scoped(&g_db, "company", "toyotaleak",
                                     ES_BREACH_TENANT);
  assert(ja && en && hol && per && br);
  assert(count("SELECT count(*) FROM entities WHERE tenant_id='"
               ES_BREACH_TENANT "'") == 1);

  /* 1. inert */
  assert(entity_vec_tick(&g_db, NULL, 0) == 0);
  assert(!vecindex_exists(&g_db, &entity_vec_spec));
  assert(!entity_vec_ready(&g_db));
  printf("  inert: no server, no table, resolver unchanged: ok\n");

  if (estub_start(18420, 64, "stub-bow") != 0) {
    printf("  SKIP: embedding stub did not start (python3?)\n");
    return 0;
  }
  estub_use();
  assert(fake_llm_start() > 0);

  /* 2. breach-scoped rows are not embedded */
  int n = entity_vec_tick(&g_db, NULL, 0);
  assert(n >= 4);
  assert(entity_vec_ready(&g_db));
  assert(count("SELECT count(*) FROM entity_vec_done d JOIN entities e"
               " USING(entity_id) WHERE e.tenant_id='" ES_BREACH_TENANT "'") == 0);
  printf("  %d entities embedded, breach-quarantined ones excluded: ok\n", n);

  /* 3. partition + floor */
  entity_nb *nb = NULL;
  int nn = entity_vec_neighbours(&g_db, en, "company", "", ENTITY_VEC_K,
                                 ENTITY_VEC_COS_FLOOR, &nb);
  assert(nn >= 0);
  for (int i = 0; i < nn; i++) {
    assert(strcmp(nb[i].entity_id, per) != 0);   /* a person, other partition */
    assert(strcmp(nb[i].entity_id, hol) != 0);   /* cos 0.5 < 0.80            */
    assert(nb[i].cos >= ENTITY_VEC_COS_FLOOR);
  }
  entity_nb_free(nb, nn);
  printf("  neighbours: same type only, below-floor names excluded: ok\n");

  /* 4 + 5. cross-script candidate through the romaji reading */
  char ro[128] = "";
  {
    sqlite3_stmt *s;
    assert(sqlite3_prepare_v2(g_db.h, "SELECT COALESCE(name_romaji,'') FROM entities"
                              " WHERE entity_id=?1", -1, &s, NULL) == SQLITE_OK);
    sqlite3_bind_text(s, 1, ja, -1, SQLITE_STATIC);
    if (sqlite3_step(s) == SQLITE_ROW)
      snprintf(ro, sizeof ro, "%s", (const char *)sqlite3_column_text(s, 0));
    sqlite3_finalize(s);
  }
  fake_llm_set_content("{\"same\":false,\"confidence\":0.9,\"reason\":\"unit\"}");
  if (!strcasecmp(ro, "toyota")) {
    resolve(4);
    char sql[512];
    int lo = strcmp(ja, en) < 0;
    snprintf(sql, sizeof sql, "SELECT count(*) FROM entity_merges WHERE"
             " entity_a='%s' AND entity_b='%s' AND same=0", lo ? ja : en, lo ? en : ja);
    assert(count(sql) == 1 && "the cross-script pair never reached the judge");
    assert(count("SELECT count(*) FROM entities WHERE type='company'"
                 " AND (tenant_id IS NULL)") == 3);
    printf("  トヨタ / Toyota (no shared prefix) asked of the judge via the romaji "
           "'%s', recorded same=0, nothing merged: ok\n", ro);
  } else {
    printf("  SKIP cross-script case: no MeCab reading for トヨタ (got '%s')\n", ro);
  }

  /* 6. a folded-away entity's vector is pruned */
  long before = count("SELECT count(*) FROM entity_vec_done");
  char sql[256];
  snprintf(sql, sizeof sql, "DELETE FROM entities WHERE entity_id='%s'", hol);
  assert(sqlite3_exec(g_db.h, sql, NULL, NULL, NULL) == SQLITE_OK);
  assert(entity_vec_tick(&g_db, NULL, 0) >= 0);
  assert(count("SELECT count(*) FROM entity_vec_done") == before - 1);
  snprintf(sql, sizeof sql, "SELECT count(*) FROM entity_vec WHERE entity_id='%s'", hol);
  assert(count(sql) == 0);
  printf("  deleted entity's vector pruned: ok\n");

  estub_stop();
  free(ja); free(en); free(hol); free(per); free(br);
  db_close(&g_db);
  printf("test_entity_vec: ok\n");
  return 0;
}
