/* tests/unit/test_source_category.c — canonical category labels
 * (core/source_category.c) over the service index, against
 * tools/embed_stub_server.py.
 *
 * WHAT IS BEING PROTECTED.
 *   1. The vocabulary here is source.h's: every one of the 31 still appears
 *      in its comment, so neither list can change alone.
 *   2. NOTHING IS RENAMED: `sources.category` is byte-identical before and
 *      after both passes. The labels are derived columns beside it.
 *   3. Declared labels need no embedding server and are exactly the declared
 *      category for every source that declares one of the 31.
 *   4. Derived labels are one of the 31, carry provenance 'derived' and a
 *      confidence in (0, 1], and are never given to a pod.
 *   5. A derivation runs once per index: the second call is a no-op.
 *
 * The stub's vectors are hashed character trigrams: the derived
 * DISTRIBUTION printed here says nothing about embedding quality.
 *
 * Includes source_category.c directly, so run.sh links every object EXCEPT
 * obj/core/source_category.o and obj/main.o. */

#include "../../core/source_category.c"
#include "../../core/llm.h"

#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t g_stub = 0;

static void stub_stop(void) {
  if (g_stub > 0) { kill(g_stub, SIGKILL); waitpid(g_stub, NULL, 0); g_stub = 0; }
}

static int answers(int port) {
  char url[64];
  snprintf(url, sizeof url, "http://127.0.0.1:%d", port);
  llm_client c = { .http = NULL, .base_url = url, .interactive = 1 };
  const char *t[1] = { "ping" };
  float *v = NULL; int d = 0; llm_status st;
  int ok = llm_embed(&c, t, 1, &v, &d, 1000, &st) == 0;
  free(v);
  return ok;
}

static int stub_start(int port) {
  for (int tries = 0; tries < 40 && answers(port); tries++) port++;
  if (answers(port)) return -1;
  fflush(stdout);
  pid_t p = fork();
  if (p < 0) return -1;
  if (p == 0) {
    char ps[16];
    snprintf(ps, sizeof ps, "%d", port);
    /* Both streams: an inherited pipe would hold a reader open after this
     * test died, which hangs `run.sh | tail` rather than failing it. */
    if (!freopen("/dev/null", "w", stdout) || !freopen("/dev/null", "w", stderr)) {
      /* stub noise is harmless */
    }
    execlp("python3", "python3", "tools/embed_stub_server.py", ps, "64", (char *)NULL);
    _exit(127);
  }
  g_stub = p;
  for (int i = 0; i < 100; i++) {
    if (answers(port)) {
      char url[64];
      snprintf(url, sizeof url, "http://127.0.0.1:%d", port);
      setenv("JO_EMBED_URL", url, 1);
      return 0;
    }
    usleep(100000);
  }
  return -1;
}

/* FNV of every (id, category) — the "nothing renamed" fingerprint. */
static unsigned long long category_fingerprint(db_handle *db, long *rows) {
  sqlite3_stmt *s = NULL;
  unsigned long long h = 1469598103934665603ULL;
  *rows = 0;
  assert(sqlite3_prepare_v2(db->h, "SELECT id, category FROM sources ORDER BY id",
                            -1, &s, NULL) == SQLITE_OK);
  while (sqlite3_step(s) == SQLITE_ROW) {
    for (int c = 0; c < 2; c++) {
      const unsigned char *t = sqlite3_column_text(s, c);
      for (; t && *t; t++) { h ^= *t; h *= 1099511628211ULL; }
      h ^= 0xff; h *= 1099511628211ULL;
    }
    (*rows)++;
  }
  sqlite3_finalize(s);
  return h;
}

static void test_vocabulary(void) {
  char path[512];
  snprintf(path, sizeof path, "%s/native/source.h", JO_REPO_ROOT);
  FILE *f = fopen(path, "r");
  assert(f);
  static char buf[1 << 16];
  size_t n = fread(buf, 1, sizeof buf - 1, f);
  fclose(f);
  buf[n] = 0;
  const char *start = strstr(buf, "The `category` vocabulary");
  assert(start);
  for (int i = 0; i < SOURCE_CATEGORY_COUNT; i++) {
    char w[40];
    snprintf(w, sizeof w, " %s ", SOURCE_CATEGORIES[i]);
    char w2[40];
    snprintf(w2, sizeof w2, " %s\n", SOURCE_CATEGORIES[i]);
    assert(strstr(start, w) || strstr(start, w2));
  }
  printf("  the 31 categories are source.h's list: ok\n");
}

static void test_declared(db_handle *db) {
  long rows = 0;
  unsigned long long before = category_fingerprint(db, &rows);
  unsetenv("JO_EMBED_URL");
  assert(source_category_declare(db) > 0);
  assert(source_category_declare(db) == 0);        /* steady: no writes */
  sqlite3_stmt *s = NULL;
  long bad = 0, declared = 0;
  assert(sqlite3_prepare_v2(db->h, "SELECT id, category, category_canonical, "
         "category_provenance FROM sources", -1, &s, NULL) == SQLITE_OK);
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *id = (const char *)sqlite3_column_text(s, 0);
    const char *cat = (const char *)sqlite3_column_text(s, 1);
    const char *can = (const char *)sqlite3_column_text(s, 2);
    const char *pr = (const char *)sqlite3_column_text(s, 3);
    const source_card *c = source_card_get(id);
    if (!c) continue;
    if (c->kind != SC_KIND_POD && source_category_is_canonical(c->category)) {
      if (!pr || strcmp(pr, "declared") || !can || strcmp(can, c->category)) bad++;
      declared++;
    } else if (pr) bad++;
    (void)cat;
  }
  sqlite3_finalize(s);
  assert(bad == 0 && declared > 0);
  long rows2 = 0;
  assert(category_fingerprint(db, &rows2) == before && rows2 == rows);
  assert(source_category_derive(db) == 0);         /* inert: nothing derived */

  /* entity types land in the sources table: mask for pivots, NULL else */
  assert(source_entity_types_write(db) > 0);
  assert(source_entity_types_write(db) == 0);      /* steady: no writes */
  long lab = 0, wrong = 0;
  assert(sqlite3_prepare_v2(db->h, "SELECT id, entity_types, "
         "entity_types_provenance FROM sources", -1, &s, NULL) == SQLITE_OK);
  while (sqlite3_step(s) == SQLITE_ROW) {
    const source_card *c = source_card_get((const char *)sqlite3_column_text(s, 0));
    if (!c) continue;
    int isnull = sqlite3_column_type(s, 1) == SQLITE_NULL;
    if (c->kind != SC_KIND_PIVOT) { if (!isnull) wrong++; continue; }
    if (isnull || (unsigned)sqlite3_column_int(s, 1) != c->entity_types) wrong++;
    if (c->entity_types) {
      lab++;
      if (sqlite3_column_type(s, 2) == SQLITE_NULL) wrong++;
    }
  }
  sqlite3_finalize(s);
  assert(wrong == 0 && lab > 0);
  printf("  entity types written: %ld labelled pivots, non-pivots NULL: ok\n", lab);
  printf("  declared: %ld sources labelled without an embedding server, "
         "category column untouched: ok\n", declared);
}

static void test_derived(db_handle *db) {
  long rows = 0;
  unsigned long long before = category_fingerprint(db, &rows);
  assert(service_vec_build(db) > 0);
  int n = source_category_derive(db);
  assert(n > 0);
  assert(source_category_derive(db) == 0);         /* same index: no-op */
  sqlite3_stmt *s = NULL;
  long bad = 0, derived = 0;
  assert(sqlite3_prepare_v2(db->h, "SELECT id, category_canonical, "
         "category_confidence FROM sources WHERE category_provenance='derived'",
         -1, &s, NULL) == SQLITE_OK);
  while (sqlite3_step(s) == SQLITE_ROW) {
    const char *id = (const char *)sqlite3_column_text(s, 0);
    const char *can = (const char *)sqlite3_column_text(s, 1);
    double conf = sqlite3_column_double(s, 2);
    const source_card *c = source_card_get(id);
    if (!c || c->kind == SC_KIND_POD || source_category_is_canonical(c->category) ||
        !source_category_is_canonical(can) || !(conf > 0 && conf <= 1.0)) bad++;
    derived++;
  }
  sqlite3_finalize(s);
  assert(bad == 0 && derived == n);
  long rows2 = 0;
  assert(category_fingerprint(db, &rows2) == before && rows2 == rows);
  cJSON *d = source_category_distribution(db);
  char *js = cJSON_PrintUnformatted(d);
  printf("  derived: %d sources by a %d-nearest vote, all canonical, confidence "
         "in (0,1], category column untouched: ok\n  distribution (stub "
         "vectors): %s\n", n, SCAT_K, js);
  free(js);
  cJSON_Delete(d);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  assert(dbp && "run.sh sets JO_DB to a scratch database");
  db_handle db;
  if (db_open(&db, dbp, NULL) != 0) return 2;
  setenv("JO_EMBED_MODEL_CHECK_TTL_MS", "0", 1);
  setenv("JO_SVEC_MAX_PER_RUN", "0", 1);
  unsetenv("JO_EMBED_MODEL");
  test_vocabulary();
  test_declared(&db);
  if (stub_start(18260) != 0) {
    printf("test_source_category: stub unavailable, derived-label tests skipped\n");
    stub_stop();
    db_close(&db);
    return 0;
  }
  test_derived(&db);
  stub_stop();
  db_close(&db);
  printf("test_source_category: ok\n");
  return 0;
}
