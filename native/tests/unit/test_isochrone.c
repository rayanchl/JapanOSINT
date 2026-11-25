/* tests/unit/test_isochrone.c — core/isochrone.c: GTFS reachability, driven
 * through isochrone_run() against a three-stop fixture timetable.
 * Pins the request validation (400s name the field), the honest "no stops"
 * answer, parameter clamping reported in meta.params, band handling,
 * calendar semantics (weekday pattern, exception_type=2 removes, =1 adds),
 * the claim made about transit ("first reached by transit" vs walking-only
 * note), result caching with meta.cached flipping, and the origin-rounding
 * cache key.  Reached through the header. */
#include "../../core/isochrone.h"
#include "../../third_party/cJSON.h"
#include "../../third_party/sqlite3.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
typedef struct { int st; cJSON *j; } resp;
static resp run(const char *qs) {
  resp r = {0};
  char *s = isochrone_run(&db, qs, &r.st);
  assert(s);
  r.j = cJSON_Parse(s);
  free(s);
  assert(r.j);
  return r;
}
static const char *err(resp *r) {
  cJSON *e = cJSON_GetObjectItem(r->j, "error");
  return cJSON_IsString(e) ? e->valuestring : "";
}
static cJSON *meta(resp *r) { return cJSON_GetObjectItem(r->j, "meta"); }
static double mnum(resp *r, const char *k) {
  cJSON *v = cJSON_GetObjectItem(meta(r), k);
  return cJSON_IsNumber(v) ? v->valuedouble : -1;
}
static cJSON *par(resp *r) { return cJSON_GetObjectItem(meta(r), "params"); }

int main(void) {
  assert(db_open(&db, NULL, NULL) == 0);
  ex("DELETE FROM gtfs_stop_times; DELETE FROM gtfs_trips; DELETE FROM gtfs_calendar; "
     "DELETE FROM gtfs_calendar_dates; DELETE FROM gtfs_stops; DELETE FROM collector_cache");

  printf("-- validation\n");
  resp r = run("");
  ok(r.st == 400 && !strcmp(err(&r), "lat_required"), "no lat -> 400 lat_required");
  cJSON_Delete(r.j);
  r = run("lat=35");
  ok(r.st == 400 && !strcmp(err(&r), "lon_required"), "no lon -> 400 lon_required");
  cJSON_Delete(r.j);
  r = run("lat=abc&lon=139");
  ok(r.st == 400 && !strcmp(err(&r), "lat_invalid"), "non-numeric lat -> lat_invalid");
  cJSON_Delete(r.j);
  r = run("lat=91&lon=139");
  ok(r.st == 400 && !strcmp(err(&r), "lat_invalid"), "lat 91 -> lat_invalid");
  cJSON_Delete(r.j);
  r = run("lat=35&lon=181");
  ok(r.st == 400 && !strcmp(err(&r), "lon_invalid"), "lon 181 -> lon_invalid");
  cJSON_Delete(r.j);
  r = run("lat=nan&lon=139");
  ok(r.st == 400, "lat=nan refused (NaN fails the range test)");
  cJSON_Delete(r.j);
  r = run("lat=35&lon=139&depart_at=tomorrow");
  ok(r.st == 400 && !strcmp(err(&r), "depart_at_invalid"), "unparseable depart_at -> 400");
  cJSON_Delete(r.j);
  r = run("lat=%33%35&lon=139&depart_at=2026-11-23T09%3A55%3A00");
  ok(r.st == 200, "percent-encoded values decode (lat=35, depart_at with %3A)");
  cJSON_Delete(r.j);

  printf("-- no timetable in range\n");
  r = run("lat=35&lon=139&depart_at=2026-11-23T09:55:00");
  ok(r.st == 200 && cJSON_GetArraySize(cJSON_GetObjectItem(r.j, "features")) == 0 && mnum(&r, "stops_reached") == 0 &&
         strstr(cJSON_GetObjectItem(meta(&r), "note")->valuestring, "no gtfs_stops"),
     "empty gtfs_stops: 200 with an explicit note, never an unexplained empty shape");
  cJSON_Delete(r.j);

  /* fixture: S1 (35.000,139.000) -> S2 (35.000,139.100) -> S3 (35.000,139.200), 10 min apart */
  ex("INSERT INTO gtfs_stops(org_id,feed_id,stop_id,name,lat,lon) VALUES"
     "('o','f','S1','one',35.0,139.0),('o','f','S2','two',35.0,139.1),('o','f','S3','three',35.0,139.2)");
  ex("INSERT INTO gtfs_calendar(org_id,feed_id,service_id,mon,tue,wed,thu,fri,sat,sun,start_date,end_date) VALUES"
     "('o','f','wk',1,1,1,1,1,0,0,NULL,NULL),('o','f','hol',0,0,0,0,0,0,0,NULL,NULL)");
  ex("INSERT INTO gtfs_trips(org_id,feed_id,trip_id,route_id,service_id) VALUES('o','f','T1','R','wk'),('o','f','T2','R','hol')");
  ex("INSERT INTO gtfs_stop_times(org_id,feed_id,trip_id,stop_sequence,stop_id,arrival_sec,departure_sec) VALUES"
     "('o','f','T1',1,'S1',36000,36000),('o','f','T1',2,'S2',36600,36600),('o','f','T1',3,'S3',37200,37200),"
     "('o','f','T2',1,'S1',36000,36000),('o','f','T2',2,'S2',36600,36600),('o','f','T2',3,'S3',37200,37200)");

  printf("-- a weekday with service\n");
  /* 2026-11-23 is a Monday. Origin a few metres from S1; depart 09:55, bus at 10:00. */
  r = run("lat=35.0005&lon=139.0&depart_at=2026-11-23T09:55:00&max_min=30");
  ok(r.st == 200, "200");
  ok(!strcmp(cJSON_GetObjectItem(meta(&r), "weekday")->valuestring, "mon") && !strcmp(cJSON_GetObjectItem(meta(&r), "service_date")->valuestring, "20261123"),
     "service day derived from depart_at (Mon 20261123)");
  ok(mnum(&r, "stops_loaded") == 3 && mnum(&r, "stops_reached") >= 1, "3 stops loaded, the walkable one reached");
  ok(mnum(&r, "stops_first_reached_by_transit") == 2, "S2 and S3 are first reached by the 10:00 service (S1 by walking)");
  ok(cJSON_IsFalse(cJSON_GetObjectItem(meta(&r), "cached")) && cJSON_IsTrue(cJSON_GetObjectItem(meta(&r), "stops_complete")) &&
         cJSON_IsFalse(cJSON_GetObjectItem(meta(&r), "truncated")), "fresh, complete, not truncated");
  ok(cJSON_GetObjectItem(meta(&r), "note") == NULL, "no walking-only note when transit contributed");
  cJSON *feats = cJSON_GetObjectItem(r.j, "features");
  ok(cJSON_GetArraySize(feats) >= 1, "at least one band polygon");
  double prev = 1e9;
  int desc = 1, shapes = 1;
  cJSON *ft;
  cJSON_ArrayForEach(ft, feats) {
    double b = cJSON_GetObjectItem(cJSON_GetObjectItem(ft, "properties"), "band_min")->valuedouble;
    if (b >= prev) desc = 0;
    prev = b;
    if (strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(ft, "geometry"), "type")->valuestring, "MultiPolygon")) shapes = 0;
  }
  ok(desc && shapes, "bands are MultiPolygons, largest first");
  cJSON *b3 = cJSON_GetObjectItem(par(&r), "bands");
  ok(cJSON_GetArraySize(b3) == 3 && cJSON_GetArrayItem(b3, 2)->valueint == 30, "default bands end at max_min");
  int s_hi = 0;
  cJSON_ArrayForEach(ft, feats) {
    cJSON *pr = cJSON_GetObjectItem(ft, "properties");
    if (cJSON_GetObjectItem(pr, "band_min")->valueint == 30) s_hi = cJSON_GetObjectItem(pr, "stops_within")->valueint;
  }
  ok(s_hi == 3, "the 30-minute band counts all 3 stops within it");
  cJSON_Delete(r.j);

  printf("-- cache\n");
  r = run("lat=35.0005&lon=139.0&depart_at=2026-11-23T09:55:00&max_min=30");
  ok(cJSON_IsTrue(cJSON_GetObjectItem(meta(&r), "cached")) && mnum(&r, "stops_first_reached_by_transit") == 2, "second identical request served from cache with cached=true");
  cJSON_Delete(r.j);
  r = run("lat=35.00051&lon=139.00001&depart_at=2026-11-23T09:56:00&max_min=30");
  ok(cJSON_IsTrue(cJSON_GetObjectItem(meta(&r), "cached")), "origin rounded to 4 dp and departure to a 15-minute bucket share a slot");
  cJSON_Delete(r.j);
  r = run("lat=35.0005&lon=139.0&depart_at=2026-11-23T09:55:00&max_min=31");
  ok(cJSON_IsFalse(cJSON_GetObjectItem(meta(&r), "cached")), "a different parameter is a different answer (no false hit)");
  cJSON_Delete(r.j);

  printf("-- calendar exceptions\n");
  /* Saturday: weekly pattern says no service; an ADD exception turns T2 on. */
  r = run("lat=35.0005&lon=139.0&depart_at=2026-11-28T09:55:00&max_min=30");
  ok(mnum(&r, "stops_first_reached_by_transit") == 0 && cJSON_GetObjectItem(meta(&r), "note") != NULL,
     "Saturday, no service: transit contributes nothing and the meta says the polygon is walking reach only");
  cJSON_Delete(r.j);
  ex("DELETE FROM collector_cache");
  ex("INSERT INTO gtfs_calendar_dates(org_id,feed_id,service_id,date,exception_type) VALUES('o','f','hol','20261128',1)");
  r = run("lat=35.0005&lon=139.0&depart_at=2026-11-28T09:55:00&max_min=30");
  ok(mnum(&r, "stops_first_reached_by_transit") == 2, "exception_type=1 adds the 'hol' service on that Saturday");
  cJSON_Delete(r.j);
  ex("INSERT INTO gtfs_calendar_dates(org_id,feed_id,service_id,date,exception_type) VALUES('o','f','wk','20261124',2)");
  r = run("lat=35.0005&lon=139.0&depart_at=2026-11-24T09:55:00&max_min=30");
  ok(mnum(&r, "stops_first_reached_by_transit") == 0, "exception_type=2 removes the weekday service on a holiday Tuesday");
  cJSON_Delete(r.j);
  r = run("lat=35.0005&lon=139.0&depart_at=2026-11-25T09:55:00&max_min=30");
  ok(mnum(&r, "stops_first_reached_by_transit") == 2, "the day after is back to the ordinary pattern");
  cJSON_Delete(r.j);

  printf("-- time-of-day\n");
  r = run("lat=35.0005&lon=139.0&depart_at=2026-11-25T11:00:00&max_min=30");
  ok(mnum(&r, "stops_first_reached_by_transit") == 0, "departing after the last service: walking reach only");
  cJSON_Delete(r.j);
  r = run("lat=35.0005&lon=139.0&depart_at=2026-11-25T00:55:00Z&max_min=30");
  ok(!strcmp(cJSON_GetObjectItem(meta(&r), "depart_at")->valuestring, "2026-11-25T09:55:00+09:00") &&
         mnum(&r, "stops_first_reached_by_transit") == 2, "a 'Z' timestamp is normalised to JST (00:55Z == 09:55 JST)");
  cJSON_Delete(r.j);

  printf("-- clamping is reported, not silent\n");
  r = run("lat=35.0005&lon=139.0&depart_at=2026-11-25T09:55:00&max_min=500&max_walk_m=99999&walk_kmh=99&max_transfers=99&bands=5,10,999,abc,-4");
  ok(cJSON_GetObjectItem(par(&r), "max_min")->valueint == 90, "max_min clamped to 90");
  ok(cJSON_GetObjectItem(par(&r), "max_walk_m")->valuedouble == 2000, "max_walk_m clamped to 2000");
  ok(cJSON_GetObjectItem(par(&r), "walk_kmh")->valuedouble == 7, "walk_kmh clamped to 7");
  ok(cJSON_GetObjectItem(par(&r), "max_transfers")->valueint == 4, "max_transfers clamped to 4");
  cJSON *bb = cJSON_GetObjectItem(par(&r), "bands");
  ok(cJSON_GetArraySize(bb) == 2 && cJSON_GetArrayItem(bb, 0)->valueint == 5 && cJSON_GetArrayItem(bb, 1)->valueint == 10,
     "bands: 999 (> max_min), 'abc' and -4 dropped; 5,10 kept");
  cJSON_Delete(r.j);
  r = run("lat=35.0005&lon=139.0&depart_at=2026-11-25T09:55:00&max_min=1&max_walk_m=1&walk_kmh=0&max_transfers=-3&bands=20,10,10,junk");
  ok(cJSON_GetObjectItem(par(&r), "max_min")->valueint == 5, "max_min floor 5");
  ok(cJSON_GetObjectItem(par(&r), "max_walk_m")->valuedouble == 100, "max_walk_m floor 100");
  ok(cJSON_GetObjectItem(par(&r), "walk_kmh")->valuedouble == 2, "walk_kmh floor 2");
  ok(cJSON_GetObjectItem(par(&r), "max_transfers")->valueint == 0, "max_transfers floor 0");
  bb = cJSON_GetObjectItem(par(&r), "bands");
  ok(cJSON_GetArraySize(bb) >= 1 && cJSON_GetArrayItem(bb, 0)->valueint <= 5, "bands larger than max_min are dropped, the rest sorted ascending");
  cJSON_Delete(r.j);
  r = run("lat=35.0005&lon=139.0&depart_at=2026-11-25T09:55:00&max_min=40&bands=30,10,20");
  bb = cJSON_GetObjectItem(par(&r), "bands");
  ok(cJSON_GetArraySize(bb) == 3 && cJSON_GetArrayItem(bb, 0)->valueint == 10 && cJSON_GetArrayItem(bb, 1)->valueint == 20 &&
         cJSON_GetArrayItem(bb, 2)->valueint == 30, "explicit bands sorted ascending");
  cJSON_Delete(r.j);

  printf("-- far from any stop\n");
  r = run("lat=43.0&lon=141.3&depart_at=2026-11-25T09:55:00");
  ok(r.st == 200 && mnum(&r, "stops_reached") == 0 && cJSON_GetObjectItem(meta(&r), "note") != NULL, "origin in Sapporo, stops in Kanto: explicit empty answer");
  cJSON_Delete(r.j);

  db_close(&db);
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
