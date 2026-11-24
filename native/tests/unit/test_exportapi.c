/* test_exportapi.c — the one row exporter, driven through export_run /
 * export_plan_request with an in-memory writer (no socket).
 *
 *   plan: unknown kind/format are 400; format defaults to json; the filename is
 *     built from literal tables only (a hostile kind never reaches a header);
 *     breach needs ?source= (400) and role analyst+ (403); `case` is offered
 *     only while the cases table has a tenant column;
 *   tenancy: intel is the caller's rows plus the shared 'legacy' corpus and
 *     never another tenant's; entities are own + shared (NULL); alert_events
 *     and cases are strictly own;
 *   formats: CSV has a BOM, a header, RFC 4180 quoting, and a formula guard on
 *     text cells ('=', '+', '-', '@') that leaves numbers alone; GeoJSON drops
 *     rows with no geometry AND counts them (skipped_no_geometry); JSON carries
 *     meta.export;
 *   caps: the plan's row cap truncates, and truncation is stated in all three
 *     formats and in the audit row, never silent; exactly cap rows is NOT
 *     truncated; GeoJSON skips do not hide a truncation;
 *   filters: source, record_type, since/until, has_geom, tag and a keyset
 *     cursor narrow the rows; an unusable sort is refused 400 with the reason;
 *   breach: only the redacted projection (no hash), only the named source;
 *   audit: one export.run row per run with the true row count, also when the
 *     writer fails mid-stream (aborted), and none for a refused plan;
 *   a failing writer stops the walk. */
#include "../../core/exportapi.c"

#include <assert.h>

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

/* ── writers ─────────────────────────────────────────────────────────────── */
typedef struct { char *buf; size_t n, cap; size_t fail_after; int calls; } sinkbuf;
static int w_collect(void *ctx, const char *b, size_t len) {
  sinkbuf *s = ctx;
  s->calls++;
  if (s->fail_after && s->n >= s->fail_after) return 1;
  if (s->n + len + 1 > s->cap) {
    s->cap = (s->n + len + 1) * 2;
    s->buf = realloc(s->buf, s->cap);
  }
  memcpy(s->buf + s->n, b, len);
  s->n += len;
  s->buf[s->n] = 0;
  return 0;
}
static void sb_reset(sinkbuf *s) { free(s->buf); memset(s, 0, sizeof *s); }

static tenant_ctx T(const char *tid, const char *plan, const char *role) {
  tenant_ctx t; memset(&t, 0, sizeof t);
  snprintf(t.tenant_id, sizeof t.tenant_id, "%s", tid);
  snprintf(t.user_id, sizeof t.user_id, "u-%s", tid);
  snprintf(t.plan, sizeof t.plan, "%s", plan);
  snprintf(t.role, sizeof t.role, "%s", role);
  return t;
}
static int run(db_handle *db, const tenant_ctx *t, const char *kind, const char *fmt,
               const char *qs, sinkbuf *out, long *rows, int *status) {
  sb_reset(out);
  return export_run(db, t, kind, fmt, qs, w_collect, out, rows, status);
}
static int count_substr(const char *h, const char *n) {
  int c = 0; for (const char *p = h; (p = strstr(p, n)); p += strlen(n)) c++;
  return c;
}
static cJSON *jparse(const sinkbuf *s) { cJSON *j = cJSON_Parse(s->buf); assert(j && "output is valid JSON"); return j; }

int main(void) {
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "DELETE FROM intel_items");
  exec_ok(&db, "DELETE FROM entities");
  exec_ok(&db, "DELETE FROM alert_events");
  exec_ok(&db, "DELETE FROM audit_events");
  exec_ok(&db, "DELETE FROM breach_items");
  exec_ok(&db, "DELETE FROM cases");
  exec_ok(&db, "INSERT OR IGNORE INTO tenants(id,slug,name) VALUES('ta','ta','A'),('tb','tb','B')");
  unsetenv("JO_EXPORT_MAX_ROWS");

  exec_ok(&db,
    "INSERT INTO intel_items(uid,source_id,title,summary,fetched_at,published_at,language,tags,"
    "lat,lon,record_type,tenant_id) VALUES"
    "('i1','src1','Quake, big',  's','2026-03-01T00:00:00Z','2026-03-01T00:00:00Z','en','[\"x\",\"y\"]',35.5,139.5,'quake','ta'),"
    "('i2','src1','=HYPERLINK(\"http://evil\")','s','2026-02-01T00:00:00Z','2026-02-01T00:00:00Z','ja','[\"x\"]',NULL,NULL,'quake','ta'),"
    "('i3','src2','+cmd','s','2026-01-01T00:00:00Z',NULL,'en','[]',-33.5,151.0,'flood','ta'),"
    "('i4','src2','plain \"quoted\" title','s','2025-12-01T00:00:00Z',NULL,'en','[]',NULL,NULL,'flood','legacy'),"
    "('i5','srcB','TENANT-B-SECRET','s','2026-03-02T00:00:00Z',NULL,'en','[]',1.0,2.0,'quake','tb'),"
    "('i6','src1','@at','s','2025-11-01T00:00:00Z',NULL,'en','[]',NULL,NULL,'quake','ta')");
  exec_ok(&db, "UPDATE intel_items SET geometry='{\"type\":\"LineString\",\"coordinates\":[[0,0],[1,1]]}' WHERE uid='i3'");
  exec_ok(&db, "UPDATE intel_items SET summary='-5 degrees' WHERE uid='i6'");

  tenant_ctx ta = T("ta", "free", "viewer");
  sinkbuf o = {0}; long rows = 0; int st = 0;

  /* ── plan ────────────────────────────────────────────────────────────── */
  export_plan pl;
  assert(export_plan_request(&db, &ta, "nope", "csv", "", &pl, &st) == 1 && st == 400 &&
         !strcmp(pl.error, "unknown_kind"));
  assert(export_plan_request(&db, &ta, "intel", "xml", "", &pl, &st) == 1 && st == 400 &&
         !strcmp(pl.error, "unknown_format"));
  assert(export_plan_request(&db, &ta, NULL, "csv", "", &pl, &st) == 1 && st == 400);
  assert(export_plan_request(&db, &ta, "intel\r\nX-Evil: 1", "csv", "", &pl, &st) == 1 && st == 400);
  assert(export_plan_request(&db, &ta, "intel", NULL, "", &pl, &st) == 0 && st == 200 &&
         !strcmp(pl.content_type, "application/json") && strstr(pl.filename, "intel-") &&
         strstr(pl.filename, ".json") && "format defaults to json");
  assert(export_plan_request(&db, &ta, "alert_events", "csv", "", &pl, &st) == 0 &&
         strstr(pl.filename, "alert-events-") && strstr(pl.content_type, "text/csv"));
  assert(export_plan_request(&db, &ta, "intel", "geojson", "", &pl, &st) == 0 &&
         !strcmp(pl.content_type, "application/geo+json"));
  assert(!strpbrk(pl.filename, "\r\n\" /"));
  assert(export_plan_request(NULL, &ta, "intel", "csv", "", &pl, &st) == 1 && st == 500);
  assert(export_plan_request(&db, NULL, "intel", "csv", "", &pl, &st) == 1 && st == 500);
  assert(export_plan_request(&db, &ta, "breach", "csv", "", &pl, &st) == 1 && st == 400 &&
         !strcmp(pl.error, "source_required") && "breach is never an unfiltered feed");
  assert(export_plan_request(&db, &ta, "breach", "csv", "source=x", &pl, &st) == 1 && st == 403 &&
         "a viewer cannot bulk-export identifiers");
  tenant_ctx analyst = T("ta", "free", "analyst");
  assert(export_plan_request(&db, &analyst, "breach", "csv", "source=x", &pl, &st) == 0);
  tenant_ctx norole = T("ta", "free", "");
  assert(export_plan_request(&db, &norole, "breach", "csv", "source=x", &pl, &st) == 1 && st == 403 &&
         "an unknown role ranks below analyst");
  assert(scalar(&db, "SELECT COUNT(*) FROM audit_events WHERE action='export.run'") == 0 &&
         "a plan is not an export: no audit row");
  printf("  plan: 400/403/500, literal filename, breach gate: ok\n");

  /* ── refused runs write nothing and audit nothing ────────────────────── */
  assert(run(&db, &ta, "nope", "csv", "", &o, &rows, &st) == 1 && st == 400 && o.n == 0);
  assert(run(&db, &ta, "breach", "csv", "", &o, &rows, &st) == 1 && st == 400 && o.n == 0);
  assert(export_run(&db, &ta, "intel", "csv", "", NULL, NULL, &rows, &st) == 1 && st == 500);
  assert(scalar(&db, "SELECT COUNT(*) FROM audit_events WHERE action='export.run'") == 0);
  printf("  refused runs: nothing written, nothing audited: ok\n");

  /* ── intel / CSV: tenancy, BOM, quoting, formula guard ───────────────── */
  assert(run(&db, &ta, "intel", "csv", "", &o, &rows, &st) == 0 && st == 200);
  assert(rows == 5 && "ta's four plus the legacy row; never tenant b's");
  assert(!strstr(o.buf, "TENANT-B-SECRET"));
  assert((unsigned char)o.buf[0] == 0xEF && (unsigned char)o.buf[1] == 0xBB &&
         (unsigned char)o.buf[2] == 0xBF && "UTF-8 BOM for Excel");
  assert(!strncmp(o.buf + 3, "uid,source_id,title,summary,link,author,language,published_at,", 60));
  assert(strstr(o.buf, "\"Quake, big\"") && "a comma forces quotes");
  assert(strstr(o.buf, "\"plain \"\"quoted\"\" title\"") && "embedded quotes are doubled");
  assert(strstr(o.buf, "'=HYPERLINK") && strstr(o.buf, "'+cmd") && strstr(o.buf, "'@at") &&
         "text cells that start like formulas are neutralised");
  assert(!strstr(o.buf, ",=HYPERLINK") && !strstr(o.buf, ",+cmd"));
  assert(strstr(o.buf, "'-5 degrees") && "a leading '-' in TEXT is guarded too");
  assert(strstr(o.buf, ",35.5,139.5,") && "numeric cells are never prefixed");
  assert(strstr(o.buf, ",-33.5,151") && !strstr(o.buf, "'-33.5") && "a negative NUMBER stays a number");
  assert(count_substr(o.buf, "\r\n") == 6 && !strstr(o.buf, "truncated"));
  /* tenant b sees its own row and the legacy one */
  tenant_ctx tb = T("tb", "free", "viewer");
  assert(run(&db, &tb, "intel", "csv", "", &o, &rows, &st) == 0 && rows == 2 &&
         strstr(o.buf, "TENANT-B-SECRET") && !strstr(o.buf, "Quake"));
  printf("  intel/csv: tenancy, BOM, RFC 4180 quoting, formula guard, numbers intact: ok\n");

  /* ── filters ─────────────────────────────────────────────────────────── */
  run(&db, &ta, "intel", "json", "source=src1", &o, &rows, &st);       assert(rows == 3);
  run(&db, &ta, "intel", "json", "source=src1&record_type=quake&lang=ja", &o, &rows, &st);
  assert(rows == 1 && "filters AND together");
  run(&db, &ta, "intel", "json", "since=2026-01-15T00:00:00Z", &o, &rows, &st);   assert(rows == 2);
  run(&db, &ta, "intel", "json", "until=2026-01-15T00:00:00Z", &o, &rows, &st);   assert(rows == 3);
  run(&db, &ta, "intel", "json", "has_geom=yes", &o, &rows, &st);      assert(rows == 2);
  run(&db, &ta, "intel", "json", "has_geom=no", &o, &rows, &st);       assert(rows == 3);
  run(&db, &ta, "intel", "json", "tag=x", &o, &rows, &st);             assert(rows == 2);
  run(&db, &ta, "intel", "json", "tag=zzz", &o, &rows, &st);           assert(rows == 0);
  run(&db, &ta, "intel", "json", "limit=1&unknown=param", &o, &rows, &st);
  assert(rows == 5 && "limit is accepted and ignored; unknown params are ignored");
  run(&db, &ta, "intel", "json", "source=%27%20OR%201=1--", &o, &rows, &st);
  assert(rows == 0 && st == 200 && "an injection attempt is a literal that matches nothing");
  { /* keyset cursor {"p":..,"u":..}: resume after the first page */
    cJSON *c = cJSON_CreateObject();
    cJSON_AddStringToObject(c, "p", "2026-02-01T00:00:00Z");
    cJSON_AddStringToObject(c, "u", "i2");
    char *js = cJSON_PrintUnformatted(c); cJSON_Delete(c);
    static const char T64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    char enc[256]; size_t e = 0; unsigned v = 0; int bits = 0;
    for (const char *p = js; *p; p++) {
      v = (v << 8) | (unsigned char)*p; bits += 8;
      while (bits >= 6) { bits -= 6; enc[e++] = T64[(v >> bits) & 63]; }
    }
    if (bits) enc[e++] = T64[(v << (6 - bits)) & 63];
    enc[e] = 0; free(js);
    char qs[300]; snprintf(qs, sizeof qs, "cursor=%s", enc);
    run(&db, &ta, "intel", "json", qs, &o, &rows, &st);
    assert(rows == 3 && strstr(o.buf, "i3") && !strstr(o.buf, "\"uid\":\"i1\"") &&
           !strstr(o.buf, "\"uid\":\"i2\"") && "rows strictly after the cursor");
    run(&db, &ta, "intel", "json", "cursor=!!notbase64!!", &o, &rows, &st);
    assert(rows == 5 && "a bad cursor is ignored, as the items endpoint does");
  }
  run(&db, &ta, "intel", "json", "sort=trust", &o, &rows, &st);
  assert(st == 400 && strstr(o.buf, "sort_not_honoured") && rows == 0 && "an unhonourable sort is refused in-band");
  run(&db, &ta, "intel", "json", "sort=relevance", &o, &rows, &st);
  assert(st == 400 && "relevance needs q");
  run(&db, &ta, "intel", "json", "sort=date", &o, &rows, &st);
  assert(st == 200 && rows == 5);
  printf("  filters, cursor, injection literal, refused sorts: ok\n");

  /* ── JSON and GeoJSON shapes ─────────────────────────────────────────── */
  run(&db, &ta, "intel", "json", "source=src1&tag=x", &o, &rows, &st);
  cJSON *j = jparse(&o);
  cJSON *data = cJSON_GetObjectItem(j, "data");
  assert(cJSON_GetArraySize(data) == 2 && rows == 2);
  cJSON *ex = cJSON_GetObjectItem(cJSON_GetObjectItem(j, "meta"), "export");
  assert(cJSON_IsFalse(cJSON_GetObjectItem(ex, "truncated")) &&
         cJSON_GetObjectItem(ex, "rows")->valueint == 2 &&
         cJSON_GetObjectItem(ex, "row_cap")->valueint == 1000 &&
         !strcmp(cJSON_GetObjectItem(ex, "plan")->valuestring, "free"));
  assert(cJSON_IsNumber(cJSON_GetObjectItem(cJSON_GetArrayItem(data, 0), "lat")));
  assert(cJSON_IsArray(cJSON_GetObjectItem(cJSON_GetArrayItem(data, 0), "tags")) && "JSON columns are real JSON");
  assert(cJSON_IsNull(cJSON_GetObjectItem(cJSON_GetArrayItem(data, 1), "lat")) && "NULL stays null");
  assert(!strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "meta"), "filters"),
                                     "source")->valuestring, "src1") && "the filters are echoed");
  cJSON_Delete(j);

  run(&db, &ta, "intel", "geojson", "", &o, &rows, &st);
  j = jparse(&o);
  assert(!strcmp(cJSON_GetObjectItem(j, "type")->valuestring, "FeatureCollection"));
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(j, "features")) == 2 && rows == 2 &&
         "only rows with geometry become features");
  cJSON *pr = cJSON_GetObjectItem(j, "properties");
  assert(cJSON_GetObjectItem(pr, "skipped_no_geometry")->valueint == 3 && "and the skips are counted");
  cJSON *f0 = cJSON_GetArrayItem(cJSON_GetObjectItem(j, "features"), 0);
  assert(!cJSON_GetObjectItem(cJSON_GetObjectItem(f0, "properties"), "geometry") &&
         !cJSON_GetObjectItem(cJSON_GetObjectItem(f0, "properties"), "lat") == 0 &&
         "geometry is not duplicated into properties");
  int saw_line = 0, saw_pt = 0;
  cJSON *ft;
  cJSON_ArrayForEach(ft, cJSON_GetObjectItem(j, "features")) {
    const char *ty = cJSON_GetObjectItem(cJSON_GetObjectItem(ft, "geometry"), "type")->valuestring;
    if (!strcmp(ty, "LineString")) saw_line = 1;   /* stored geometry wins over lat/lon */
    if (!strcmp(ty, "Point")) saw_pt = 1;
  }
  assert(saw_line && saw_pt);
  cJSON_Delete(j);
  printf("  JSON meta.export; GeoJSON skips counted, stored geometry wins: ok\n");

  /* ── caps and truncation ─────────────────────────────────────────────── */
  exec_ok(&db, "WITH RECURSIVE n(i) AS (SELECT 1 UNION ALL SELECT i+1 FROM n WHERE i<1100) "
               "INSERT INTO intel_items(uid,source_id,title,fetched_at,record_type,tenant_id,lat,lon)"
               " SELECT 'bulk'||i,'bulk','t'||i,'2026-01-01T00:00:00Z','bulk','tbulk',"
               "CASE WHEN i%2=0 THEN 1.0 END, CASE WHEN i%2=0 THEN 2.0 END FROM n");
  exec_ok(&db, "INSERT OR IGNORE INTO tenants(id,slug,name) VALUES('tbulk','tbulk','Bulk')");
  tenant_ctx tf = T("tbulk", "free", "viewer");
  run(&db, &tf, "intel", "csv", "source=bulk", &o, &rows, &st);
  assert(rows == 1000 && strstr(o.buf, "# truncated=true; plan=\"free\" row_cap=1000"));
  run(&db, &tf, "intel", "json", "source=bulk", &o, &rows, &st);
  j = jparse(&o);
  assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "meta"), "export"), "truncated")));
  cJSON_Delete(j);
  run(&db, &tf, "intel", "geojson", "source=bulk", &o, &rows, &st);
  j = jparse(&o);
  pr = cJSON_GetObjectItem(j, "properties");
  assert(cJSON_IsTrue(cJSON_GetObjectItem(pr, "truncated")) &&
         cJSON_GetObjectItem(pr, "skipped_no_geometry")->valueint > 0 &&
         "the cap counts rows SCANNED, so skipped rows cannot hide a truncation");
  assert(cJSON_GetObjectItem(pr, "rows")->valueint + cJSON_GetObjectItem(pr, "skipped_no_geometry")->valueint == 1000);
  cJSON_Delete(j);
  assert(scalar(&db, "SELECT json_extract(payload_json,'$.truncated') FROM audit_events"
                     " WHERE action='export.run' ORDER BY rowid DESC LIMIT 1") == 1 &&
         "the audit row says truncated too");
  /* exactly cap rows is complete */
  exec_ok(&db, "DELETE FROM intel_items WHERE source_id='bulk' AND CAST(substr(uid,5) AS INT) > 1000");
  run(&db, &tf, "intel", "csv", "source=bulk", &o, &rows, &st);
  assert(rows == 1000 && !strstr(o.buf, "truncated") && "exactly the cap is not truncated");
  /* plans */
  assert(plan_row_cap("pro") == 50000 && plan_row_cap("team") == 250000 &&
         plan_row_cap("") == 1000 && plan_row_cap(NULL) == 1000 && plan_row_cap("bogus") == 1000);
  assert(plan_row_cap("enterprise") == EXPORT_ENTERPRISE_CAP && "enterprise has a ceiling too");
  setenv("JO_EXPORT_MAX_ROWS", "7", 1);
  assert(plan_row_cap("enterprise") == 7);
  tenant_ctx te = T("tbulk", "enterprise", "owner");
  run(&db, &te, "intel", "csv", "source=bulk", &o, &rows, &st);
  assert(rows == 7 && strstr(o.buf, "row_cap=7") && "the operator's ceiling applies and is stated");
  unsetenv("JO_EXPORT_MAX_ROWS");
  printf("  caps: scanned-not-emitted, stated in every format and the audit row: ok\n");

  /* ── entities, alert events, cases ───────────────────────────────────── */
  exec_ok(&db, "INSERT INTO entities(entity_id,type,canonical,norm_key,tenant_id,mention_count,aliases_json) VALUES"
               "('E1','person','Alice','alice','ta',5,'[\"Al\"]'),('E2','org','Shared Co','sharedco',NULL,9,'[]'),"
               "('E3','person','Bob B','bobb','tb',1,'[]')");
  run(&db, &ta, "entities", "json", "", &o, &rows, &st);
  assert(rows == 2 && strstr(o.buf, "Alice") && strstr(o.buf, "Shared Co") && !strstr(o.buf, "Bob B"));
  assert(strstr(o.buf, "\"entity_id\":\"E2\"") < strstr(o.buf, "\"entity_id\":\"E1\"") && "ordered by mentions");
  run(&db, &ta, "entities", "json", "type=PERSON", &o, &rows, &st);
  assert(rows == 1 && "type is case-folded");
  exec_ok(&db, "INSERT INTO alert_events(id,tenant_id,rule_id,item_uid,matched_at,suppressed,reason,rule_name) VALUES"
               "('ev1','ta','r1','i1','2026-03-01T00:00:00Z',0,NULL,'Rule One'),"
               "('ev2','ta','r1','i2','2026-03-02T00:00:00Z',1,'dedup','Rule One'),"
               "('ev3','tb','r9','i5','2026-03-03T00:00:00Z',0,NULL,'B rule')");
  run(&db, &ta, "alert_events", "csv", "", &o, &rows, &st);
  assert(rows == 2 && !strstr(o.buf, "B rule") && strstr(o.buf, "true") && strstr(o.buf, "false") &&
         "booleans are true/false in CSV");
  run(&db, &ta, "alert_events", "json", "rule_id=r1&since=2026-03-02T00:00:00Z", &o, &rows, &st);
  assert(rows == 1 && strstr(o.buf, "dedup"));
  exec_ok(&db, "INSERT INTO cases(id,tenant_id,name) VALUES('c1','ta','Case A'),('c2','tb','Case B')");
  run(&db, &ta, "case", "json", "", &o, &rows, &st);
  assert(rows == 1 && strstr(o.buf, "Case A") && !strstr(o.buf, "Case B") && !strstr(o.buf, "tenant_id"));
  assert(!col_is_sensitive("name") && col_is_sensitive("api_KEY") && col_is_sensitive("pwd_hash") &&
         col_is_sensitive("Secret_note") && col_is_sensitive("TOKEN") && !col_is_sensitive("status"));
  exec_ok(&db, "ALTER TABLE cases RENAME TO cases_hold");
  assert(export_plan_request(&db, &ta, "case", "json", "", &pl, &st) == 1 && st == 501 &&
         !strcmp(pl.error, "kind_unavailable"));
  exec_ok(&db, "CREATE TABLE cases(id TEXT, name TEXT)");     /* no tenant_id: isolation unprovable */
  assert(export_plan_request(&db, &ta, "case", "json", "", &pl, &st) == 1 && st == 501 &&
         "a cases table with no tenant column is refused");
  exec_ok(&db, "DROP TABLE cases");
  exec_ok(&db, "ALTER TABLE cases_hold RENAME TO cases");
  printf("  entities/alert_events/cases: tenancy, ordering, 501 without a tenant column: ok\n");

  /* ── breach: redacted projection, per source ─────────────────────────── */
  exec_ok(&db, "INSERT INTO breach_meta(breach_id,name) VALUES('acme','Acme Breach')");
  exec_ok(&db, "INSERT INTO breach_items(keyid,type,value,source_id,hash,has_secret,count) VALUES"
               "('email:K1','email','alice@example.com','acme','HASH1111HASH',0,1),"
               "('password:K2','password',NULL,'acme','HASH2222HASH',1,3),"
               "('email:K3','email','other@example.com','other','HASH3333HASH',0,1)");
  run(&db, &analyst, "breach", "csv", "source=acme", &o, &rows, &st);
  assert(rows == 2 && strstr(o.buf, "alice@example.com") && !strstr(o.buf, "other@example.com"));
  assert(!strstr(o.buf, "HASH1111") && !strstr(o.buf, "HASH2222") && !strstr(o.buf, "hash") &&
         "the hash column is never selected");
  assert(strstr(o.buf, "Acme Breach") && strstr(o.buf, "breach:email:K1") && strstr(o.buf, "true"));
  run(&db, &analyst, "breach", "json", "source=acme&cursor=1", &o, &rows, &st);
  assert(rows == 1 && strstr(o.buf, "password:K2") && "the breach cursor is the row id");
  run(&db, &analyst, "breach", "json", "source=missing", &o, &rows, &st);
  assert(rows == 0 && st == 200);
  printf("  breach: redacted projection, one source, cursor: ok\n");

  /* ── a failing writer stops the walk and is still audited ────────────── */
  long before = scalar(&db, "SELECT COUNT(*) FROM audit_events WHERE action='export.run'");
  sinkbuf bad = {0}; bad.fail_after = 1;
  rows = -1;
  assert(export_run(&db, &tf, "intel", "json", "source=bulk", w_collect, &bad, &rows, &st) == 1);
  assert(bad.calls < 20 && "the walk stopped instead of running 1000 rows into a dead peer");
  assert(scalar(&db, "SELECT COUNT(*) FROM audit_events WHERE action='export.run'") == before + 1);
  assert(scalar(&db, "SELECT json_extract(payload_json,'$.aborted') FROM audit_events"
                     " WHERE action='export.run' ORDER BY rowid DESC LIMIT 1") == 1 &&
         "the half-delivered export is an audited egress event");
  free(bad.buf);
  printf("  failing writer: walk stops, audited as aborted: ok\n");

  /* ── one audit row per good run, with the truth in it ────────────────── */
  exec_ok(&db, "DELETE FROM audit_events");
  run(&db, &ta, "intel", "csv", "source=src1&record_type=quake", &o, &rows, &st);
  assert(scalar(&db, "SELECT COUNT(*) FROM audit_events") == 1);
  assert(scalar(&db, "SELECT json_extract(payload_json,'$.rows') FROM audit_events") == rows);
  assert(scalar(&db, "SELECT target='intel/csv' AND user_id='u-ta' AND tenant_id='ta' FROM audit_events") == 1);
  assert(scalar(&db, "SELECT json_extract(payload_json,'$.filters.record_type')='quake' FROM audit_events") == 1 &&
         "the filters are in the audit payload");
  printf("  audit: one row per run, true count, filters recorded: ok\n");

  sb_reset(&o);
  db_close(&db);
  printf("test_exportapi: all passed\n");
  return 0;
}
