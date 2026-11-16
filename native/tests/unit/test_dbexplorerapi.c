/* tests/unit/test_dbexplorerapi.c — core/dbexplorerapi.c, the read-only table
 * browser behind the admin DB page.
 *
 * Security-relevant contract: ONLY the six allow-listed tables are readable
 * (anything else — sqlite_master, a table that exists but is not listed, an
 * injection attempt in the name — returns NULL, which the route maps to 400);
 * `orderBy` is accepted only if it is an actual column of that table; `q` is
 * always a bound parameter; limit/offset are clamped. Functionally: the
 * q-search covers every text column of the wide intel_items table (a past bug
 * truncated that WHERE and answered "no matches" for everything), and the
 * scheduler listing returns the maintenance pods the registry really holds,
 * not invented cron strings. */
#include "../../core/dbexplorerapi.c"
#include "../../core/intel.h"
#include <assert.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}

static void put(db_handle *db, const char *key, const char *title, const char *body) {
  intel_sink k = intel_sink_make(db, "dbx-test", "legacy");
  intel_item it = {0};
  it.remote_key = key; it.title = title; it.body = body; it.record_type = "t";
  it.published_at = "2026-11-01T00:00:00Z";
  assert(k.emit(&k, &it) >= 0);
  intel_sink_free(&k);
}

static cJSON *tbl(db_handle *db, const char *name, int limit, int off, const char *q, const char *ob, const char *od) {
  char *j = dbexplorer_table(db, name, limit, off, q, ob, od);
  if (!j) return NULL;
  cJSON *o = cJSON_Parse(j); free(j); return o;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);

  for (int i = 0; i < 12; i++) {
    char k[16], t[64]; snprintf(k, sizeof k, "k%02d", i); snprintf(t, sizeof t, "Title number %02d", i);
    put(&db, k, t, i == 7 ? "needle-in-body" : "ordinary body");
  }

  printf("-- allow-list\n");
  ok(dbexplorer_table(&db, "sqlite_master", 10, 0, NULL, NULL, NULL) == NULL, "sqlite_master refused");
  ok(dbexplorer_table(&db, "users", 10, 0, NULL, NULL, NULL) == NULL, "a table that is not listed is refused");
  ok(dbexplorer_table(&db, "audit_events", 10, 0, NULL, NULL, NULL) == NULL, "the audit chain table is not browsable");
  ok(dbexplorer_table(&db, "intel_items\"; DROP TABLE sources; --", 10, 0, NULL, NULL, NULL) == NULL, "injection in the table name is refused");
  ok(dbexplorer_table(&db, "INTEL_ITEMS", 10, 0, NULL, NULL, NULL) == NULL, "names are matched exactly (case-sensitive)");
  ok(dbexplorer_table(&db, "", 10, 0, NULL, NULL, NULL) == NULL, "empty name refused");
  {
    char *j = dbexplorer_tables(&db); cJSON *a = cJSON_Parse(j); free(j);
    int n = cJSON_GetArraySize(a), clean = 1;
    const char *allow[] = { "intel_items", "sources", "fetch_log", "cameras", "transport_stations", "transport_lines" };
    cJSON *e;
    cJSON_ArrayForEach(e, a) {
      int f = 0; for (int i = 0; i < 6; i++) if (!strcmp(cJSON_GetObjectItem(e, "name")->valuestring, allow[i])) f = 1;
      if (!f) clean = 0;
    }
    ok(n >= 2 && n <= 6 && clean, "table listing contains only allow-listed tables");
    cJSON *first = cJSON_GetArrayItem(a, 0);
    ok(cJSON_GetObjectItem(first, "row_count") && cJSON_GetArraySize(cJSON_GetObjectItem(first, "columns")) > 3, "each entry carries a row count and its columns");
    cJSON *ii = NULL; cJSON_ArrayForEach(e, a) if (!strcmp(cJSON_GetObjectItem(e, "name")->valuestring, "intel_items")) ii = e;
    ok(ii && cJSON_GetObjectItem(ii, "row_count")->valuedouble >= 12, "intel_items row_count reflects the inserted rows");
    cJSON_Delete(a);
  }

  printf("-- paging bounds\n");
  cJSON *o = tbl(&db, "intel_items", 5, 0, NULL, NULL, NULL);
  ok(o && cJSON_GetArraySize(cJSON_GetObjectItem(o, "rows")) == 5 && cJSON_GetObjectItem(o, "total")->valuedouble >= 12, "limit 5 -> 5 rows, total counts all");
  ok(!cJSON_GetObjectItem(o, "error"), "no error field on a healthy query");
  cJSON_Delete(o);
  o = tbl(&db, "intel_items", 0, 0, NULL, NULL, NULL);
  ok(o && cJSON_GetObjectItem(o, "limit")->valueint == 50, "limit 0 -> default 50");
  cJSON_Delete(o);
  o = tbl(&db, "intel_items", -9, -4, NULL, NULL, NULL);
  ok(o && cJSON_GetObjectItem(o, "limit")->valueint == 50 && cJSON_GetObjectItem(o, "offset")->valueint == 0, "negative limit/offset normalised");
  cJSON_Delete(o);
  o = tbl(&db, "intel_items", 100000, 0, NULL, NULL, NULL);
  ok(o && cJSON_GetObjectItem(o, "limit")->valueint == 200, "limit is clamped to 200");
  cJSON_Delete(o);
  o = tbl(&db, "intel_items", 10, 1000000, NULL, NULL, NULL);
  ok(o && cJSON_GetArraySize(cJSON_GetObjectItem(o, "rows")) == 0 && !cJSON_GetObjectItem(o, "error"), "offset past the end: empty rows, no error");
  cJSON_Delete(o);

  printf("-- ordering\n");
  o = tbl(&db, "intel_items", 3, 0, NULL, "title", "desc");
  ok(o && cJSON_IsString(cJSON_GetObjectItem(o, "orderBy")) && !strcmp(cJSON_GetObjectItem(o, "orderDir")->valuestring, "DESC"), "valid column + desc accepted");
  cJSON *r0 = o ? cJSON_GetArrayItem(cJSON_GetObjectItem(o, "rows"), 0) : NULL;
  ok(r0 && !strcmp(cJSON_GetObjectItem(r0, "title")->valuestring, "Title number 11"), "descending title order puts 11 first");
  cJSON_Delete(o);
  o = tbl(&db, "intel_items", 3, 0, NULL, "title; DROP TABLE sources", "sideways");
  ok(o && cJSON_IsNull(cJSON_GetObjectItem(o, "orderBy")) && !strcmp(cJSON_GetObjectItem(o, "orderDir")->valuestring, "ASC"), "an orderBy that is not a column is dropped; unknown direction -> ASC");
  ok(o && !cJSON_GetObjectItem(o, "error"), "and the query still runs");
  cJSON_Delete(o);
  o = tbl(&db, "sources", 1, 0, NULL, NULL, NULL);
  ok(o && cJSON_GetObjectItem(o, "total")->valuedouble > 1000, "the sources table is intact after the injection attempts");
  cJSON_Delete(o);

  printf("-- q search\n");
  o = tbl(&db, "intel_items", 50, 0, "needle-in-body", NULL, NULL);
  ok(o && cJSON_GetObjectItem(o, "total")->valuedouble == 1 && cJSON_GetArraySize(cJSON_GetObjectItem(o, "rows")) == 1,
     "q matches a value in a late text column of the wide table (regression: truncated WHERE answered no matches)");
  cJSON_Delete(o);
  o = tbl(&db, "intel_items", 50, 0, "Title number 0", NULL, NULL);
  ok(o && cJSON_GetObjectItem(o, "total")->valuedouble == 10, "substring match counts rows 00..09");
  cJSON_Delete(o);
  o = tbl(&db, "intel_items", 50, 0, "no-such-string-anywhere", NULL, NULL);
  ok(o && cJSON_GetObjectItem(o, "total")->valuedouble == 0 && !cJSON_GetObjectItem(o, "error"), "no match: total 0 with no error");
  cJSON_Delete(o);
  o = tbl(&db, "intel_items", 50, 0, "x' OR '1'='1", NULL, NULL);
  ok(o && cJSON_GetObjectItem(o, "total")->valuedouble == 0, "a quote in q is a bound value, not SQL");
  cJSON_Delete(o);
  { char big[2000]; memset(big, 'a', sizeof big - 1); big[sizeof big - 1] = 0;
    o = tbl(&db, "intel_items", 5, 0, big, NULL, NULL);
    ok(o != NULL && cJSON_GetObjectItem(o, "total")->valuedouble == 0, "a 2000-byte q does not overflow the 256-byte pattern buffer"); cJSON_Delete(o); }
  o = tbl(&db, "intel_items", 50, 0, "%", NULL, NULL);
  ok(o && cJSON_GetObjectItem(o, "total")->valuedouble >= 12, "[characterisation] '%' in q is a LIKE wildcard (not escaped) and matches every row");
  cJSON_Delete(o);
  o = tbl(&db, "intel_items", 50, 0, "", NULL, NULL);
  ok(o && !strcmp(cJSON_GetObjectItem(o, "q")->valuestring, ""), "empty q means no filter");
  cJSON_Delete(o);
  o = tbl(&db, "fetch_log", 5, 0, "zzz", NULL, NULL);
  ok(o != NULL, "a table with few text columns also searches cleanly");
  cJSON_Delete(o);

  printf("-- scheduler listing\n");
  {
    char *j = dbexplorer_scheduler(&db); cJSON *s = cJSON_Parse(j); free(j);
    cJSON *jobs = cJSON_GetObjectItem(s, "jobs"), *src = cJSON_GetObjectItem(s, "sources");
    ok(cJSON_IsArray(jobs) && cJSON_GetArraySize(jobs) >= 3, "jobs lists the registry's maintenance pods (>= 3)");
    int bad = 0, haspod = 0; cJSON *e;
    cJSON_ArrayForEach(e, jobs) {
      const source_def *d = registry_get(cJSON_GetObjectItem(e, "id")->valuestring);
      if (!d || !d->collector || d->collector[0] != '_' || d->update_interval_sec <= 0) bad++;
      if (!strncmp(cJSON_GetObjectItem(e, "cron")->valuestring, "every ", 6)) haspod++;
      if (!cJSON_IsNull(cJSON_GetObjectItem(e, "last_run"))) bad++;
    }
    ok(bad == 0 && haspod == cJSON_GetArraySize(jobs), "every listed job is a real `_` pod with interval > 0, cron derived from it, last_run null (not invented)");
    int onlypods = 1;
    for (int i = 0, n = registry_count(); i < n; i++) {
      const source_def *d = registry_all()[i];
      if (d && d->collector && d->collector[0] == '_' && d->update_interval_sec > 0) {
        int f = 0; cJSON_ArrayForEach(e, jobs) if (!strcmp(cJSON_GetObjectItem(e, "id")->valuestring, d->id)) f = 1;
        if (!f) onlypods = 0;
      }
    }
    ok(onlypods, "no pod with a schedule is missing from the listing");
    ok(cJSON_IsArray(src) && cJSON_GetArraySize(src) > 1000, "sources array carries the seeded registry");
    cJSON_Delete(s);
  }

  db_close(&db);
  if (g_fail) { printf("%d FAILED\n", g_fail); return 1; }
  printf("all passed\n");
  return 0;
}
