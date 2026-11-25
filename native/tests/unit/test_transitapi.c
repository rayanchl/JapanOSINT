/* tests/unit/test_transitapi.c — core/transitapi.c through its dispatcher.
 * Pins the routing contract (NULL only for an unknown/mis-shaped subpath;
 * recognised routes always return a JSON body, errors included), the query
 * parsing helpers' JS-compatible quirks, /routes (mode/bbox validation, bbox
 * clip, vertex-less lines dropped), /gtfs/operators, the honest 501 bodies for
 * the unported routes, the departures board (service calendar, limit clamp,
 * ordering — fixture times are relative to JST "now"), station summary lookup
 * (LIKE wildcards in the uid are NOT wildcards) and the vehicle uid parser.
 * Includes the .c so the static helpers are reachable; run.sh excludes
 * obj/core/transitapi.o from this test's link line. */
#include "../../core/transitapi.c"
#include <assert.h>

static int g_fail = 0;
static void ok(int c, const char *w) {
  printf("%s  %s\n", c ? "  ok  " : "FAIL  ", w);
  if (!c) g_fail++;
}
static db_handle db;
static void ex(const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db.h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql: %s\n  %s\n", e ? e : "?", sql);
    exit(1);
  }
}
static cJSON *call(const char *path, const char *query) {
  char *s = transitapi(&db, path, query);
  if (!s) return NULL;
  cJSON *j = cJSON_Parse(s);
  free(s);
  assert(j);
  return j;
}
static const char *errs(cJSON *j) {
  cJSON *e = j ? cJSON_GetObjectItem(j, "error") : NULL;
  return cJSON_IsString(e) ? e->valuestring : "";
}

int main(void) {
  assert(db_open(&db, NULL, NULL) == 0);

  printf("-- query helpers\n");
  char b[64];
  ok(qget("a=1&b=2", "b", b, sizeof b) && !strcmp(b, "2"), "qget finds a later key");
  ok(!qget("a=1", "b", b, sizeof b) && b[0] == 0, "qget miss clears the buffer");
  ok(qget("q=%E6%9D%B1%2B+x", "q", b, sizeof b) && !strcmp(b, "東+ x") , "percent-decoding; '+' is a space");
  ok(!qget(NULL, "a", b, sizeof b) && !qget("", "a", b, sizeof b), "NULL/empty query");
  ok(qget("a=1&a=2", "a", b, sizeof b) && !strcmp(b, "1"), "first of a repeated key wins");
  ok(qget("ab=1&a=2", "a", b, sizeof b) && !strcmp(b, "2"), "a key is matched whole, not as a prefix");
  ok(qint("limit=7", "limit", 5, 1, 20) == 7, "qint in range");
  ok(qint("limit=99", "limit", 5, 1, 20) == 20 && qint("limit=-4", "limit", 5, 1, 20) == 1, "qint clamps both ways");
  ok(qint("limit=abc", "limit", 5, 1, 20) == 5 && qint("limit=0", "limit", 5, 1, 20) == 5 && qint("limit=", "limit", 5, 1, 20) == 5 &&
         qint("", "limit", 5, 1, 20) == 5, "qint: NaN / 0 / empty / absent fall to the default (JS `Number(x) || def`)");
  ok(js_number("  12 ") == 12 && js_number("") == 0 && isnan(js_number("12x")) && isnan(js_number("x")), "js_number matches Number() on the cases that matter");
  bbox_t bb;
  ok(parse_bbox("bbox=139,35,140,36", &bb) == 1 && bb.minLng == 139 && bb.maxLat == 36, "bbox parsed");
  ok(parse_bbox("", &bb) == 0 && parse_bbox("mode=x", &bb) == 0 && parse_bbox("bbox=", &bb) == 0, "no bbox -> 0");
  ok(parse_bbox("bbox=1,2,3", &bb) == -1 && parse_bbox("bbox=1,2,3,4,5", &bb) == -1, "wrong count -> -1");
  ok(parse_bbox("bbox=a,2,3,4", &bb) == -1 && parse_bbox("bbox=1,2,3,Infinity", &bb) == -1 && parse_bbox("bbox=1,2,3,1e999", &bb) == -1,
     "non-finite component -> -1");

  printf("-- routing contract\n");
  ok(transitapi(NULL, "/routes", NULL) == NULL && transitapi(&db, NULL, NULL) == NULL, "NULL db / path -> NULL");
  const char *unknown[] = { "/", "", "/nope", "/routes/extra", "/gtfs", "/gtfs/stop/x", "/gtfs/stop//departures", "/gtfs/stop/x/departuresX",
                            "/station//summary", "/station/x", "/vehicle/x", "/vehicle//info", "/gtfs/hydrate/", "/gtfs/hydrate/a/b" };
  int all_null = 1;
  for (size_t i = 0; i < sizeof unknown / sizeof *unknown; i++) {
    char *s = transitapi(&db, unknown[i], NULL);
    if (s) { all_null = 0; printf("   not NULL: '%s' -> %s\n", unknown[i], s); free(s); }
  }
  ok(all_null, "unknown or mis-shaped subpaths return NULL (the caller's 404)");
  cJSON *j = call("/gtfs/hydrate/someorg", NULL);
  ok(!strcmp(errs(j), "not_implemented"), "hydrate: honest not_implemented body, not a fake success");
  cJSON_Delete(j);
  j = call("/station-boundaries", NULL);
  ok(!strcmp(errs(j), "not_implemented"), "station-boundaries: honest not_implemented, not an empty FeatureCollection");
  cJSON_Delete(j);

  printf("-- /routes\n");
  j = call("/routes", NULL);
  ok(!strcmp(errs(j), "mode must be train|subway|bus"), "no mode -> 400-body");
  cJSON_Delete(j);
  j = call("/routes", "mode=tram");
  ok(!strcmp(errs(j), "mode must be train|subway|bus"), "unknown mode refused");
  cJSON_Delete(j);
  j = call("/routes", "mode=train&bbox=1,2,3");
  ok(!strncmp(errs(j), "bbox must be", 12), "bad bbox refused");
  cJSON_Delete(j);
  ex("DELETE FROM intel_items WHERE source_id LIKE 'unified-%'");
  ex("INSERT INTO intel_items(uid,source_id,fetched_at,record_type,tenant_id,geometry,properties) VALUES"
     "('unified-trains|l1','unified-trains','2026-01-01','line','legacy','{\"type\":\"LineString\",\"coordinates\":[[139.0,35.0],[139.1,35.1]]}','{\"line_uid\":\"L1\",\"name\":\"Yamanote\",\"line_color\":\"#9acd32\"}'),"
     "('unified-trains|l2','unified-trains','2026-01-01','line','legacy','{\"type\":\"LineString\",\"coordinates\":[[150.0,40.0],[150.1,40.1]]}','{\"line_uid\":\"L2\"}'),"
     "('unified-trains|l3','unified-trains','2026-01-01','line','legacy','{\"type\":\"LineString\",\"coordinates\":[[139.0,35.0]]}','{}'),"
     "('unified-trains|p1','unified-trains','2026-01-01','line','legacy','{\"type\":\"Point\",\"coordinates\":[139.0,35.0]}','{}'),"
     "('unified-subways|s1','unified-subways','2026-01-01','line','legacy','{\"type\":\"LineString\",\"coordinates\":[[139.7,35.6],[139.8,35.7]]}','{\"name\":\"Ginza\"}')");
  j = call("/routes", "mode=train");
  cJSON *feats = cJSON_GetObjectItem(j, "features");
  ok(cJSON_GetArraySize(feats) == 2, "train: 2 lines (the one-vertex line and the Point are not lines)");
  ok(cJSON_GetObjectItem(j, "_meta") == NULL, "not truncated: no _meta");
  cJSON *p0 = cJSON_GetObjectItem(cJSON_GetArrayItem(feats, 0), "properties");
  ok(!strcmp(cJSON_GetObjectItem(p0, "route_id")->valuestring, "L1") && !strcmp(cJSON_GetObjectItem(p0, "line_color")->valuestring, "#9acd32"), "route_id/name/line_color shaped");
  cJSON *p1 = cJSON_GetObjectItem(cJSON_GetArrayItem(feats, 1), "properties");
  ok(cJSON_IsNull(cJSON_GetObjectItem(p1, "name")) && cJSON_IsNull(cJSON_GetObjectItem(p1, "line_color")), "absent fields are null, not invented");
  cJSON_Delete(j);
  j = call("/routes", "mode=TRAIN&bbox=138.9,34.9,139.5,35.5");
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(j, "features")) == 1, "mode is case-insensitive; bbox keeps only the line with a vertex inside");
  cJSON_Delete(j);
  j = call("/routes", "mode=subway");
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(j, "features")) == 1, "subway mode reads unified-subways only");
  cJSON_Delete(j);
  j = call("/routes", "mode=bus");
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(j, "features")) == 0, "no bus rows: empty collection");
  cJSON_Delete(j);
  ok(line_intersects_bbox(cJSON_Parse("[[\"x\",1],[2,2]]"), &(bbox_t){ 0, 0, 3, 3 }) == 1, "a non-numeric vertex is skipped, not read as 0");

  printf("-- /gtfs/operators\n");
  j = call("/gtfs/operators", NULL);
  ok(cJSON_IsArray(cJSON_GetObjectItem(j, "operators")), "operators envelope");
  cJSON_Delete(j);
  ex("DELETE FROM gtfs_operators");
  ex("INSERT INTO gtfs_operators(org_id,org_name,hydrated_at,stop_count,trip_count) VALUES"
     "('o1','Old','2026-01-01',1,2),('o2','New','2026-06-01',3,4),('o3','Never',NULL,0,0)");
  j = call("/gtfs/operators", NULL);
  cJSON *ops = cJSON_GetObjectItem(j, "operators");
  ok(cJSON_GetArraySize(ops) == 3 && !strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(ops, 0), "org_id")->valuestring, "o2"), "ordered by hydrated_at, newest first");
  ok(cJSON_IsNull(cJSON_GetObjectItem(cJSON_GetArrayItem(ops, 2), "hydrated_at")), "a never-hydrated operator shows null, not a made-up date");
  cJSON_Delete(j);

  printf("-- departures board\n");
  j = call("/gtfs/stop//departures", NULL);
  ok(j == NULL, "empty stop id is a shape mismatch (NULL)");
  j = call("/gtfs/stop/NOPE/departures", "t=notadate");
  ok(!strcmp(errs(j), "bad t (ISO date)"), "an explicit t with no digits -> error body");
  cJSON_Delete(j);
  j = call("/gtfs/stop/NOPE/departures", NULL);
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(j, "departures")) == 0 && !strcmp(cJSON_GetObjectItem(j, "stop_id")->valuestring, "NOPE"),
     "unknown stop: empty list, stop id echoed");
  cJSON_Delete(j);
  struct tm lt;
  jst_now(&lt);
  int now = lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec;
  if (now > 86400 - 4000) {
    printf("  (departure fixtures skipped: too close to JST midnight)\n");
  } else {
    char sql[1500];
    ex("DELETE FROM gtfs_stop_times WHERE org_id='o'; DELETE FROM gtfs_trips WHERE org_id='o'; DELETE FROM gtfs_calendar WHERE org_id='o'; "
       "DELETE FROM gtfs_calendar_dates WHERE org_id='o'; DELETE FROM gtfs_routes WHERE org_id='o'");
    ex("INSERT INTO gtfs_routes(org_id,feed_id,route_id,short_name,long_name,color) VALUES('o','f','R1','1','Line One','FF0000'),('o','f','R2',NULL,'Line Two',NULL)");
    ex("INSERT INTO gtfs_calendar(org_id,feed_id,service_id,mon,tue,wed,thu,fri,sat,sun,start_date,end_date) VALUES"
       "('o','f','daily',1,1,1,1,1,1,1,NULL,NULL),('o','f','never',0,0,0,0,0,0,0,NULL,NULL),('o','f','expired',1,1,1,1,1,1,1,'20000101','20001231')");
    ex("INSERT INTO gtfs_trips(org_id,feed_id,trip_id,route_id,service_id,headsign) VALUES"
       "('o','f','TA','R1','daily','North'),('o','f','TB','R2','daily',NULL),('o','f','TC','R1','never','Ghost'),"
       "('o','f','TD','R1','expired','Old'),('o','f','TE','R1','daily','Soon'),('o','f','TF','R1','daily','Later')");
    snprintf(sql, sizeof sql,
             "INSERT INTO gtfs_stop_times(org_id,feed_id,trip_id,stop_sequence,stop_id,arrival_sec,departure_sec) VALUES"
             "('o','f','TA',1,'STOP',%d,%d),('o','f','TB',1,'STOP',%d,%d),('o','f','TC',1,'STOP',%d,%d),('o','f','TD',1,'STOP',%d,%d),"
             "('o','f','TE',1,'STOP',%d,%d),('o','f','TF',1,'STOP',%d,%d)",
             now + 1800, now + 1800, now + 600, now + 600, now + 300, now + 300, now + 300, now + 300,
             now + 120, now + 120, now + 3000, now + 3000);
    ex(sql);
    j = call("/gtfs/stop/STOP/departures", "limit=20");
    cJSON *deps = cJSON_GetObjectItem(j, "departures");
    ok(cJSON_GetArraySize(deps) == 4, "4 boardable departures: the 'never' and expired services are excluded");
    double prev = -1;
    int sorted = 1;
    cJSON *d;
    cJSON_ArrayForEach(d, deps) {
      double su = cJSON_GetObjectItem(d, "seconds_until")->valuedouble;
      if (su < prev) sorted = 0;
      prev = su;
    }
    ok(sorted, "ordered soonest first");
    cJSON *d0 = cJSON_GetArrayItem(deps, 0);
    ok(!strcmp(cJSON_GetObjectItem(d0, "trip_id")->valuestring, "TE") && !strcmp(cJSON_GetObjectItem(d0, "headsign")->valuestring, "Soon") &&
           !strcmp(cJSON_GetObjectItem(d0, "route_name")->valuestring, "1") && !strcmp(cJSON_GetObjectItem(d0, "route_color")->valuestring, "#FF0000"),
       "first is TE: headsign, short route name, '#'-prefixed colour");
    cJSON *dB = NULL;
    cJSON_ArrayForEach(d, deps) if (!strcmp(cJSON_GetObjectItem(d, "trip_id")->valuestring, "TB")) dB = d;
    ok(dB && cJSON_IsNull(cJSON_GetObjectItem(dB, "headsign")) && !strcmp(cJSON_GetObjectItem(dB, "route_name")->valuestring, "Line Two") &&
           cJSON_IsNull(cJSON_GetObjectItem(dB, "route_color")), "no short name -> long name; no headsign/colour -> null");
    cJSON_Delete(j);
    j = call("/gtfs/stop/STOP/departures", "limit=2");
    ok(cJSON_GetArraySize(cJSON_GetObjectItem(j, "departures")) == 2, "limit=2 honoured");
    cJSON_Delete(j);
    j = call("/gtfs/stop/STOP/departures", "limit=1000");
    ok(cJSON_GetArraySize(cJSON_GetObjectItem(j, "departures")) == 4, "limit clamped to 20 (4 exist)");
    cJSON_Delete(j);
    /* an exception_type=2 row for today removes the daily service */
    char ymd[16];
    snprintf(ymd, sizeof ymd, "%04d%02d%02d", lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
    snprintf(sql, sizeof sql, "INSERT INTO gtfs_calendar_dates(org_id,feed_id,service_id,date,exception_type) VALUES('o','f','daily','%s',2),('o','f','never','%s',1)", ymd, ymd);
    ex(sql);
    j = call("/gtfs/stop/STOP/departures", "limit=20");
    deps = cJSON_GetObjectItem(j, "departures");
    ok(cJSON_GetArraySize(deps) == 1 && !strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(deps, 0), "trip_id")->valuestring, "TC"),
       "today's exception: 'daily' removed (type 2) and 'never' added (type 1) — only TC boards");
    cJSON_Delete(j);
    j = call("/gtfs/stop/STOP/departures", "t=2026-01-01T00:00:00.000Z");
    ok(!strcmp(cJSON_GetObjectItem(j, "now")->valuestring, "2026-01-01T00:00:00.000Z"), "an explicit t is echoed as now");
    cJSON_Delete(j);
  }

  printf("-- station summary / vehicle info\n");
  j = call("/station/nope/summary", NULL);
  ok(!strcmp(errs(j), "station not found"), "unknown station -> error body");
  cJSON_Delete(j);
  j = call("/station/0123456789abcdef0123456789abcdef01234567/summary", NULL);
  ok(!strcmp(errs(j), "cluster not found"), "a 40-hex lowercase uid is treated as a cluster id");
  cJSON_Delete(j);
  j = call("/station/0123456789ABCDEF0123456789ABCDEF01234567/summary", NULL);
  ok(!strcmp(errs(j), "station not found"), "uppercase hex is not a cluster id");
  cJSON_Delete(j);
  ex("DELETE FROM intel_items WHERE record_type='station'");
  ex("INSERT INTO intel_items(uid,source_id,sub_source_id,fetched_at,record_type,tenant_id,title,lat,lon,properties) VALUES"
     "('unified-stations|odpt:A_1','unified-stations','train','2026-01-01','station','legacy','Alpha',35.1,139.1,"
     "'{\"operator\":\"JR\",\"line\":\"Yamanote\",\"line_color\":\"#9acd32\",\"stop_id\":\"STOP\"}'),"
     "('unified-stations|zzz','unified-stations','subway','2026-01-01','station','legacy','Zulu',35.2,139.2,'{}')");
  j = call("/station/odpt:A_1/summary", NULL);
  cJSON *stn = cJSON_GetObjectItem(j, "station");
  ok(stn && !strcmp(cJSON_GetObjectItem(stn, "name")->valuestring, "Alpha") && !strcmp(cJSON_GetObjectItem(stn, "mode")->valuestring, "train"),
     "an existing station is found by its uid suffix");
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(j, "lines")) == 1 && cJSON_IsArray(cJSON_GetObjectItem(j, "departures")) &&
         cJSON_IsArray(cJSON_GetObjectItem(j, "arrivals")) && cJSON_IsArray(cJSON_GetObjectItem(j, "alerts")), "summary carries lines/departures/arrivals/alerts arrays");
  cJSON_Delete(j);
  j = call("/station/%25/summary", NULL);
  ok(!strcmp(errs(j), "station not found"), "'%' in the uid is not a LIKE wildcard: it does not match the first station");
  cJSON_Delete(j);
  j = call("/station/_/summary", NULL);
  ok(!strcmp(errs(j), "station not found"), "'_' is not a wildcard either");
  cJSON_Delete(j);
  j = call("/station/lpha/summary", NULL);
  ok(!strcmp(errs(j), "station not found"), "a proper suffix of the uid that is not the whole station uid after '|' is not a match");
  cJSON_Delete(j);

  j = call("/vehicle/justone/info", NULL);
  ok(!strncmp(errs(j), "bad tripId", 10), "vehicle id without separators -> error body");
  cJSON_Delete(j);
  j = call("/vehicle/o%7Cf/info", NULL);
  ok(!strncmp(errs(j), "bad tripId", 10), "two parts only -> error body");
  cJSON_Delete(j);
  j = call("/vehicle/o%7C%7Ct/info", NULL);
  ok(!strncmp(errs(j), "bad tripId", 10), "an empty middle part -> error body");
  cJSON_Delete(j);
  j = call("/vehicle/o%7Cf%7CNOSUCH/info", NULL);
  ok(!strcmp(errs(j), "trip not found"), "well-formed but unknown trip -> trip not found");
  cJSON_Delete(j);

  db_close(&db);
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
