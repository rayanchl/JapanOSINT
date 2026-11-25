/* tests/unit/test_entity_stats.c — core/entity_stats.c: PMI/lift over the
 * co-mention graph.  Pins the numbers against a hand-computed corpus, the
 * support floor (NULL stats, count still written), the exemptions (asserted
 * edges untouched, self edges unscored), the TTL gate, cancellation, an empty
 * corpus, and the open-addressed marginal map past its first growth (>1024
 * entities).  Reached through the header; no .c is included. */
#include "../../core/db.h"
#include "../../core/entity_stats.h"
#include "../../third_party/sqlite3.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}
static void ex(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql: %s\n  %s\n", e ? e : "?", sql);
    exit(1);
  }
}
static void ent(db_handle *db, const char *id) {
  char q[200];
  snprintf(q, sizeof q, "INSERT OR IGNORE INTO entities(entity_id,type,canonical,norm_key) VALUES('%s','org','%s','%s')", id, id, id);
  ex(db, q);
}
static void mention(db_handle *db, const char *e, const char *uid) {
  char q[250];
  snprintf(q, sizeof q, "INSERT OR IGNORE INTO entity_mentions(entity_id,item_uid,source_id,field,extractor) VALUES('%s','%s','s','title','t')", e, uid);
  ex(db, q);
}
static void edge(db_handle *db, const char *a, const char *b, const char *type) {
  char q[250];
  snprintf(q, sizeof q, "INSERT OR IGNORE INTO entity_relationships(src_entity_id,dst_entity_id,rel_type) VALUES('%s','%s','%s')", a, b, type);
  ex(db, q);
}
typedef struct { int null_pmi, null_lift, null_co, null_at; double pmi, lift; long co; int found; } row;
static row get(db_handle *db, const char *a, const char *b, const char *type) {
  row r = {0};
  sqlite3_stmt *s;
  char q[300];
  snprintf(q, sizeof q, "SELECT pmi,lift,co_count,stats_at FROM entity_relationships WHERE src_entity_id='%s' AND dst_entity_id='%s' AND rel_type='%s'", a, b, type);
  assert(sqlite3_prepare_v2(db->h, q, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) {
    r.found = 1;
    r.null_pmi = sqlite3_column_type(s, 0) == SQLITE_NULL;
    r.null_lift = sqlite3_column_type(s, 1) == SQLITE_NULL;
    r.null_co = sqlite3_column_type(s, 2) == SQLITE_NULL;
    r.null_at = sqlite3_column_type(s, 3) == SQLITE_NULL;
    r.pmi = sqlite3_column_double(s, 0);
    r.lift = sqlite3_column_double(s, 1);
    r.co = (long)sqlite3_column_int64(s, 2);
  }
  sqlite3_finalize(s);
  return r;
}

int main(void) {
  db_handle db;
  assert(db_open(&db, NULL, NULL) == 0);
  ex(&db, "DELETE FROM entity_relationships");
  ex(&db, "DELETE FROM entity_mentions");
  ex(&db, "DELETE FROM entities");

  printf("-- guards and migration\n");
  ok(entity_stats_ensure_columns(NULL) == -1, "ensure_columns(NULL) -> -1");
  ok(entity_stats_recompute(NULL, NULL) == -1, "recompute(NULL) -> -1");
  ok(entity_stats_ensure_columns(&db) == 0 && entity_stats_ensure_columns(&db) == 0, "ensure_columns idempotent");
  ok(entity_stats_recompute(&db, NULL) == 0, "no edges, no corpus: 0, not an error");

  printf("-- empty corpus leaves edges unstamped\n");
  ent(&db, "A"); ent(&db, "B");
  edge(&db, "A", "B", "co_mention");
  ok(entity_stats_recompute(&db, NULL) == 0, "edges but zero mentions (N=0): nothing scored");
  row r = get(&db, "A", "B", "co_mention");
  ok(r.found && r.null_at && r.null_pmi, "edge keeps NULL stats_at/pmi when there is no corpus");

  printf("-- numbers\n");
  /* 10 documents u0..u9.  A in u0-u5 (6), B in u0-u5 (6), C in u0,u1,u2,u6 (4),
   * D in u0 (1), E in u7 (1). N = 8 distinct docs carrying a mention
   * (u0-u7); u8,u9 carry none and must not count. */
  const char *A = "A", *B = "B", *C = "C", *D = "D", *E = "E";
  ent(&db, C); ent(&db, D); ent(&db, E); ent(&db, "ZZ-no-such");
  char u[16];
  for (int i = 0; i < 6; i++) { snprintf(u, sizeof u, "u%d", i); mention(&db, A, u); mention(&db, B, u); }
  const char *cu[] = { "u0", "u1", "u2", "u6" };
  for (int i = 0; i < 4; i++) mention(&db, C, cu[i]);
  mention(&db, D, "u0");
  mention(&db, E, "u7");
  edge(&db, "A", "C", "co_mention");     /* co = u0,u1,u2 = 3: at the floor */
  edge(&db, "A", "D", "co_mention");     /* co = 1: below the floor         */
  edge(&db, "A", "E", "co_mention");     /* co = 0: never together          */
  edge(&db, "A", "A", "co_mention");     /* self edge                       */
  edge(&db, "A", "C", "asserted");       /* human claim: never scored       */
  edge(&db, "A", "ZZ-no-such", "co_mention"); /* dangling endpoint, no mentions */
  long n = entity_stats_recompute(&db, NULL);
  ok(n == 6, "all six non-asserted edges stamped, the asserted one skipped");

  r = get(&db, "A", "B", "co_mention");
  double Nn = 8.0, lift_ab = 6.0 * Nn / (6.0 * 6.0);
  ok(r.found && r.co == 6 && fabs(r.lift - lift_ab) < 1e-9 && fabs(r.pmi - log(lift_ab)) < 1e-9,
     "A-B: co=6, lift=(6*8)/(6*6), pmi=log(lift)");
  r = get(&db, "A", "C", "co_mention");
  double lift_ac = 3.0 * Nn / (6.0 * 4.0);
  ok(r.co == 3 && fabs(r.lift - lift_ac) < 1e-9 && fabs(r.pmi - log(lift_ac)) < 1e-9,
     "A-C at ENTITY_STATS_MIN_CO_COUNT is scored; N counts only documents with a mention");
  r = get(&db, "A", "D", "co_mention");
  ok(r.co == 1 && r.null_pmi && r.null_lift && !r.null_co && !r.null_at,
     "below the support floor: NULL pmi/lift, co_count and stats_at still written");
  r = get(&db, "A", "E", "co_mention");
  ok(r.co == 0 && !r.null_co && r.null_pmi, "never co-mentioned: co_count 0, no score");
  r = get(&db, "A", "A", "co_mention");
  ok(!r.null_at && r.null_pmi && r.null_lift && r.null_co, "self edge: stamped, unscored, no co_count");
  r = get(&db, "A", "ZZ-no-such", "co_mention");
  ok(!r.null_at && r.co == 0 && r.null_pmi, "endpoint with no mentions: co_count 0, no score");
  r = get(&db, "A", "C", "asserted");
  ok(r.null_at && r.null_pmi && r.null_lift && r.null_co, "asserted edge keeps pmi/lift/co_count/stats_at NULL");

  printf("-- TTL gate\n");
  ok(entity_stats_recompute(&db, NULL) == 0, "second run inside the TTL: nothing stale, nothing done");
  ex(&db, "UPDATE entity_relationships SET stats_at=datetime('now','-7 hours'), pmi=99 WHERE src_entity_id='A' AND dst_entity_id='B'");
  n = entity_stats_recompute(&db, NULL);
  r = get(&db, "A", "B", "co_mention");
  ok(n == 1 && fabs(r.pmi - log(lift_ab)) < 1e-9, "only the edge past the TTL is rescored, and its stale value replaced");
  ex(&db, "UPDATE entity_relationships SET stats_at=datetime('now','-5 hours') WHERE rel_type<>'asserted'");
  ok(entity_stats_recompute(&db, NULL) == 0, "5 h old is still fresh (TTL is 6 h)");

  printf("-- cancellation\n");
  ex(&db, "UPDATE entity_relationships SET stats_at=NULL");
  volatile int stop = 1;
  ok(entity_stats_recompute(&db, &stop) == 0, "cancel set before start: returns 0");
  ok(get(&db, "A", "B", "co_mention").null_at, "a pre-cancelled run stamps nothing (edges stay stale for the next run)");
  stop = 0;
  ok(entity_stats_recompute(&db, &stop) == 6, "the next run, uncancelled, finishes the sweep");

  printf("-- marginal map growth (>1024 entities) and paging (>500 edges)\n");
  ex(&db, "DELETE FROM entity_relationships");
  ex(&db, "DELETE FROM entity_mentions");
  ex(&db, "DELETE FROM entities");
  ex(&db, "BEGIN");
  const int NE = 1700;
  for (int i = 0; i < NE; i++) {
    char id[24], q[300];
    snprintf(id, sizeof id, "ent-%05d", i);
    snprintf(q, sizeof q, "INSERT INTO entities(entity_id,type,canonical,norm_key) VALUES('%s','org','%s','%s')", id, id, id);
    ex(&db, q);
    /* every entity is mentioned in its own doc and in the shared docs s0..s2 */
    for (int k = 0; k < 3; k++) {
      char d[16];
      snprintf(d, sizeof d, "s%d", k);
      mention(&db, id, d);
    }
    char own[24];
    snprintf(own, sizeof own, "own-%05d", i);
    mention(&db, id, own);
    if (i > 0) {
      char prev[24];
      snprintf(prev, sizeof prev, "ent-%05d", i - 1);
      edge(&db, prev, id, "co_mention");
    }
  }
  ex(&db, "COMMIT");
  long scored = entity_stats_recompute(&db, NULL);
  ok(scored == NE - 1, "1,699 edges (more than ENTITY_STATS_BATCH) all stamped across pages");
  row b = get(&db, "ent-01698", "ent-01699", "co_mention");
  /* N = 3 shared + 1700 own docs; each entity is in 4 docs, joint = 3. */
  double Nb = 3.0 + NE, lift_b = 3.0 * Nb / (4.0 * 4.0);
  ok(b.co == 3 && fabs(b.lift - lift_b) < 1e-9, "an edge whose endpoints were inserted after the map grew reads the right marginals");
  row f = get(&db, "ent-00000", "ent-00001", "co_mention");
  ok(f.co == 3 && fabs(f.pmi - log(lift_b)) < 1e-9, "and so does one inserted before it");
  ok(entity_stats_recompute(&db, NULL) == 0, "re-run inside TTL is a no-op");

  db_close(&db);
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
