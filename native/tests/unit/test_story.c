/* test_story.c — stories and novelty from the embedding pod's post-batch hook
 * (core/story.c) and GET /api/stories/:id (core/storyapi.c), end to end: the
 * real embedding pod against the bag-of-words stub (embed_stub.h), the hook
 * firing on its committed batches.
 *
 * WHAT IS BEING PROTECTED.
 *   1. two items saying the same thing within 72 h share a story_id; the same
 *      text ten days earlier does not join (the window is a KNN filter);
 *   2. a story never crosses tenants: an identical tenant row stays out;
 *   3. an unrelated item has no story (NULL, never a placeholder);
 *   4. novelty is measured against EARLIER items only: the later twin is
 *      ~0.09, the earlier one 1.0;
 *   5. a row past the hook's time budget waits in story_pending and is
 *      processed by a later call — nothing skipped;
 *   6. /api/stories/:id pages its members with a measured total, 404s an
 *      unknown story and 400s a malformed id. */
#include "../../core/story.c"
#include "core/storyapi.h"
#include "core/httpclient.h"
#include "../../source.h"
#include "embed_stub.h"

#include <assert.h>

static db_handle g_db;

static void sql(const char *q) {
  char *err = NULL;
  if (sqlite3_exec(g_db.h, q, NULL, NULL, &err) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", err ? err : "?", q);
    assert(0);
  }
}
static char *one(const char *q) {
  sqlite3_stmt *s; char *v = NULL;
  assert(sqlite3_prepare_v2(g_db.h, q, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 0) != SQLITE_NULL)
    v = strdup((const char *)sqlite3_column_text(s, 0));
  sqlite3_finalize(s);
  return v;
}

static void item(const char *uid, const char *title, const char *ago,
                 const char *tenant) {
  char q[1024];
  snprintf(q, sizeof q,
    "INSERT INTO intel_items(uid,source_id,title,published_at,fetched_at,"
    "record_type,tenant_id) VALUES('%s','unit-src','%s',"
    "strftime('%%Y-%%m-%%dT%%H:%%M:%%SZ','now','%s'),"
    "strftime('%%Y-%%m-%%dT%%H:%%M:%%fZ','now'),'news','%s')",
    uid, title, ago, tenant);
  sql(q);
}

static void pod_tick(void) {
  const source_def *d = registry_get("embedding-backfill");
  assert(d && d->run);
  http_client *hc = http_client_new();
  volatile int cancel = 0;
  source_ctx ctx = { .source_id = d->id, .http = hc, .db = &g_db, .cancel = &cancel };
  assert(d->run(&ctx, NULL) == 0);
  http_client_free(hc);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  assert(db_open(&g_db, NULL, NULL) == 0);
  if (estub_start(18460, 64, "stub-bow") != 0) {
    printf("  SKIP: embedding stub did not start (python3?)\n");
    return 0;
  }
  estub_use();

  item("u|a1", "earthquake tsunami warning kobe port", "-0 hours", "legacy");
  item("u|a2", "earthquake tsunami warning kobe port evacuation", "-1 hours", "legacy");
  item("u|a3", "earthquake tsunami warning kobe port", "-10 days", "legacy");
  item("u|b1", "sony quarterly earnings report", "-2 hours", "legacy");
  item("u|t1", "earthquake tsunami warning kobe port", "-0 hours", "t1");
  pod_tick();

  /* 1 + 2 + 3 */
  char *s1 = one("SELECT story_id FROM intel_items WHERE uid='u|a1'");
  char *s2 = one("SELECT story_id FROM intel_items WHERE uid='u|a2'");
  assert(s1 && s2 && !strcmp(s1, s2) && !strncmp(s1, "st-", 3));
  assert(!one("SELECT story_id FROM intel_items WHERE uid='u|a3'"));
  assert(!one("SELECT story_id FROM intel_items WHERE uid='u|b1'"));
  assert(!one("SELECT story_id FROM intel_items WHERE uid='u|t1'"));
  printf("  story: twins within 72 h share %s; 10-day-old copy, unrelated item "
         "and the other tenant's identical row stay out: ok\n", s1);

  /* 4 */
  char *n1 = one("SELECT novelty FROM intel_items WHERE uid='u|a1'");
  char *n2 = one("SELECT novelty FROM intel_items WHERE uid='u|a2'");
  assert(n1 && n2);
  assert(atof(n1) < 0.2 && atof(n2) > 0.999);
  printf("  novelty: later twin %.3f, earlier twin %.3f (only earlier items "
         "count): ok\n", atof(n1), atof(n2));
  free(n1); free(n2);

  /* 5: a pending row is processed by a later call (n == 0 drains) */
  sql("UPDATE intel_items SET novelty=NULL WHERE uid='u|b1'");
  {
    sqlite3_stmt *s;
    assert(sqlite3_prepare_v2(g_db.h, "INSERT INTO story_pending(uid,tenant_id,ts)"
        " SELECT uid,tenant_id,ts FROM intel_vec_v2 WHERE uid='u|b1'", -1, &s, NULL) == SQLITE_OK);
    assert(sqlite3_step(s) == SQLITE_DONE);
    sqlite3_finalize(s);
  }
  int dim = (int)vecindex_meta_long(&g_db, &embed_intel_spec, "dim");
  story_process_batch(&g_db, 0, NULL, NULL, dim, NULL, NULL);
  assert(!one("SELECT uid FROM story_pending"));
  char *nb = one("SELECT novelty FROM intel_items WHERE uid='u|b1'");
  assert(nb && atof(nb) > 0.5);
  free(nb);
  printf("  pending row drained by a later call, novelty written: ok\n");

  /* 6 */
  int st = 0;
  char *js = storyapi_get(&g_db, "legacy", s1, "limit=1", 0, &st);
  assert(st == 200 && js);
  cJSON *o = cJSON_Parse(js);
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(o, "members")) == 1);
  assert(cJSON_GetObjectItem(cJSON_GetObjectItem(o, "page"), "total")->valueint == 2);
  assert(cJSON_GetObjectItem(cJSON_GetObjectItem(o, "meta"), "tau")->valuedouble > 0.8);
  cJSON_Delete(o); free(js);
  js = storyapi_get(&g_db, "legacy", "st-0000000000000000", "", 0, &st);
  assert(st == 404); free(js);
  js = storyapi_get(&g_db, "legacy", "nope", "", 0, &st);
  assert(st == 400); free(js);
  printf("  /api/stories/:id: paged members with measured total, 404, 400: ok\n");

  free(s1); free(s2);
  estub_stop();
  db_close(&g_db);
  printf("test_story: ok\n");
  return 0;
}
