/* test_search_runs_workspace.c — every member of a workspace can see every
 * member's OSINT search runs (the query, the author, the LLM synthesis), and
 * nobody outside the workspace can list or read them.
 *
 * Decided 2026-10-05: everything in a workspace — records, searches, queries,
 * syntheses — is visible to its members. A run was always readable by id
 * within its workspace, but nothing listed runs: the web client knew only the
 * runs its own tab had started, so a teammate's investigation was invisible
 * unless someone handed over its id.
 *
 * Holds (searchapi_runs, the layer GET /api/search/runs calls):
 *   - a teammate lists another member's run, with user_id and mine=false;
 *   - ?mine=1 narrows to the caller, and meta.scope says which view it is;
 *   - a completed run shows its synthesis preview and full length, a live run
 *     its phase, a run that left no record says "unknown";
 *   - an old run whose owner row has no query takes it from its stored row;
 *   - paging is lossless and page.total is measured;
 *   - another workspace lists none of it, and cannot open a run by id. */
#include "../../core/searchapi.c"
#include "../../core/intel.h"
#include "../../core/db.h"

#include <assert.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", sql); exit(1); }
}

static void run_row(db_handle *db, const char *tenant, const char *rid,
                    const char *query, const char *synth, const char *props) {
  char uid[128], title[256];
  snprintf(uid, sizeof uid, "osint-search|run:%s", rid);
  snprintf(title, sizeof title, "OSINT search: %s", query);
  intel_sink k = intel_sink_make(db, "osint-search", tenant);
  intel_item it = {0};
  it.uid = uid; it.title = title; it.summary = synth; it.body = synth;
  it.record_type = "osint_search_run"; it.published_at = "2026-10-05T00:00:00Z";
  it.properties_json = props;
  assert(k.emit(&k, &it) >= 0);
  intel_sink_free(&k);
}

static cJSON *find(cJSON *arr, const char *rid) {
  cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    cJSON *id = cJSON_GetObjectItem(r, "request_id");
    if (cJSON_IsString(id) && !strcmp(id->valuestring, rid)) return r;
  }
  return NULL;
}
static const char *str(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return cJSON_IsString(v) ? v->valuestring : NULL;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);

  assert(owner_persist(&db, "r1", "tA", "uA1", "who owns kobe port") == 0);
  assert(owner_persist(&db, "r2", "tA", "uA2", "osaka shipping") == 0);
  assert(owner_persist(&db, "r3", "tA", "uA1", "lost to a restart") == 0);
  assert(owner_persist(&db, "rb", "tB", "uB1", "tenant b secret") == 0);
  /* r0: recorded before the owner row carried the query. */
  exec_ok(&db, "INSERT INTO search_run_owners(request_id,tenant_id,user_id) "
               "VALUES ('r0','tA','uA2')");
  exec_ok(&db, "UPDATE search_run_owners SET created_at=CASE request_id "
               "WHEN 'r0' THEN '2026-10-01 00:00:00' WHEN 'r1' THEN '2026-10-02 00:00:00' "
               "WHEN 'r3' THEN '2026-10-03 00:00:00' WHEN 'r2' THEN '2026-10-04 00:00:00' "
               "ELSE '2026-10-04 00:00:00' END");

  char longs[1200];
  memset(longs, 0, sizeof longs);
  strcpy(longs, "Kobe Port Holdings is owned by ");
  while (strlen(longs) < 1000) strcat(longs, "日本語の合成 ");
  run_row(&db, "tA", "r1", "who owns kobe port", longs,
          "{\"query\":\"who owns kobe port\",\"phase\":\"completed\",\"degraded\":false}");
  run_row(&db, "tA", "r0", "older run", "short synthesis",
          "{\"query\":\"older run from the stored row\",\"phase\":\"completed\"}");
  run_row(&db, "tB", "rb", "tenant b secret", "b's synthesis",
          "{\"query\":\"tenant b secret\",\"phase\":\"completed\"}");
  assert(progress_create_owned("r2", "osaka shipping", 5, "tA", "k2"));

  int st = 0;
  char *b = searchapi_runs(&db, "tA", "uA2", 50, 0, NULL, &st);
  assert(b && st == 200);
  cJSON *j = cJSON_Parse(b); assert(j);
  cJSON *data = cJSON_GetObjectItem(j, "data");
  assert(cJSON_GetArraySize(data) == 4 && "every run in the workspace, no other");
  assert(!find(data, "rb") && !strstr(b, "tenant b secret"));
  cJSON *r1 = find(data, "r1");
  assert(r1 && !strcmp(str(r1, "user_id"), "uA1") &&
         cJSON_IsFalse(cJSON_GetObjectItem(r1, "mine")) &&
         "a teammate sees another member's run, and who ran it");
  assert(!strcmp(str(r1, "query"), "who owns kobe port") && !strcmp(str(r1, "status"), "completed"));
  assert(str(r1, "synthesis_preview") && !strncmp(str(r1, "synthesis_preview"), "Kobe Port Holdings", 18));
  assert(cJSON_IsTrue(cJSON_GetObjectItem(r1, "synthesis_truncated")) &&
         (size_t) cJSON_GetObjectItem(r1, "synthesis_bytes")->valuedouble == strlen(longs) &&
         "the preview says how much of the synthesis it shows");
  assert(strlen(str(r1, "synthesis_preview")) <= RUNS_PREVIEW_BYTES);
  cJSON *r2 = find(data, "r2");
  assert(r2 && cJSON_IsTrue(cJSON_GetObjectItem(r2, "mine")) &&
         !strcmp(str(r2, "status"), "running") && "a live run reports its phase");
  cJSON *r3 = find(data, "r3");
  assert(r3 && !strcmp(str(r3, "status"), "unknown") &&
         !strcmp(str(r3, "query"), "lost to a restart") &&
         "a run with no record left says so, and still names its query");
  cJSON *r0 = find(data, "r0");
  assert(r0 && !strcmp(str(r0, "query"), "older run from the stored row"));
  assert(!strcmp(str(cJSON_GetObjectItem(j, "meta"), "scope"), "workspace"));
  assert(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "page"), "total")->valueint == 4);
  assert(!strcmp(str(cJSON_GetArrayItem(data, 0), "request_id"), "r2") &&
         "newest first");
  cJSON_Delete(j); free(b);
  printf("  workspace list: every member's runs, author named, status honest: ok\n");

  b = searchapi_runs(&db, "tA", "uA2", 50, 1, NULL, &st);
  assert(b && st == 200 && strstr(b, "\"r2\"") && strstr(b, "\"r0\"") &&
         !strstr(b, "\"r1\"") && !strstr(b, "\"r3\"") && strstr(b, "\"scope\":\"user\""));
  free(b);
  printf("  ?mine=1 narrows to the caller: ok\n");

  /* paging: two pages of two, no overlap, nothing lost */
  b = searchapi_runs(&db, "tA", "uA1", 2, 0, NULL, &st);
  j = cJSON_Parse(b); assert(j);
  cJSON *pg = cJSON_GetObjectItem(j, "page");
  const char *nc = str(pg, "next_cursor");
  assert(nc && cJSON_GetArraySize(cJSON_GetObjectItem(j, "data")) == 2);
  char cur[160]; snprintf(cur, sizeof cur, "%s", nc);
  char *b2 = searchapi_runs(&db, "tA", "uA1", 2, 0, cur, &st);
  cJSON *j2 = cJSON_Parse(b2); assert(j2);
  cJSON *d1 = cJSON_GetObjectItem(j, "data"), *d2 = cJSON_GetObjectItem(j2, "data");
  assert(cJSON_GetArraySize(d2) == 2);
  const char *want[] = { "r0", "r1", "r2", "r3" };
  for (int i = 0; i < 4; i++)
    assert((find(d1, want[i]) != NULL) + (find(d2, want[i]) != NULL) == 1);
  cJSON_Delete(j); cJSON_Delete(j2); free(b); free(b2);
  printf("  paging is lossless: ok\n");

  b = searchapi_runs(&db, "tB", "uB1", 50, 0, NULL, &st);
  assert(b && st == 200 && strstr(b, "\"rb\"") && !strstr(b, "\"r1\"") &&
         !strstr(b, "kobe") && !strstr(b, "\"r2\""));
  free(b);
  assert(searchapi_results(&db, "tB", "r1") == NULL &&
         "another workspace cannot open a run by id");
  assert(searchapi_results(&db, "tB", "r2") == NULL &&
         "nor a live one");
  char *ok = searchapi_results(&db, "tA", "r1");
  assert(ok && strstr(ok, "Kobe Port Holdings") && "a teammate opens the run and its synthesis");
  free(ok);
  printf("  other workspaces list nothing and open nothing: ok\n");

  db_close(&db);
  printf("\nall passed\n");
  return 0;
}
