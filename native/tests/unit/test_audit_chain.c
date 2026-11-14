/* test_audit_chain.c — audit_write() chains, and /api/audit/verify checks it.
 *
 * H5: audit_write() and member_audit() left prev_hash/row_hash/chain_seq NULL
 * and the verifier walked only rows WHERE row_hash IS NOT NULL, so every
 * tenant's chain "verified" over zero rows. Pinned here:
 *   - N writes verify ok over N rows, per tenant, interleaved tenants apart;
 *   - legacy NULL-hash rows are reported as "unchained", not skipped silently;
 *   - an edited row breaks the walk AT that row, a deleted row as a gap;
 *   - four writers on four connections cannot fork one tenant's chain;
 *   - (2026-11-12) GET /api/audit[/verify]?scope=platform reads the reserved
 *     "platform" chain for a PLATFORM OPERATOR only: tenantapi_audit_chain()
 *     is the route's decision (core/httpd.c), 403 for anyone else, 400 for an
 *     unknown scope, and the chain it names lists and verifies only the
 *     platform rows — never a workspace's. */
#include "../../core/audit.c"
#include "core/tenantapi.h"
#include "core/db.h"
#include "third_party/cJSON.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", sql); exit(1); }
}
static long scalar(db_handle *db, const char *sql) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}
static cJSON *verify(db_handle *db, const char *tenant) {
  char *js = tenantapi_audit_verify(db, tenant);
  assert(js);
  cJSON *o = cJSON_Parse(js);
  free(js);
  assert(o);
  return o;
}
static double num(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  assert(cJSON_IsNumber(v));
  return v->valuedouble;
}

typedef struct { const char *path; int n; int id; } wr_arg;
static void *writer(void *vp) {
  wr_arg *a = vp;
  db_handle own = {0};
  assert(db_attach(&own, a->path) == 0);
  for (int i = 0; i < a->n; i++) {
    char tgt[32]; snprintf(tgt, sizeof tgt, "w%d-%d", a->id, i);
    int rc = audit_write_ex(&own, "tC", "u", "race.write", tgt, NULL, NULL, NULL);
    assert(rc == 0 && "every concurrent write lands IN the chain");
  }
  db_close(&own);
  return NULL;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);

  /* Two legacy rows, as written before the writer chained. */
  exec_ok(&db, "INSERT INTO audit_events(id,tenant_id,action) VALUES"
               " ('legacy-1','tA','old.raw'),('legacy-2','tA','old.raw')");

  /* 20 rows for tA interleaved with 5 for tB. */
  for (int i = 0; i < 20; i++) {
    char tgt[16]; snprintf(tgt, sizeof tgt, "t%d", i);
    audit_write(&db, "tA", "uA", "case.update", tgt, "{\"k\":\"v\"}");
    if (i % 4 == 0) audit_write(&db, "tB", "uB", "case.update", tgt, NULL);
  }
  assert(scalar(&db, "SELECT COUNT(*) FROM audit_events WHERE tenant_id='tA'"
                     " AND row_hash IS NOT NULL") == 20);
  assert(scalar(&db, "SELECT MAX(chain_seq) FROM audit_events WHERE tenant_id='tA'") == 20);
  assert(scalar(&db, "SELECT MAX(chain_seq) FROM audit_events WHERE tenant_id='tB'") == 5);
  assert(scalar(&db, "SELECT COUNT(*) FROM audit_events WHERE chain_seq=1"
                     " AND prev_hash='GENESIS'") == 2 && "each tenant has its own genesis");

  cJSON *o = verify(&db, "tA");
  assert(cJSON_IsTrue(cJSON_GetObjectItem(o, "ok")));
  assert(num(o, "count") == 20 && "the walk covers the rows, not zero of them");
  assert(num(o, "unchained") == 2 && "legacy rows are reported, not skipped");
  cJSON_Delete(o);
  o = verify(&db, "tB");
  assert(cJSON_IsTrue(cJSON_GetObjectItem(o, "ok")) && num(o, "count") == 5);
  assert(num(o, "unchained") == 0);
  cJSON_Delete(o);

  /* Tamper: edit the 7th row's payload. The walk breaks AT it. */
  char tampered[40] = {0};
  { sqlite3_stmt *s;
    assert(sqlite3_prepare_v2(db.h, "SELECT id FROM audit_events WHERE tenant_id='tA'"
                              " AND chain_seq=7", -1, &s, NULL) == SQLITE_OK);
    assert(sqlite3_step(s) == SQLITE_ROW);
    snprintf(tampered, sizeof tampered, "%s", (const char *)sqlite3_column_text(s, 0));
    sqlite3_finalize(s); }
  exec_ok(&db, "UPDATE audit_events SET payload_json='{\"k\":\"edited\"}'"
               " WHERE tenant_id='tA' AND chain_seq=7");
  o = verify(&db, "tA");
  assert(cJSON_IsFalse(cJSON_GetObjectItem(o, "ok")) && "an edited row fails verify");
  assert(num(o, "brokenAt") == 6);
  cJSON *bid = cJSON_GetObjectItem(o, "brokenId");
  assert(cJSON_IsString(bid) && strcmp(bid->valuestring, tampered) == 0);
  assert(strstr(cJSON_GetObjectItem(o, "reason")->valuestring, "row_hash mismatch"));
  cJSON_Delete(o);

  /* Deleting a row is a gap. (tB, untouched by the edit above.) */
  exec_ok(&db, "DELETE FROM audit_events WHERE tenant_id='tB' AND chain_seq=3");
  o = verify(&db, "tB");
  assert(cJSON_IsFalse(cJSON_GetObjectItem(o, "ok")) && num(o, "brokenAt") == 2);
  cJSON_Delete(o);

  /* A second writer cannot claim a position the chain already holds. */
  { char *e = NULL;
    int rc = sqlite3_exec(db.h, "INSERT INTO audit_events(id,tenant_id,action,"
                          "row_hash,prev_hash,chain_seq) VALUES ('fork','tA','x',"
                          "'h','p',5)", NULL, NULL, &e);
    assert(rc != SQLITE_OK && "UNIQUE(tenant_id, chain_seq) refuses a fork");
    sqlite3_free(e); }

  /* Past the old 100,000-row cap: a 100,050-row chain verifies over all of it,
   * and an edit at row 100,040 is FOUND. The walk used to stop at 100,000 and
   * answer ok:true, so this edit was invisible. Rows go in directly with their
   * real hashes, in one transaction, so the test stays fast. */
  { const long N = 100050;
    sqlite3_stmt *ins;
    exec_ok(&db, "BEGIN");
    assert(sqlite3_prepare_v2(db.h, "INSERT INTO audit_events(id,tenant_id,"
             "user_id,action,target,payload_json,ts,ip,ua,prev_hash,row_hash,"
             "chain_seq) VALUES(?1,'tD','u','bulk',?2,NULL,'2026-10-21T00:00:00Z',"
             "NULL,NULL,?3,?4,?5)", -1, &ins, NULL) == SQLITE_OK);
    char prev[65] = "GENESIS", rh[65], id[32], tgt[32];
    for (long i = 1; i <= N; i++) {
      snprintf(id, sizeof id, "bulk-%ld", i);
      snprintf(tgt, sizeof tgt, "t%ld", i);
      audit_row_hash(id, "tD", "u", "bulk", tgt, NULL, "2026-10-21T00:00:00Z",
                     NULL, NULL, prev, i, rh);
      sqlite3_bind_text(ins, 1, id, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(ins, 2, tgt, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(ins, 3, prev, -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(ins, 4, rh, -1, SQLITE_TRANSIENT);
      sqlite3_bind_int64(ins, 5, i);
      assert(sqlite3_step(ins) == SQLITE_DONE);
      sqlite3_reset(ins);
      memcpy(prev, rh, sizeof prev);
    }
    sqlite3_finalize(ins);
    exec_ok(&db, "COMMIT");
    o = verify(&db, "tD");
    assert(cJSON_IsTrue(cJSON_GetObjectItem(o, "ok")) && num(o, "count") == N);
    cJSON_Delete(o);
    exec_ok(&db, "UPDATE audit_events SET target='edited' WHERE tenant_id='tD'"
                 " AND chain_seq=100040");
    o = verify(&db, "tD");
    assert(cJSON_IsFalse(cJSON_GetObjectItem(o, "ok")) && "found past 100,000");
    assert(num(o, "brokenAt") == 100039 && num(o, "count") == N);
    cJSON_Delete(o);
    printf("  100,050-row chain walked whole; an edit at row 100,040 found: ok\n"); }

  /* Four connections, one tenant, 50 writes each: one unbroken chain of 200. */
  pthread_t th[4]; wr_arg wa[4];
  for (int i = 0; i < 4; i++) {
    wa[i] = (wr_arg){ dbp, 50, i };
    assert(pthread_create(&th[i], NULL, writer, &wa[i]) == 0);
  }
  for (int i = 0; i < 4; i++) pthread_join(th[i], NULL);
  o = verify(&db, "tC");
  assert(cJSON_IsTrue(cJSON_GetObjectItem(o, "ok")) && num(o, "count") == 200);
  assert(num(o, "unchained") == 0);
  cJSON_Delete(o);

  /* Inside a caller's open transaction the writer nests (SAVEPOINT) and the
   * row commits with the caller. */
  exec_ok(&db, "BEGIN");
  assert(audit_write_ex(&db, "tC", "u", "nested", NULL, NULL, NULL, NULL) == 0);
  exec_ok(&db, "COMMIT");
  o = verify(&db, "tC");
  assert(cJSON_IsTrue(cJSON_GetObjectItem(o, "ok")) && num(o, "count") == 201);
  cJSON_Delete(o);

  /* ?scope=platform — the route's decision, then what it reads. */
  { const char *ch = (const char *)1;
    assert(tenantapi_audit_chain(NULL, 0, "tA", &ch) == 0 && !strcmp(ch, "tA"));
    assert(tenantapi_audit_chain("", 1, "tA", &ch) == 0 && !strcmp(ch, "tA"));
    assert(tenantapi_audit_chain("workspace", 0, "tA", &ch) == 0 && !strcmp(ch, "tA"));
    assert(tenantapi_audit_chain("platform", 0, "tA", &ch) == 403 && ch == NULL
           && "a workspace owner/admin is not a platform operator");
    assert(tenantapi_audit_chain("platform", 1, "tA", &ch) == 0
           && !strcmp(ch, "platform"));
    assert(tenantapi_audit_chain("Platform", 1, "tA", &ch) == 400 && ch == NULL);
    assert(tenantapi_audit_chain("tenant", 1, "tA", &ch) == 400
           && "an unknown scope is refused, never read as the workspace"); }
  for (int i = 0; i < 3; i++)
    audit_write(&db, TENANTAPI_PLATFORM_CHAIN, "op", "threshold.set", "story_tau", NULL);
  { char *js = tenantapi_audit_list(&db, TENANTAPI_PLATFORM_CHAIN, 100, 0);
    assert(js);
    cJSON *l = cJSON_Parse(js); free(js);
    cJSON *data = cJSON_GetObjectItem(l, "data");
    assert(cJSON_GetArraySize(data) == 3 && "the platform rows, and only them");
    cJSON *r;
    cJSON_ArrayForEach(r, data)
      assert(!strcmp(cJSON_GetObjectItem(r, "action")->valuestring, "threshold.set"));
    cJSON_Delete(l);
    o = verify(&db, TENANTAPI_PLATFORM_CHAIN);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(o, "ok")) && num(o, "count") == 3);
    cJSON_Delete(o);
    js = tenantapi_audit_list(&db, "tA", 500, 0);
    l = cJSON_Parse(js); free(js);
    cJSON_ArrayForEach(r, cJSON_GetObjectItem(l, "data"))
      assert(strcmp(cJSON_GetObjectItem(r, "action")->valuestring, "threshold.set") != 0
             && "a workspace list never carries a platform row");
    cJSON_Delete(l); }

  db_close(&db);
  printf("test_audit_chain: ok\n");
  return 0;
}
