/* test_tenant_pivot_key.c — a workspace's saved key is used for ITS on-demand
 * pivots, and is never stored (2026-11-12, the repository owner's decision:
 * "use them for pivots").
 *
 * Before this, /api/tenant-keys stored per-workspace keys (encrypted,
 * core/keysapi.c) and nothing read them: every pivot resolved key_env from the
 * process environment only, so a workspace that saved its own key was told
 * "set" and still ran on the platform's key — or on none. Pinned here:
 *   1. keysapi_tenant_secret() returns the workspace's saved plaintext, and
 *      nothing for another workspace, the shared corpus ("legacy"), or a key
 *      too short to scrub safely;
 *   2. the scrub sink replaces the key in every string field, passes an item
 *      without it through untouched, and is a pass-through with no key;
 *   3. END TO END through osint_dispatch_tenant(): an hp pivot row whose URL
 *      carries {key} is dispatched for workspace tA. Its fetch is refused (the
 *      default outbound floor refuses 127.0.0.1) and hpengine files a
 *      collector-fetch-failure record naming the URL — so the stored row is
 *      exactly the place a key would leak. The tenant key was USED (the
 *      environment's value is absent) and is NOT stored ("[redacted]" is);
 *      dispatched with no tenant, or for a workspace with no saved key, the
 *      same row uses the environment's value. The env variable deliberately
 *      has a name core/intel.c does not treat as secret, so its value staying
 *      visible proves which key the request carried;
 *   4. WHERE the rows go (2026-11-14, the owner's decision "workspace-private"):
 *      the run made with tA's own key stores under tenant tA and a
 *      workspace-qualified uid ("<source>|ws:tA|…"), so it neither lands in nor
 *      overwrites the shared corpus; runs with no workspace key (the env's, or
 *      tB with none saved) store in the shared corpus ("legacy") as before;
 *   5. a HAND-WRITTEN collector (2026-11-16) that reads its credential through
 *      jo_getenv() gets the workspace's key the same way, with the same
 *      privacy and scrubbing: stored under tA and a "ws:" uid, the key
 *      redacted; with no workspace, the environment's value and the shared
 *      corpus; and the resolver is cleared when the run ends;
 *   6. a stray tenant_secrets row under a CONFIGURATION name
 *      (JO_HTTP_BLOCK_PRIVATE) never resolves, so a workspace cannot reach a
 *      collector's configuration through its key store. */
#include "core/keysapi.h"
#include "core/osint_dispatch.h"
#include "core/intel.h"
#include "core/db.h"
#include "lib/hpengine.h"
#include "third_party/cJSON.h"
#include "third_party/sqlite3.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_ENV   "JO_TEST_PIVOT_CRED"        /* not *_KEY: intel.c leaves it */
#define ENV_VAL   "envcred-0123456789"
#define TENANT_A  "tenantcred-ABCDEFGHIJ"

static const hp_source T_ROWS[] = {
  { .id = "T_TENANT_PIVOT_KEY", .name = "tenant pivot key test row",
    .collector = "test_tenant_pivot_key", .category = "test",
    .record_type = "test-record", .mode = HP_JSON, .want = HP_ANY,
    .url = "http://127.0.0.1:9/lookup?q={q}&api_key={key}",
    .key_env = KEY_ENV },
};
HP_REGISTER_TABLE(T_ROWS)

/* A hand-written pivot: reads its credential the way the bespoke collectors
 * do, and puts it in the record's link — exactly where a key leaks. */
static int run_bespoke(const source_ctx *c, intel_sink *s) {
  const char *k = jo_getenv(KEY_ENV);
  char link[256], key[160];
  snprintf(link, sizeof link, "https://bespoke.example/?q=%s&api_key=%s",
           c->entity ? c->entity : "", k ? k : "none");
  snprintf(key, sizeof key, "bespoke-%s", c->entity ? c->entity : "");
  intel_item it = {0};
  it.remote_key = key;
  it.title = c->entity;
  it.link = link;
  it.record_type = "test-record";
  return s->emit(s, &it) > 0 ? 0 : -1;
}
static const source_def t_bespoke = {
  .id = "T_TENANT_BESPOKE", .collector = "osint_test", .name = "bespoke key test",
  .update_interval_sec = 0, .run = run_bespoke, .category = "test" };
REGISTER_SOURCE(t_bespoke)

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", sql); exit(1); }
}

/* Every text column of every stored row of the test source, concatenated. */
static char *stored_text(db_handle *db) {
  sqlite3_stmt *s;
  assert(sqlite3_prepare_v2(db->h,
    "SELECT * FROM intel_items WHERE source_id='T_TENANT_PIVOT_KEY'",
    -1, &s, NULL) == SQLITE_OK);
  size_t cap = 1 << 16, n = 0;
  char *out = calloc(1, cap);
  while (sqlite3_step(s) == SQLITE_ROW)
    for (int i = 0; i < sqlite3_column_count(s); i++) {
      const char *t = (const char *)sqlite3_column_text(s, i);
      if (!t) continue;
      size_t l = strlen(t);
      while (n + l + 2 > cap) { cap *= 2; out = realloc(out, cap); }
      memcpy(out + n, t, l); n += l; out[n++] = '\n'; out[n] = 0;
    }
  sqlite3_finalize(s);
  return out;
}

/* Rows of the test source whose entity text contains `needle`, by tenant. */
static long rows_for(db_handle *db, const char *needle, const char *tenant,
                     int ws_uid) {
  char q[512];
  snprintf(q, sizeof q,
    "SELECT count(*) FROM intel_items WHERE source_id='T_TENANT_PIVOT_KEY' "
    "AND (title LIKE '%%%s%%' OR body LIKE '%%%s%%' OR properties LIKE '%%%s%%') "
    "AND tenant_id='%s' AND (%d=0 OR uid LIKE 'T_TENANT_PIVOT_KEY|ws:%s|%%')",
    needle, needle, needle, tenant, ws_uid, tenant);
  sqlite3_stmt *s; long v = -1;
  assert(sqlite3_prepare_v2(db->h, q, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) v = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}

/* A sink that keeps the last item's link and properties. */
static char g_link[512], g_props[512];
static int g_emits;
static int keep_emit(intel_sink *self, const intel_item *it) {
  (void)self;
  g_emits++;
  snprintf(g_link, sizeof g_link, "%s", it->link ? it->link : "");
  snprintf(g_props, sizeof g_props, "%s", it->properties_json ? it->properties_json : "");
  return 1;
}

static void run_pivot(db_handle *db, const char *entity, const char *tenant,
                      osint_result *r) {
  intel_sink sink = intel_sink_make(db, "osint-search", "legacy");
  if (tenant) osint_dispatch_tenant(db, NULL, "T_TENANT_PIVOT_KEY", entity,
                                    "name", &sink, r, tenant);
  else        osint_dispatch(db, NULL, "T_TENANT_PIVOT_KEY", entity, "name",
                             &sink, r);
  intel_sink_free(&sink);
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  /* The fetch is meant to be REFUSED by the default floor; make sure no
   * environment left by a caller lowers it. */
  unsetenv("JO_HTTP_BLOCK_PRIVATE");
  setenv("SECRETS_MASTER_KEY",
         "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff", 1);
  setenv(KEY_ENV, ENV_VAL, 1);
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "INSERT OR IGNORE INTO tenants(id,slug,name) VALUES"
               " ('tA','ta','A'),('tB','tb','B')");

  /* Saved the way the settings page saves it: PUT /api/tenant-keys/<name>.
   * The row's key_env is a known name because a registered row declares it. */
  tenant_ctx ta = {0};
  snprintf(ta.tenant_id, sizeof ta.tenant_id, "tA");
  snprintf(ta.user_id, sizeof ta.user_id, "uA");
  snprintf(ta.role, sizeof ta.role, "owner");
  int st = 0;
  char *b = keysapi_tenant(&db, &ta, "PUT", KEY_ENV,
                           "{\"value\":\"" TENANT_A "\"}", &st);
  assert(b && st == 200);
  free(b);

  /* 1. the lookup */
  char *k = keysapi_tenant_secret(&db, "tA", KEY_ENV);
  assert(k && !strcmp(k, TENANT_A));
  free(k);
  assert(!keysapi_tenant_secret(&db, "tB", KEY_ENV));
  assert(!keysapi_tenant_secret(&db, "legacy", KEY_ENV));
  assert(!keysapi_tenant_secret(&db, "tA", "SOME_OTHER_KEY"));
  b = keysapi_tenant(&db, &ta, "PUT", KEY_ENV, "{\"value\":\"short7c\"}", &st);
  assert(b && st == 200); free(b);
  assert(!keysapi_tenant_secret(&db, "tA", KEY_ENV) && "too short to scrub: not used");
  b = keysapi_tenant(&db, &ta, "PUT", KEY_ENV, "{\"value\":\"" TENANT_A "\"}", &st);
  assert(b && st == 200); free(b);
  printf("  workspace key resolves for its own workspace only: ok\n");

  /* 2. the scrub sink */
  { intel_sink keep = { NULL, keep_emit };
    keysapi_scrub_sink w;
    keysapi_scrub_sink_init(&w, &keep, TENANT_A);
    intel_item it = {0};
    it.link = "https://x.example/?api_key=" TENANT_A "&k=" TENANT_A;
    it.properties_json = "{\"url\":\"https://x.example/?api_key=" TENANT_A "\"}";
    assert(w.base.emit(&w.base, &it) == 1);
    assert(!strstr(g_link, TENANT_A) && !strstr(g_props, TENANT_A));
    assert(!strcmp(g_link, "https://x.example/?api_key=[redacted]&k=[redacted]"));
    intel_item plain = {0};
    plain.link = "https://x.example/clean";
    assert(w.base.emit(&w.base, &plain) == 1 && !strcmp(g_link, plain.link));
    keysapi_scrub_sink_init(&w, &keep, NULL);
    assert(w.base.emit(&w.base, &it) == 1 && strstr(g_link, TENANT_A)
           && "no key: a pass-through");
    assert(g_emits == 3); }
  printf("  scrub sink redacts the key in every field: ok\n");

  /* 3. end to end */
  osint_result r;
  run_pivot(&db, "acme-tenant", "tA", &r);
  char *txt = stored_text(&db);
  assert(strstr(txt, "acme-tenant") && "the fetch-failure record was stored");
  assert(!strstr(txt, TENANT_A) && "the workspace key is never stored");
  assert(!strstr(txt, ENV_VAL) && "the workspace key, not the env's, was sent");
  assert(strstr(txt, "[redacted]"));
  assert(!r.data || !strstr(r.data, TENANT_A));
  assert(!r.error || !strstr(r.error, TENANT_A));
  assert(rows_for(&db, "acme-tenant", "tA", 1) >= 1 && "stored as tA's, under a ws uid");
  assert(rows_for(&db, "acme-tenant", "legacy", 0) == 0 && "and not in the shared corpus");
  free(txt);
  osint_result_free(&r);

  run_pivot(&db, "acme-env", NULL, &r);
  txt = stored_text(&db);
  assert(strstr(txt, "acme-env") && strstr(txt, ENV_VAL)
         && "no workspace: the environment's key, as before");
  assert(rows_for(&db, "acme-env", "legacy", 0) >= 1 && "shared, as before");
  free(txt);
  osint_result_free(&r);

  run_pivot(&db, "acme-tb", "tB", &r);
  txt = stored_text(&db);
  assert(strstr(txt, "acme-tb") && !strstr(txt, TENANT_A)
         && "a workspace with no saved key falls back to the environment");
  assert(rows_for(&db, "acme-tb", "legacy", 0) >= 1 && rows_for(&db, "acme-tb", "tB", 0) == 0
         && "no workspace key used: the shared corpus");
  free(txt);
  osint_result_free(&r);

  /* 5. the hand-written collector */
  { intel_sink sink = intel_sink_make(&db, "osint-search", "legacy");
    osint_dispatch_tenant(&db, NULL, "T_TENANT_BESPOKE", "acme-bespoke", "name",
                          &sink, &r, "tA");
    intel_sink_free(&sink); }
  { sqlite3_stmt *q;
    assert(sqlite3_prepare_v2(db.h,
      "SELECT tenant_id, uid, link FROM intel_items WHERE source_id='T_TENANT_BESPOKE' "
      "AND title='acme-bespoke'", -1, &q, NULL) == SQLITE_OK);
    int rows = 0;
    while (sqlite3_step(q) == SQLITE_ROW) {
      rows++;
      const char *tn = (const char *)sqlite3_column_text(q, 0);
      const char *uid = (const char *)sqlite3_column_text(q, 1);
      const char *ln = (const char *)sqlite3_column_text(q, 2);
      assert(tn && !strcmp(tn, "tA") && "the workspace's own row");
      assert(uid && strstr(uid, "|ws:tA|") && "under a workspace uid");
      assert(ln && strstr(ln, "api_key=[redacted]") && "the workspace key was used and scrubbed");
      assert(!strstr(ln, TENANT_A) && !strstr(ln, ENV_VAL));
    }
    sqlite3_finalize(q);
    assert(rows == 1); }
  assert(!r.data || !strstr(r.data, TENANT_A));
  osint_result_free(&r);
  { intel_sink sink = intel_sink_make(&db, "osint-search", "legacy");
    osint_dispatch(&db, NULL, "T_TENANT_BESPOKE", "acme-bespoke-env", "name", &sink, &r);
    intel_sink_free(&sink); }
  { sqlite3_stmt *q;
    assert(sqlite3_prepare_v2(db.h,
      "SELECT tenant_id, link FROM intel_items WHERE source_id='T_TENANT_BESPOKE' "
      "AND title='acme-bespoke-env'", -1, &q, NULL) == SQLITE_OK);
    assert(sqlite3_step(q) == SQLITE_ROW);
    assert(!strcmp((const char *)sqlite3_column_text(q, 0), "legacy"));
    assert(strstr((const char *)sqlite3_column_text(q, 1), ENV_VAL)
           && "no workspace: the environment's key, shared corpus");
    sqlite3_finalize(q); }
  osint_result_free(&r);
  assert(jo_getenv(KEY_ENV) && !strcmp(jo_getenv(KEY_ENV), ENV_VAL)
         && "the resolver is cleared when the run ends");
  printf("  a hand-written collector gets the workspace key through jo_getenv, privately: ok\n");

  /* 6. a configuration name never resolves from the key store */
  exec_ok(&db, "INSERT OR REPLACE INTO tenant_secrets(tenant_id,key_name,encrypted_value) "
               "SELECT tenant_id,'JO_HTTP_BLOCK_PRIVATE',encrypted_value FROM tenant_secrets "
               "WHERE tenant_id='tA' AND key_name='" KEY_ENV "'");
  assert(!keysapi_tenant_secret(&db, "tA", "JO_HTTP_BLOCK_PRIVATE"));
  printf("  a stray key-store row under a configuration name never resolves: ok\n");

  /* nothing left behind for the next run on this thread */
  run_pivot(&db, "acme-after", NULL, &r);
  txt = stored_text(&db);
  assert(strstr(txt, "acme-after") && !strstr(txt, TENANT_A));
  free(txt);
  osint_result_free(&r);
  printf("  a pivot for workspace tA used its key and stored none of it: ok\n");
  printf("  its records are tA's own; runs without a workspace key stay shared: ok\n");

  db_close(&db);
  printf("test_tenant_pivot_key: all passed\n");
  return 0;
}
