/* test_breach_rescan_gate.c — at most one breach-corpus rescan at a time.
 *
 * rescan_async() spawned a detached thread per call with no limit, and POST
 * /api/breach-monitors/:id/rescan ran another walk inline on top, so N
 * requests were N concurrent full-corpus walks. Pinned here:
 *   - while the slot is held, POST .../rescan is a 409 rescan_in_progress
 *     (and writes no audit row — nothing ran);
 *   - creating a domain monitor while it is held reports meta.scan "busy",
 *     never "running" for a walk that did not start;
 *   - once released, the same POST runs and answers 200;
 *   - a background rescan releases the slot when it finishes. */
#include "../../core/breach_monitor.c"
#include "core/db.h"
#include "third_party/cJSON.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>

static long scalar(db_handle *db, const char *sql) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}

static cJSON *call(db_handle *db, const tenant_ctx *t, const char *meth,
                   const char *seg, const char *act, const char *body, int *st) {
  char *js = breach_monitors_api(db, t, meth, seg, act, body, NULL, 0, st);
  if (!js) return NULL;
  cJSON *o = cJSON_Parse(js);
  free(js);
  return o;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  breach_monitor_migrate(&db);

  tenant_ctx t = {0};
  snprintf(t.user_id, sizeof t.user_id, "u1");
  snprintf(t.tenant_id, sizeof t.tenant_id, "t1");
  snprintf(t.role, sizeof t.role, "owner");

  int st = 0;
  cJSON *o = call(&db, &t, "POST", "", "", "{\"kind\":\"email\",\"value\":\"a@b.example\"}", &st);
  assert(o && st == 201);
  char mid[64];
  snprintf(mid, sizeof mid, "%s",
           cJSON_GetObjectItem(cJSON_GetObjectItem(o, "data"), "id")->valuestring);
  cJSON_Delete(o);

  /* Hold the slot, as a running background rescan would. */
  assert(breach_monitor_rescan_try_begin() == 1);
  assert(breach_monitor_rescan_try_begin() == 0 && "the slot is exclusive");
  long audits0 = scalar(&db, "SELECT COUNT(*) FROM audit_events WHERE action='breach_monitor.rescan'");
  o = call(&db, &t, "POST", mid, "rescan", NULL, &st);
  assert(o && st == 409);
  assert(!strcmp(cJSON_GetObjectItem(o, "error")->valuestring, "rescan_in_progress"));
  assert(!strcmp(cJSON_GetObjectItem(o, "rescan")->valuestring, "running"));
  cJSON_Delete(o);
  assert(scalar(&db, "SELECT COUNT(*) FROM audit_events WHERE action='breach_monitor.rescan'")
         == audits0 && "a refused rescan is not audited as one that ran");

  /* A domain monitor's create wants a background walk: busy, not running. */
  o = call(&db, &t, "POST", "", "", "{\"kind\":\"domain\",\"value\":\"b.example\"}", &st);
  assert(o && st == 201);
  cJSON *meta = cJSON_GetObjectItem(o, "meta");
  assert(meta && !strcmp(cJSON_GetObjectItem(meta, "scan")->valuestring, "busy"));
  assert(cJSON_IsNull(cJSON_GetObjectItem(meta, "initial_hits")));
  cJSON_Delete(o);

  breach_monitor_rescan_end();
  assert(!breach_monitor_rescan_busy());
  o = call(&db, &t, "POST", mid, "rescan", NULL, &st);
  assert(o && st == 200 && "the slot is free again");
  cJSON_Delete(o);
  assert(!breach_monitor_rescan_busy() && "an inline rescan releases the slot");

  /* A background rescan (domain monitor present -> async) takes and then
   * releases the slot on its own. */
  o = call(&db, &t, "POST", "", "", "{\"kind\":\"domain\",\"value\":\"c.example\"}", &st);
  assert(o && st == 201);
  meta = cJSON_GetObjectItem(o, "meta");
  assert(!strcmp(cJSON_GetObjectItem(meta, "scan")->valuestring, "running"));
  cJSON_Delete(o);
  for (int i = 0; i < 200 && breach_monitor_rescan_busy(); i++) {
    struct timespec d = { 0, 25 * 1000000L };
    nanosleep(&d, NULL);
  }
  assert(!breach_monitor_rescan_busy() && "the background rescan released its slot");

  db_close(&db);
  printf("test_breach_rescan_gate: ok\n");
  return 0;
}
