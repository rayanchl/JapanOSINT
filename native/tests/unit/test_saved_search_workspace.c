/* test_saved_search_workspace.c — saved searches and search history are
 * visible to every member of the workspace, and to nobody outside it.
 *
 * Decided 2026-10-05: everything in a workspace is meant to be seen by its
 * members. Reads carry tenant_id only and name the author; changing an entry
 * (PATCH, DELETE, clearing history) stays with its author.
 *
 * Holds:
 *   - a teammate lists and opens another member's saved search (mine=false);
 *   - another workspace sees nothing;
 *   - a teammate can run it, but not rename or delete it;
 *   - search history is workspace-wide, ?mine=1 narrows it to the caller,
 *     and clearing history removes only the caller's own entries. */
#include "core/savedsearchapi.h"
#include "core/tenantapi.h"
#include "core/db.h"
#include "third_party/sqlite3.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static tenant_ctx who(const char *tid, const char *uid, const char *role) {
  tenant_ctx t; memset(&t, 0, sizeof t);
  snprintf(t.tenant_id, sizeof t.tenant_id, "%s", tid);
  snprintf(t.user_id, sizeof t.user_id, "%s", uid);
  snprintf(t.role, sizeof t.role, "%s", role);
  return t;
}
static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", sql); exit(1); }
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "INSERT INTO tenants(id,slug,name) VALUES ('tA','ta','A'),('tB','tb','B')");
  exec_ok(&db, "INSERT INTO users(id,email) VALUES ('uA1','a1@x.test'),('uA2','a2@x.test'),('uB1','b1@x.test')");

  tenant_ctx a1 = who("tA", "uA1", "analyst"), a2 = who("tA", "uA2", "analyst"),
             b1 = who("tB", "uB1", "owner");
  int st = 0;
  char *r = savedsearchapi(&db, &a1, "POST", "", "", "",
                           "{\"name\":\"kobe port\",\"kind\":\"intel\",\"params\":{\"q\":\"kobe\"}}", &st);
  assert(r && (st == 201 || st == 200));
  char id[64] = {0};
  const char *p = strstr(r, "\"id\":\"");
  assert(p); sscanf(p + 6, "%63[^\"]", id);
  free(r);

  r = savedsearchapi(&db, &a2, "GET", "", "", "", NULL, &st);
  assert(r && st == 200 && strstr(r, "kobe port") && strstr(r, "\"mine\":false") &&
         "a teammate lists another member's saved search");
  free(r);
  r = savedsearchapi(&db, &a2, "GET", "", "", "mine=1", NULL, &st);
  assert(r && !strstr(r, "kobe port") && "?mine=1 narrows to the caller");
  free(r);
  r = savedsearchapi(&db, &a2, "GET", id, "", "", NULL, &st);
  assert(st == 200 && "and opens it by id"); free(r);
  r = savedsearchapi(&db, &b1, "GET", "", "", "", NULL, &st);
  assert(r && !strstr(r, "kobe port") && "another workspace sees nothing"); free(r);
  r = savedsearchapi(&db, &b1, "GET", id, "", "", NULL, &st);
  assert(st == 404); free(r);
  printf("  saved searches: whole workspace sees them, other workspaces do not: ok\n");

  r = savedsearchapi(&db, &a2, "PATCH", id, "", "", "{\"name\":\"renamed\"}", &st);
  assert(st == 404 && "a teammate cannot rename it"); free(r);
  r = savedsearchapi(&db, &a2, "DELETE", id, "", "", NULL, &st);
  assert(st == 404 || st == 204); free(r);
  r = savedsearchapi(&db, &a1, "GET", id, "", "", NULL, &st);
  assert(st == 200 && "a teammate's delete removed nothing"); free(r);
  r = savedsearchapi(&db, &a2, "POST", id, "run", "", "{}", &st);
  assert((st == 200 || st == 201) && "a teammate can run it"); free(r);
  printf("  only the author renames or deletes; any member runs: ok\n");

  search_history_record(&db, "tA", "uA1", "intel", "{\"q\":\"tokyo\"}", 3);
  search_history_record(&db, "tA", "uA2", "intel", "{\"q\":\"osaka\"}", 4);
  r = searchhistoryapi(&db, &a2, "GET", "", NULL, &st);
  assert(r && strstr(r, "tokyo") && strstr(r, "osaka") && strstr(r, "\"scope\":\"workspace\""));
  free(r);
  r = searchhistoryapi(&db, &a2, "GET", "mine=1", NULL, &st);
  assert(r && !strstr(r, "tokyo") && strstr(r, "osaka")); free(r);
  r = searchhistoryapi(&db, &b1, "GET", "", NULL, &st);
  assert(r && !strstr(r, "tokyo") && !strstr(r, "osaka")); free(r);
  r = searchhistoryapi(&db, &a2, "DELETE", "", NULL, &st); free(r);
  r = searchhistoryapi(&db, &a1, "GET", "", NULL, &st);
  assert(r && strstr(r, "tokyo") && !strstr(r, "osaka") &&
         "clearing removes only the caller's own entries"); free(r);
  printf("  history: workspace-wide, ?mine=1, other workspaces excluded, clear is own-only: ok\n");

  db_close(&db);
  printf("\nall passed\n");
  return 0;
}
