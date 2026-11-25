/* tests/unit/test_pagination_probe.c — core/pagination_probe.c without a
 * network: candidate detection, the family/generic continuation parameter
 * choice, record fingerprints (JSON shapes and CSV) and their overlap, the
 * job-table envelope, and the start gates (no database, probe already
 * running).  The survey thread itself fetches real URLs and is not started.
 * Includes the .c, so run.sh excludes obj/core/pagination_probe.o. */
#include "../../core/pagination_probe.c"
#include <assert.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}
static source_def sd(const char *url) {
  source_def d;
  memset(&d, 0, sizeof d);
  d.id = "pp-test";
  d.url = url;
  return d;
}

int main(void) {
  printf("-- is_candidate\n");
  long sz = 0;
  const char *sp = NULL;
  source_def d = sd("https://example.org/api/items?limit=25");
  ok(is_candidate(&d, &sz, &sp) && sz == 25 && !strcmp(sp, "limit"), "a URL declaring limit=25 with no continuation is a candidate");
  d = sd("https://example.org/api/items?limit=25&offset=0");
  ok(!is_candidate(&d, &sz, &sp), "one that already binds an offset is not");
  d = sd("https://example.org/api/items");
  ok(!is_candidate(&d, &sz, &sp), "no declared size: not a candidate");
  d = sd("ftp://example.org/x?limit=25");
  ok(!is_candidate(&d, &sz, &sp), "non-http(s) is not");
  d = sd(NULL);
  ok(!is_candidate(&d, &sz, &sp) && !is_candidate(NULL, &sz, &sp), "NULL url / NULL def are not");
  d = sd("http://example.org/x?per_page=30");
  ok(is_candidate(&d, &sz, &sp) && sz == 30, "http (not only https), per_page spelling");
  ok(pagination_probe_candidates() >= 0, "the registry census runs");

  printf("-- candidates_for\n");
  ok(!strcmp(candidates_for("https://h/api/3/action/package_search?rows=10", "rows")[0].name, "offset"), "CKAN -> offset");
  ok(!strcmp(candidates_for("https://h/x/FeatureServer/0/query?resultRecordCount=10", "resultRecordCount")[0].name, "resultOffset"), "ArcGIS FeatureServer -> resultOffset");
  ok(!strcmp(candidates_for("https://h/x/MapServer/0/query?resultRecordCount=10", "resultRecordCount")[0].name, "resultOffset"), "ArcGIS MapServer -> resultOffset");
  ok(!strcmp(candidates_for("https://h/odata/T?$top=50", "$top")[0].name, "$skip"), "OData -> $skip");
  ok(!strcmp(candidates_for("https://h/solr/c/select?wt=json&rows=5", "rows")[0].name, "start"), "Solr -> start");
  const pw_cand *g = candidates_for("https://h/x?limit=5", "limit");
  ok(!strcmp(g[0].name, "offset") && !g[0].is_page, "generic: offset first");
  const pw_cand *pn = candidates_for("https://h/x?per_page=5", "per_page");
  ok(!strcmp(pn[0].name, "page") && pn[0].is_page, "a page-NUMBERED size parameter tries 'page' (1-based) first");

  printf("-- url_with\n");
  char *u = url_with("https://h/x", "offset", 25);
  ok(u && !strcmp(u, "https://h/x?offset=25"), "adds with '?'");
  free(u);
  u = url_with("https://h/x?a=1", "page", 2);
  ok(u && !strcmp(u, "https://h/x?a=1&page=2"), "adds with '&' when a query exists");
  free(u);

  printf("-- record_array / fingerprint / overlap\n");
  cJSON *doc = cJSON_Parse("{\"meta\":{\"n\":3},\"items\":[{\"a\":1},{\"a\":2},{\"a\":3}],\"tiny\":[{\"a\":1}]}");
  ok(cJSON_GetArraySize(record_array(doc, 0)) == 3, "the longest array of objects is the record set");
  cJSON_Delete(doc);
  doc = cJSON_Parse("{\"data\":{\"results\":[{\"a\":1},{\"a\":2}]}}");
  ok(cJSON_GetArraySize(record_array(doc, 0)) == 2, "found one object level down");
  cJSON_Delete(doc);
  doc = cJSON_Parse("{\"items\":[]}");
  ok(record_array(doc, 0) != NULL && cJSON_GetArraySize(record_array(doc, 0)) == 0, "an EMPTY array is returned (page 2 empty is an answer)");
  cJSON_Delete(doc);
  doc = cJSON_Parse("{\"a\":1,\"b\":\"x\"}");
  ok(record_array(doc, 0) == NULL, "no array at all: NULL");
  cJSON_Delete(doc);
  ok(record_array(NULL, 0) == NULL, "NULL doc");
  doc = cJSON_Parse("[1,2,3]");
  ok(record_array(doc, 0) == doc, "a bare root array is itself");
  cJSON_Delete(doc);

  fpset a, b;
  ok(fingerprint("[{\"id\":1},{\"id\":2},{\"id\":3}]", &a) && a.n == 3, "json array fingerprints");
  ok(fingerprint("[{\"id\":3},{\"id\":1},{\"id\":2}]", &b) && overlap(&a, &b) == 1.0, "same records in another order overlap 1.0 (sorted compare)");
  ok(fingerprint("[{\"id\":7},{\"id\":8}]", &b) && overlap(&a, &b) == 0.0, "disjoint pages overlap 0");
  ok(fingerprint("[{\"id\":1},{\"id\":9}]", &b) && overlap(&a, &b) == 0.5, "half shared -> 0.5 (fraction of the SECOND page)");
  fpset empty;
  ok(fingerprint("[]", &empty) && empty.n == 0 && overlap(&a, &empty) == 0.0, "an empty second page overlaps 0 (and n==0 is how 'complete' is told)");
  ok(!fingerprint("{\"a\":1}", &b), "JSON with no record array: unreadable, not zero records");
  ok(!fingerprint(NULL, &b), "NULL body");
  ok(fingerprint("{\"envelope\":\"2026-01-01T00:00:00Z\",\"rows\":[{\"id\":1},{\"id\":2}]}", &a) &&
         fingerprint("{\"envelope\":\"2026-01-01T00:00:09Z\",\"rows\":[{\"id\":2},{\"id\":1}]}", &b) && overlap(&a, &b) == 1.0,
     "a generated-at stamp in the envelope does not make identical pages differ");

  /* CSV: header skipped, and a different row order must still be recognised as the same rows */
  ok(fingerprint("id,name\nA,1\nB,2\nC,3\n", &a) && a.n == 3, "csv: header skipped, 3 rows");
  ok(fingerprint("id,name\nC,3\nB,2\nA,1\n", &b) && overlap(&a, &b) == 1.0,
     "csv: the same rows in reverse order overlap 1.0 (the CSV set must be sorted like the JSON one)");
  ok(fingerprint("id,name\nB,2\nX,9\nA,1\nY,8\n", &b) && overlap(&a, &b) == 0.5, "csv: partially shared rows in a different order -> 0.5");
  ok(fingerprint("id,name\nA,1\nB,2\n\n\nC,3", &b) && b.n == 3, "csv: blank lines skipped, last line without newline counted");
  ok(!fingerprint("", &b), "empty body: unreadable");

  printf("-- job table\n");
  char *s = pagination_probe_status(NULL);
  cJSON *j = cJSON_Parse(s);
  free(s);
  ok(cJSON_IsArray(cJSON_GetObjectItem(j, "data")) && cJSON_GetObjectItem(j, "candidates_now")->valueint >= 0, "no job: data [] and the live candidate census");
  cJSON_Delete(j);
  s = pagination_probe_status("pp-nope");
  j = cJSON_Parse(s);
  free(s);
  ok(cJSON_IsNull(cJSON_GetObjectItem(j, "data")), "unknown job id -> data null, not an error");
  cJSON_Delete(j);
  g_jobs[1].used = 1;
  snprintf(g_jobs[1].id, sizeof g_jobs[1].id, "pp-77");
  snprintf(g_jobs[1].state, sizeof g_jobs[1].state, "done");
  snprintf(g_jobs[1].msg, sizeof g_jobs[1].msg, "all done");
  g_jobs[1].total = 5; g_jobs[1].done = 4; g_jobs[1].paginates = 1; g_jobs[1].ignored = 1; g_jobs[1].complete_already = 1; g_jobs[1].errored = 1;
  g_jobs[1].started = 100; g_jobs[1].ended = 200;
  s = pagination_probe_status("pp-77");
  j = cJSON_Parse(s);
  free(s);
  cJSON *dj = cJSON_GetObjectItem(j, "data");
  ok(dj && !strcmp(cJSON_GetObjectItem(dj, "state")->valuestring, "done") && cJSON_GetObjectItem(dj, "probed")->valueint == 4 &&
         cJSON_GetObjectItem(dj, "ignores_offset")->valueint == 1 && cJSON_GetObjectItem(dj, "ended_at")->valueint == 200,
     "one job by id, counters and ended_at present");
  cJSON_Delete(j);
  s = pagination_probe_status("");
  j = cJSON_Parse(s);
  free(s);
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(j, "data")) == 1, "empty id lists all jobs");
  cJSON_Delete(j);

  printf("-- start gates\n");
  int st = 0;
  s = pagination_probe_start(NULL, 0, &st);
  ok(st == 503 && strstr(s, "no_database"), "no database -> 503");
  free(s);
  db_handle shared;
  memset(&shared, 0, sizeof shared);
  s = pagination_probe_start(&shared, 0, &st);
  ok(st == 503, "a handle with no sqlite connection -> 503");
  free(s);
  g_busy = 1;
  db_handle fake;
  memset(&fake, 0, sizeof fake);
  fake.h = (sqlite3 *)0x1;
  s = pagination_probe_start(&fake, 0, &st);
  ok(st == 409 && strstr(s, "probe_already_running"), "a survey already running -> 409, no second thread");
  free(s);
  g_busy = 0;
  pagination_probe_stop_all();
  ok(g_probe_stop == 1, "stop_all raises the stop flag");
  g_probe_stop = 0;

  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
