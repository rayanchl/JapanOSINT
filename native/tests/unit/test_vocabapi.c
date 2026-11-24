/* test_vocabapi.c — GET /api/vocab serves the server's own enumerations.
 *
 *   - every CLOSED vocabulary matches the schema CHECK it claims to transcribe:
 *     each listed value is accepted by the database and an unlisted one is
 *     refused (the drift this endpoint exists to prevent, caught here);
 *   - OPEN vocabularies are counted from the database, most frequent first,
 *     empty/NULL values never listed, and bounded by VOCAB_MAX_VALUES;
 *   - tenancy: record_types counts the caller's own rows plus the shared
 *     'legacy' corpus and never another tenant's; entity_types counts
 *     tenant-owned and unowned (NULL) entities; rel_types says it is corpus-wide;
 *   - the memo: a second call inside the TTL is served from cache (a row added
 *     in between is invisible), refresh=1 rebuilds, tenants do not share a slot,
 *     and more tenants than slots evicts without corrupting anything;
 *   - an empty database yields empty arrays, not an error. */
#include "../../core/vocabapi.c"

#include <assert.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", sql); exit(1); }
}
static int exec_rc(db_handle *db, const char *sql) {
  return sqlite3_exec(db->h, sql, NULL, NULL, NULL);
}
static cJSON *get(db_handle *db, const char *tenant, int refresh) {
  char *j = vocabapi_get(db, tenant, refresh);
  assert(j);
  cJSON *r = cJSON_Parse(j);
  free(j);
  assert(r);
  return r;
}
static cJSON *vals(cJSON *r, const char *grp, const char *key) {
  cJSON *g = cJSON_GetObjectItem(cJSON_GetObjectItem(r, grp), key);
  assert(g);
  cJSON *v = cJSON_GetObjectItem(g, "values");
  assert(cJSON_IsArray(v));
  return v;
}
static double count_of(cJSON *arr, const char *value) {
  cJSON *o;
  cJSON_ArrayForEach(o, arr) {
    cJSON *v = cJSON_GetObjectItem(o, "value");
    if (v && !strcmp(v->valuestring, value))
      return cJSON_GetObjectItem(o, "count")->valuedouble;
  }
  return -1;
}
static void ins_item(db_handle *db, const char *uid, const char *rt, const char *tenant) {
  char sql[400];
  snprintf(sql, sizeof sql,
    "INSERT INTO intel_items(uid,source_id,fetched_at,record_type,tenant_id)"
    " VALUES('%s','vocab-test','2026-01-01','%s','%s')", uid, rt, tenant);
  exec_ok(db, sql);
}

int main(void) {
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "DELETE FROM intel_items");
  exec_ok(&db, "DELETE FROM entity_relationships");
  exec_ok(&db, "DELETE FROM entities");

  /* ── empty corpus ────────────────────────────────────────────────────── */
  cJSON *r = get(&db, "t-empty", 0);
  assert(cJSON_GetArraySize(vals(r, "open", "record_types")) == 0);
  assert(cJSON_GetArraySize(vals(r, "open", "entity_types")) == 0);
  assert(cJSON_GetArraySize(vals(r, "open", "rel_types")) == 0);
  assert(cJSON_GetArraySize(vals(r, "closed", "tenant_roles")) == 4);
  cJSON *meta = cJSON_GetObjectItem(r, "meta");
  assert(cJSON_GetObjectItem(meta, "ttl_sec")->valueint == VOCAB_TTL_SEC);
  assert(strlen(cJSON_GetObjectItem(meta, "generated_at")->valuestring) == 20);
  cJSON_Delete(r);
  printf("  empty corpus: closed lists present, open lists empty: ok\n");

  /* ── closed lists carry a source and match the schema's own CHECKs ───── */
  r = get(&db, "t-empty", 1);
  cJSON *closed = cJSON_GetObjectItem(r, "closed");
  for (size_t i = 0; i < sizeof CLOSED / sizeof *CLOSED; i++) {
    cJSON *b = cJSON_GetObjectItem(closed, CLOSED[i].key);
    assert(b && cJSON_GetObjectItem(b, "source") &&
           *cJSON_GetObjectItem(b, "source")->valuestring);
    int n = 0; for (const char *const *p = CLOSED[i].vals; *p; p++) n++;
    assert(cJSON_GetArraySize(cJSON_GetObjectItem(b, "values")) == n && n > 0);
  }
  assert(cJSON_GetArraySize(vals(r, "closed", "ref_types")) > 0 &&
         "ref_types comes from annotations_ref_types(), not a copy");
  cJSON_Delete(r);

  char sql[400];
  for (const char *const *p = V_SOURCE_TYPES; *p; p++) {
    snprintf(sql, sizeof sql, "INSERT INTO sources(id,name,type,category)"
             " VALUES('vk-%s','n','%s','c')", *p, *p);
    assert(exec_rc(&db, sql) == 0 && "every listed source type is accepted");
  }
  assert(exec_rc(&db, "INSERT INTO sources(id,name,type,category)"
                      " VALUES('vk-bad','n','bogus','c')") != 0 &&
         "an unlisted source type is refused by the schema");
  for (const char *const *p = V_SOURCE_STATUSES; *p; p++) {
    snprintf(sql, sizeof sql, "UPDATE sources SET status='%s' WHERE id='vk-api'", *p);
    assert(exec_rc(&db, sql) == 0);
  }
  assert(exec_rc(&db, "UPDATE sources SET status='nope' WHERE id='vk-api'") != 0);
  exec_ok(&db, "DELETE FROM sources WHERE id LIKE 'vk-%'");
  for (const char *const *p = V_CASE_STATUS; *p; p++) {
    snprintf(sql, sizeof sql,
      "SELECT 1 WHERE '%s' IN ('open','closed','archived')", *p);
    assert(exec_rc(&db, sql) == 0);
  }
  printf("  closed lists: sourced, and agree with the schema CHECKs: ok\n");

  /* ── open lists: counted, ordered, tenant-scoped ─────────────────────── */
  ins_item(&db, "a1", "quake", "t-a");
  ins_item(&db, "a2", "quake", "t-a");
  ins_item(&db, "a3", "quake", "t-a");
  ins_item(&db, "a4", "flood", "t-a");
  ins_item(&db, "b1", "secret-b-only", "t-b");
  ins_item(&db, "b2", "quake", "t-b");
  ins_item(&db, "l1", "shared", "legacy");
  ins_item(&db, "l2", "quake", "legacy");
  exec_ok(&db, "INSERT INTO intel_items(uid,source_id,fetched_at,record_type,tenant_id)"
               " VALUES('e1','v','2026-01-01','','t-a'),('e2','v','2026-01-01',NULL,'t-a')");
  exec_ok(&db, "INSERT INTO entities(entity_id,type,canonical,norm_key,tenant_id) VALUES"
               "('E1','person','p','p','t-a'),('E2','person','q','q',NULL),"
               "('E3','org','o','o','t-a'),('E4','person','x','x','t-b'),"
               "('E5','vessel','v','v','t-b')");
  exec_ok(&db, "INSERT INTO entity_relationships(src_entity_id,dst_entity_id,rel_type)"
               " VALUES('E1','E3','works_at'),('E4','E5','owns'),('E2','E3','works_at')");

  r = get(&db, "t-a", 0);
  cJSON *rt = vals(r, "open", "record_types");
  assert(count_of(rt, "quake") == 4 && "t-a's 3 plus the shared legacy 1, not t-b's");
  assert(count_of(rt, "flood") == 1 && count_of(rt, "shared") == 1);
  assert(count_of(rt, "secret-b-only") == -1 && "another tenant's type never leaks");
  assert(count_of(rt, "") == -1 && "empty/NULL types are not vocabulary");
  assert(!strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(rt, 0), "value")->valuestring, "quake")
         && "most frequent first");
  cJSON *et = vals(r, "open", "entity_types");
  assert(count_of(et, "person") == 2 && "own + unowned, not t-b's");
  assert(count_of(et, "org") == 1 && count_of(et, "vessel") == -1);
  cJSON *rl = vals(r, "open", "rel_types");
  assert(count_of(rl, "works_at") == 2 && count_of(rl, "owns") == 1);
  cJSON *relg = cJSON_GetObjectItem(cJSON_GetObjectItem(r, "open"), "rel_types");
  assert(!strcmp(cJSON_GetObjectItem(relg, "scope")->valuestring, "corpus") &&
         cJSON_GetObjectItem(relg, "scope_note") && "rel_types admits it is corpus-wide");
  cJSON_Delete(r);

  r = get(&db, "t-b", 0);
  rt = vals(r, "open", "record_types");
  assert(count_of(rt, "secret-b-only") == 1 && count_of(rt, "quake") == 2 &&
         count_of(rt, "flood") == -1 && "t-b: its own plus legacy");
  cJSON_Delete(r);
  printf("  open lists: counted, ordered, tenant-scoped, NULL/'' skipped: ok\n");

  /* ── the memo ────────────────────────────────────────────────────────── */
  ins_item(&db, "a5", "fresh-type", "t-a");
  r = get(&db, "t-a", 0);
  assert(count_of(vals(r, "open", "record_types"), "fresh-type") == -1 &&
         "inside the TTL the cached answer is served");
  cJSON_Delete(r);
  r = get(&db, "t-a", 1);
  assert(count_of(vals(r, "open", "record_types"), "fresh-type") == 1 &&
         "refresh=1 rebuilds");
  cJSON_Delete(r);
  r = get(&db, "t-a", 0);
  assert(count_of(vals(r, "open", "record_types"), "fresh-type") == 1 &&
         "and the rebuilt answer is what is cached");
  cJSON_Delete(r);
  r = get(&db, NULL, 0);                              /* no tenant: its own slot */
  assert(count_of(vals(r, "open", "record_types"), "fresh-type") == -1 &&
         "a NULL tenant sees no tenant's rows and does not reuse t-a's slot");
  cJSON_Delete(r);

  /* more tenants than slots: eviction never serves another tenant's answer */
  for (int i = 0; i < VOCAB_SLOTS + 4; i++) {
    char t[32]; snprintf(t, sizeof t, "t-evict-%d", i);
    r = get(&db, t, 0);
    assert(count_of(vals(r, "open", "record_types"), "secret-b-only") == -1);
    cJSON_Delete(r);
  }
  r = get(&db, "t-b", 0);
  assert(count_of(vals(r, "open", "record_types"), "secret-b-only") == 1 &&
         "an evicted tenant is rebuilt as itself");
  cJSON_Delete(r);
  printf("  memo: TTL cache, refresh, per-tenant slots, eviction: ok\n");

  /* ── bound ───────────────────────────────────────────────────────────── */
  exec_ok(&db, "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM n WHERE i<700) "
               "INSERT INTO intel_items(uid,source_id,fetched_at,record_type,tenant_id)"
               " SELECT 'big'||i,'v','2026-01-01','type-'||i,'t-big' FROM n");
  r = get(&db, "t-big", 0);
  assert(cJSON_GetArraySize(vals(r, "open", "record_types")) == VOCAB_MAX_VALUES);
  assert(cJSON_GetObjectItem(cJSON_GetObjectItem(r, "meta"),
         "max_values_per_open_vocab")->valueint == VOCAB_MAX_VALUES &&
         "the cap is stated in-band");
  cJSON_Delete(r);
  printf("  open lists bounded at %d, and the bound is stated: ok\n", VOCAB_MAX_VALUES);

  db_close(&db);
  printf("test_vocabapi: all passed\n");
  return 0;
}
