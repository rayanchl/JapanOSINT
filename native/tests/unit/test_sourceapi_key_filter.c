/* tests/unit/test_sourceapi_key_filter.c — GET /api/sources/search?key=<NAME>
 * (WEB-05): the sources that need one credential, and how many.
 *
 * The mapping is the registry's own, never a table of ours: an hpengine row's
 * key_env, the credtab req/any rows, and what a run recorded (a gated notice's
 * env_vars_required). The expected set is computed here from those same three
 * declarations, independently of core/sourceapi.c, and the response must equal
 * it. Pinned:
 *   - an hpengine key_env row is found by its key;
 *   - a credtab req row is found, an `opt` row is NOT ("works without it");
 *   - a recorded gated notice adds a source the declarations do not name;
 *   - meta.credential.sources is the matched total (rule 2: stated in-band) and
 *     page.total agrees, also when the page is smaller than the match;
 *   - an unknown key is an honest empty page that still says 0 matched;
 *   - the filter is exact: a prefix of a key matches nothing.
 *
 * JO_DB is set by tests/unit/run.sh to a scratch file. */
#include "../../core/sourceapi.h"
#include "../../core/credtab.h"
#include "../../core/db.h"
#include "../../source.h"
#include "../../lib/hpengine.h"
#include "../../third_party/cJSON.h"
#include "../../third_party/sqlite3.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const cJSON *get(const cJSON *o, const char *path) {
  char buf[256]; snprintf(buf, sizeof buf, "%s", path);
  const cJSON *cur = o;
  for (char *tok = strtok(buf, "."); tok && cur; tok = strtok(NULL, "."))
    cur = cJSON_GetObjectItem(cur, tok);
  return cur;
}
static double num(const cJSON *o, const char *path) {
  const cJSON *v = get(o, path);
  assert(v && cJSON_IsNumber(v));
  return v->valuedouble;
}
static cJSON *search(db_handle *db, const char *key, int limit) {
  sourceapi_query q = { .credkey = key, .limit = limit, .sort = "id" };
  char *j = sourceapi_search(db, &q);
  cJSON *o = cJSON_Parse(j);
  free(j);
  assert(o);
  return o;
}
static int in_list(const char *const *a, const char *k) {
  for (int i = 0; a[i]; i++) if (!strcmp(a[i], k)) return 1;
  return 0;
}
/* The declared need, from the registry alone (no DB). */
static int declared(const source_def *d, const char *key) {
  if (d->engine && !strcmp(d->engine, "hpengine")) {
    const hp_source *h = hp_source_get(d->id);
    if (h && h->key_env && !strcmp(h->key_env, key)) return 1;
  }
  const cred_def *c = cred_get(d->id);
  return c && (in_list(c->req, key) || in_list(c->any, key));
}
static int expected(const char *key, const char *also_id) {
  const source_def **a = registry_all();
  int n = 0;
  for (int i = 0; i < registry_count(); i++)
    if (declared(a[i], key) || (also_id && !strcmp(a[i]->id, also_id))) n++;
  return n;
}
static int has_row(const cJSON *o, const char *id) {
  const cJSON *r;
  cJSON_ArrayForEach(r, get(o, "data")) {
    const cJSON *i = cJSON_GetObjectItem(r, "id");
    if (i && cJSON_IsString(i) && !strcmp(i->valuestring, id)) return 1;
  }
  return 0;
}
static void insert_item(db_handle *db, const char *src, const char *uid, const char *rt,
                        const char *props, const char *fetched) {
  sqlite3_stmt *s;
  assert(sqlite3_prepare_v2(db->h,
    "INSERT OR REPLACE INTO intel_items(uid,source_id,title,fetched_at,record_type,properties) "
    "VALUES(?1,?2,?3,?4,?5,?6)", -1, &s, NULL) == SQLITE_OK);
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 2, src, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 3, "notice", -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 4, fetched, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 5, rt, -1, SQLITE_STATIC);
  sqlite3_bind_text(s, 6, props, -1, SQLITE_STATIC);
  assert(sqlite3_step(s) == SQLITE_DONE);
  sqlite3_finalize(s);
}

int main(void) {
  db_handle db;
  assert(db_open(&db, NULL, NULL) == 0);

  /* A source no declaration names, given a recorded gated notice BEFORE the
   * first search (the snapshot is cached). */
  const char *ghost = "JO_TEST_ONLY_KEY_7";
  const source_def *plain = NULL;
  {
    const source_def **al = registry_all();
    for (int i = 0; i < registry_count() && !plain; i++)
      if (!cred_get(al[i]->id) && !(al[i]->engine && !strcmp(al[i]->engine, "hpengine")))
        plain = al[i];
  }
  assert(plain);
  insert_item(&db, plain->id, "ck1", "collector-status-notice",
              "{\"status\":\"needs_credential\",\"env_vars_required\":[\"JO_TEST_ONLY_KEY_7\",\"JO_OTHER\"]}",
              "2026-10-22T10:00:00Z");

  /* an hpengine row that declares key_env, and a credtab req row, from the
   * registry as it stands */
  const source_def *hpk = NULL, *crq = NULL;
  const source_def **all = registry_all();
  for (int i = 0; i < registry_count(); i++) {
    const source_def *d = all[i];
    const cred_def *c = cred_get(d->id);
    const hp_source *h = (d->engine && !strcmp(d->engine, "hpengine")) ? hp_source_get(d->id) : NULL;
    int hp_key = h && h->key_env && *h->key_env;
    /* each declaration is proved on a row only IT names */
    if (!hpk && hp_key && !c) hpk = d;
    if (!crq && c && c->req[0] && !hp_key) crq = d;
  }
  assert(hpk && "no hpengine row declares key_env?");
  assert(crq && "no credtab row requires a key?");
  const char *hpkey = hp_source_get(hpk->id)->key_env;
  const char *crkey = cred_get(crq->id)->req[0];
  printf("hp row %s needs %s; credtab row %s needs %s\n", hpk->id, hpkey, crq->id, crkey);

  /* 1. an hpengine key_env row is found by its key */
  cJSON *o = search(&db, hpkey, 500);
  assert(has_row(o, hpk->id) && "key_env row found");
  assert((int)num(o, "meta.credential.sources") == (int)num(o, "page.total"));
  assert((int)num(o, "page.total") == expected(hpkey, NULL) &&
         "the set equals the registry's own declarations");
  assert(!strcmp(get(o, "meta.filters.key")->valuestring, hpkey));
  cJSON_Delete(o);

  /* 2. a credtab req row is found */
  o = search(&db, crkey, 500);
  assert(has_row(o, crq->id) && "credtab required row found");
  assert((int)num(o, "page.total") == expected(crkey, NULL));
  cJSON_Delete(o);

  /* 3. an OPTIONAL var is not "needs": no credtab row lists it under req/any
   * and nothing else declares it, so a name only ever optional matches only
   * what declared() says */
  const char *optkey = NULL;
  const source_def *optsrc = NULL;
  for (int i = 0; i < registry_count() && !optkey; i++) {
    const cred_def *c = cred_get(all[i]->id);
    if (!c) continue;
    for (int k = 0; c->opt[k]; k++)
      if (!declared(all[i], c->opt[k])) { optkey = c->opt[k]; optsrc = all[i]; break; }
  }
  if (optkey) {
    o = search(&db, optkey, 500);
    assert(!has_row(o, optsrc->id) && "an optional credential is not a need");
    assert((int)num(o, "page.total") == expected(optkey, NULL));
    cJSON_Delete(o);
  }

  /* 4. the recorded notice (inserted before the first search: the snapshot is
   * cached for ~10 s) names a source no declaration does */
  o = search(&db, ghost, 50);
  assert(has_row(o, plain->id) && (int)num(o, "page.total") == 1 &&
         "the recorded notice names the source");
  cJSON_Delete(o);
  o = search(&db, "JO_TEST_ONLY_KEY", 50);               /* a prefix is not the key */
  assert((int)num(o, "page.total") == 0 && "exact, not a substring");
  cJSON_Delete(o);
  o = search(&db, "JO_NO_SUCH_KEY_AT_ALL", 50);
  assert((int)num(o, "page.total") == 0 && (int)num(o, "meta.credential.sources") == 0 &&
         "an unknown key is an honest empty page that says 0");
  assert(cJSON_GetArraySize(get(o, "data")) == 0);
  cJSON_Delete(o);

  /* 5. the matched total is stated even when the page is smaller (rule 2) */
  const char *many = NULL; int most = 0;
  for (int i = 0; i < registry_count(); i++) {
    const cred_def *c = cred_get(all[i]->id);
    if (c && c->req[0]) {
      int e = expected(c->req[0], NULL);
      if (e > most) { most = e; many = c->req[0]; }
    }
  }
  if (most >= 2) {
    o = search(&db, many, 1);
    assert(cJSON_GetArraySize(get(o, "data")) == 1);
    assert((int)num(o, "meta.credential.sources") == most && (int)num(o, "page.total") == most &&
           (int)num(o, "meta.credential.shown") == 1 &&
           "1 shown of N matched, said in the response");
    assert(cJSON_IsTrue(get(o, "page.has_more")));
    cJSON_Delete(o);
  }

  /* 6. no key filter: no meta.credential block (the field is not invented) */
  sourceapi_query q = { .limit = 1 };
  char *j = sourceapi_search(&db, &q);
  o = cJSON_Parse(j); free(j);
  assert(!get(o, "meta.credential"));
  cJSON_Delete(o);

  db_close(&db);
  printf("test_sourceapi_key_filter: all passed\n");
  return 0;
}
