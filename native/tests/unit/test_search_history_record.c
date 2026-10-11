/* test_search_history_record.c — every committed search lands in the
 * workspace's history, once.
 *
 * Decided 2026-10-09: history records every ad-hoc search, not only saved-
 * search runs. The server cannot tell a keystroke of a search-as-you-type box
 * from a search, so the clients POST /api/search-history when a search is
 * committed. Holds:
 *   - POST records the caller's own row and returns it (201), with author;
 *   - the same search by the same person inside SS_HISTORY_MERGE_SEC refreshes
 *     that row (200, meta.merged) instead of adding a copy — a reload or a
 *     feed refetch is not a new search; a different parameter, a different
 *     kind, a different person, or the same search after the window is a row;
 *   - a bad kind, a non-object params, a negative count, an oversized params
 *     are refused (400), never stored or truncated;
 *   - nothing crosses workspaces;
 *   - search_history_record (run_saved, searchapi's run start) merges too. */
#include "core/savedsearchapi.h"
#include "core/tenantapi.h"
#include "core/db.h"
#include "../../third_party/cJSON.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  int rc = sqlite3_exec(db->h, sql, NULL, NULL, &e);
  if (rc != SQLITE_OK) fprintf(stderr, "%s: %s\n", sql, e ? e : "?");
  assert(rc == SQLITE_OK);
  sqlite3_free(e);
}
static long long count(db_handle *db, const char *sql) {
  sqlite3_stmt *s; long long n = -1;
  assert(sqlite3_prepare_v2(db->h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}
static tenant_ctx as(const char *tid, const char *uid, const char *role) {
  tenant_ctx t; memset(&t, 0, sizeof t);
  snprintf(t.tenant_id, sizeof t.tenant_id, "%s", tid);
  snprintf(t.user_id, sizeof t.user_id, "%s", uid);
  snprintf(t.role, sizeof t.role, "%s", role);
  return t;
}
static int post(db_handle *db, const tenant_ctx *t, const char *body, char **out) {
  int st = 0;
  char *r = searchhistoryapi(db, t, "POST", "", body, &st);
  if (out) *out = r; else free(r);
  return st;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "INSERT INTO tenants(id,slug,name) VALUES ('tA','ta','A'),('tB','tb','B')");
  tenant_ctx a1 = as("tA", "uA1", "viewer");          /* a viewer may record */
  tenant_ctx a2 = as("tA", "uA2", "analyst");
  tenant_ctx b1 = as("tB", "uB1", "owner");

  char *r = NULL;
  int st = post(&db, &a1, "{\"kind\":\"intel\",\"params\":{\"q\":\"kobe port\"},\"result_count\":42}", &r);
  printf("  first POST -> %d\n", st);
  assert(st == 201 && r && strstr(r, "\"kind\":\"intel\"") && strstr(r, "\"mine\":true") &&
         strstr(r, "\"user_id\":\"uA1\"") && strstr(r, "\"result_count\":42") &&
         strstr(r, "\"merged\":false"));
  free(r);

  /* the same search again (a reload, a refetch): refreshed, not copied */
  st = post(&db, &a1, "{\"kind\":\"intel\",\"params\":{\"q\":\"kobe port\"},\"result_count\":43}", &r);
  assert(st == 200 && r && strstr(r, "\"merged\":true") && strstr(r, "\"result_count\":43"));
  free(r);
  assert(count(&db, "SELECT COUNT(*) FROM search_history") == 1);
  printf("  the same search twice: one row, count refreshed: ok\n");

  /* each of these is a different search */
  assert(post(&db, &a1, "{\"kind\":\"intel\",\"params\":{\"q\":\"kobe port\",\"sort\":\"new\"}}", NULL) == 201);
  assert(post(&db, &a1, "{\"kind\":\"entity\",\"params\":{\"q\":\"kobe port\"}}", NULL) == 201);
  assert(post(&db, &a2, "{\"kind\":\"intel\",\"params\":{\"q\":\"kobe port\"}}", NULL) == 201);
  assert(post(&db, &b1, "{\"kind\":\"intel\",\"params\":{\"q\":\"kobe port\"}}", NULL) == 201);
  assert(count(&db, "SELECT COUNT(*) FROM search_history") == 5);
  printf("  another parameter, kind, member or workspace is a new row: ok\n");

  /* outside the window it is a new row again */
  exec_ok(&db, "UPDATE search_history SET ts=datetime('now','-11 minutes') WHERE user_id='uA1' AND kind='intel' AND params_json='{\"q\":\"kobe port\"}'");
  assert(post(&db, &a1, "{\"kind\":\"intel\",\"params\":{\"q\":\"kobe port\"}}", NULL) == 201);
  assert(count(&db, "SELECT COUNT(*) FROM search_history WHERE user_id='uA1' AND params_json='{\"q\":\"kobe port\"}' AND kind='intel'") == 2);
  printf("  the same search after the merge window: a new row: ok\n");

  /* refusals: nothing stored */
  long long before = count(&db, "SELECT COUNT(*) FROM search_history");
  assert(post(&db, &a1, "{\"kind\":\"weather\",\"params\":{\"q\":\"x\"}}", NULL) == 400);
  assert(post(&db, &a1, "{\"kind\":\"intel\",\"params\":\"q=x\"}", NULL) == 400);
  assert(post(&db, &a1, "{\"kind\":\"intel\",\"params\":{\"q\":\"x\"},\"result_count\":-1}", NULL) == 400);
  assert(post(&db, &a1, "not json", NULL) == 400);
  { size_t n = SS_PARAMS_MAX + 64; char *big = malloc(n + 64);
    assert(big);
    int o = snprintf(big, n + 64, "{\"kind\":\"intel\",\"params\":{\"q\":\"");
    memset(big + o, 'x', n); strcpy(big + o + n, "\"}}");
    assert(post(&db, &a1, big, NULL) == 400);
    free(big); }
  assert(count(&db, "SELECT COUNT(*) FROM search_history") == before);
  printf("  bad kind / params / count / body / oversized params: 400, nothing stored: ok\n");

  /* the read side sees the new rows workspace-wide, and tB sees only its own */
  r = searchhistoryapi(&db, &a2, "GET", "", NULL, &st);
  assert(st == 200 && r && strstr(r, "\"user_id\":\"uA1\"") && !strstr(r, "uB1"));
  free(r);
  r = searchhistoryapi(&db, &b1, "GET", "", NULL, &st);
  assert(st == 200 && r && strstr(r, "\"user_id\":\"uB1\"") && !strstr(r, "uA1"));
  free(r);
  printf("  workspace-wide read, nothing across workspaces: ok\n");

  /* the server-side recorder (run_saved, an OSINT run's start) merges too */
  search_history_record(&db, "tA", "uA2", "osint", "{\"q\":\"who owns kobe port\"}", -1);
  search_history_record(&db, "tA", "uA2", "osint", "{\"q\":\"who owns kobe port\"}", -1);
  assert(count(&db, "SELECT COUNT(*) FROM search_history WHERE kind='osint'") == 1);
  printf("  search_history_record merges the same way: ok\n");

  db_close(&db);
  printf("test_search_history_record: ok\n");
  return 0;
}
