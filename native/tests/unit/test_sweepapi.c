/* test_sweepapi.c — the map sweep layers: which ids are sweeps, what a cold
 * read serves, and that a bounded cold read says it is bounded.
 *
 *   - sweepapi_is_sweep: exactly the cameras / station / unified-* ids; NULL,
 *     empty, near-misses and prefixes are not;
 *   - a non-sweep id is NULL from both _data and _build (never an empty layer);
 *   - cold read (no prebuilt cache): bounded by JO_SWEEP_LIMIT, `capped` is true
 *     only when a row was actually left out — exactly LIMIT rows is NOT capped —
 *     and the meta says it is cold, not live;
 *   - a stored geometry wins over lat/lon; a missing or unparsable properties
 *     column becomes {} rather than dropping the feature;
 *   - a prebuilt collcache entry is served as-is, and _build ignores the cache;
 *   - unified-* layers rebuild from the store: stations as Points, lines as
 *     stitched LineStrings, counts in _meta, and an empty store is an honest
 *     empty collection with live=false. */
#include "../../core/sweepapi.c"

#include <assert.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", sql); exit(1); }
}
static cJSON *parse(char *s) { assert(s); cJSON *j = cJSON_Parse(s); free(s); assert(j); return j; }
static int nfeat(cJSON *fc) {
  return cJSON_GetArraySize(cJSON_GetObjectItem(fc, "features"));
}
static cJSON *meta(cJSON *fc) { return cJSON_GetObjectItem(fc, "_meta"); }

int main(void) {
  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  exec_ok(&db, "DELETE FROM intel_items");
  exec_ok(&db, "DELETE FROM collector_cache");

  /* ── ids ─────────────────────────────────────────────────────────────── */
  const char *yes[] = { "cameras", "unified-station-footprints", "unified-stations",
    "unified-trains", "unified-subways", "unified-buses", "unified-airports",
    "unified-ais-ships", "unified-port-infra", "unified-flights", "unified-highway", NULL };
  for (int i = 0; yes[i]; i++) assert(sweepapi_is_sweep(yes[i]) && yes[i]);
  const char *no[] = { NULL, "", "camera", "Cameras", "unified-", "unified-train",
    "unified-trains ", "unified-trainsx", "unified-ferries", "stations", "x", NULL };
  for (int i = 0; i < 10; i++) assert(!sweepapi_is_sweep(no[i]));
  assert(sweepapi_data(&db, "not-a-sweep") == NULL);
  assert(sweepapi_build(&db, "not-a-sweep") == NULL);
  assert(sweepapi_data(&db, NULL) == NULL && sweepapi_build(&db, "") == NULL);
  printf("  sweep ids are exact; non-sweeps are NULL: ok\n");

  /* ── cold read ───────────────────────────────────────────────────────── */
  setenv("JO_SWEEP_LIMIT", "3", 1);
  exec_ok(&db, "INSERT INTO intel_items(uid,source_id,fetched_at,lat,lon,properties,geometry) VALUES"
    "('c1','unified-flights','2026-01-01',35.0,139.0,'{\"callsign\":\"JAL1\"}',NULL),"
    "('c2','unified-flights','2026-01-01',36.0,140.0,'',NULL),"
    "('c3','unified-flights','2026-01-01',37.0,141.0,'not json',"
        "'{\"type\":\"Point\",\"coordinates\":[9.5,8.5]}'),"
    "('c4','unified-flights','2026-01-01',NULL,NULL,'{}',NULL)");   /* no position */
  /* unified-flights is a "unified mode", but sweepapi_data reads it cold: */
  cJSON *fc = parse(sweepapi_data(&db, "unified-flights"));
  assert(!strcmp(cJSON_GetObjectItem(fc, "type")->valuestring, "FeatureCollection"));
  assert(nfeat(fc) == 3 && "the row with no position is not a feature");
  assert(cJSON_IsFalse(cJSON_GetObjectItem(meta(fc), "capped")) &&
         "exactly LIMIT rows is not capped: nothing was left out");
  assert(!strcmp(cJSON_GetObjectItem(meta(fc), "cache_status")->valuestring, "cold"));
  assert(cJSON_IsFalse(cJSON_GetObjectItem(meta(fc), "live")));
  assert(cJSON_GetObjectItem(meta(fc), "recordCount")->valueint == 3);
  /* a stored geometry wins; unparsable/empty properties become {} */
  int saw_geom = 0, saw_props = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, cJSON_GetObjectItem(fc, "features")) {
    cJSON *c = cJSON_GetObjectItem(cJSON_GetObjectItem(f, "geometry"), "coordinates");
    if (cJSON_GetArrayItem(c, 0)->valuedouble == 9.5) saw_geom = 1;
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    assert(cJSON_IsObject(p));
    if (cJSON_GetObjectItem(p, "callsign")) saw_props = 1;
  }
  assert(saw_geom && saw_props);
  cJSON_Delete(fc);

  exec_ok(&db, "INSERT INTO intel_items(uid,source_id,fetched_at,lat,lon) VALUES"
               "('c5','unified-flights','2026-01-01',38.0,142.0)");
  fc = parse(sweepapi_data(&db, "unified-flights"));
  assert(nfeat(fc) == 3 && cJSON_IsTrue(cJSON_GetObjectItem(meta(fc), "capped")) &&
         "a row beyond the limit is left out AND says so");
  cJSON_Delete(fc);
  setenv("JO_SWEEP_LIMIT", "10", 1);
  fc = parse(sweepapi_data(&db, "unified-flights"));
  assert(nfeat(fc) == 4 && cJSON_IsFalse(cJSON_GetObjectItem(meta(fc), "capped")));
  cJSON_Delete(fc);
  setenv("JO_SWEEP_LIMIT", "0", 1);                    /* out of range -> default */
  fc = parse(sweepapi_data(&db, "unified-flights"));
  assert(nfeat(fc) == 4);
  cJSON_Delete(fc);
  printf("  cold read: bounded, capped only when rows were left out: ok\n");

  /* ── a prebuilt cache is served as-is; _build never reads it ─────────── */
  collcache_set(&db, "unified-flights",
                "{\"type\":\"FeatureCollection\",\"features\":[],\"_meta\":{\"prebuilt\":true}}",
                600000);
  fc = parse(sweepapi_data(&db, "unified-flights"));
  assert(nfeat(fc) == 0 && cJSON_IsTrue(cJSON_GetObjectItem(meta(fc), "prebuilt")));
  cJSON_Delete(fc);
  fc = parse(sweepapi_build(&db, "unified-flights"));
  assert(!cJSON_GetObjectItem(meta(fc), "prebuilt") && "build rebuilds from the store");
  cJSON_Delete(fc);
  printf("  prebuilt cache served; build bypasses it: ok\n");

  /* ── unified rebuild: stations + lines, honest empty ─────────────────── */
  fc = parse(sweepapi_build(&db, "unified-trains"));
  assert(nfeat(fc) == 0 && cJSON_IsFalse(cJSON_GetObjectItem(meta(fc), "live")) &&
         cJSON_GetObjectItem(meta(fc), "recordCount")->valueint == 0);
  cJSON_Delete(fc);
  exec_ok(&db, "INSERT INTO intel_items(uid,source_id,fetched_at,lat,lon,properties) VALUES"
    "('t1','unified-trains','2026-01-01',35.68,139.76,'{\"name\":\"Tokyo\"}'),"
    "('t2','unified-trains','2026-01-01',35.17,136.88,'{\"name\":\"Nagoya\"}')");
  exec_ok(&db, "INSERT INTO intel_items(uid,source_id,fetched_at,geometry,properties) VALUES"
    "('tl1','unified-trains','2026-01-01',"
    "'{\"type\":\"LineString\",\"coordinates\":[[139.76,35.68],[138.0,35.4],[136.88,35.17]]}',"
    "'{\"name\":\"Tokaido\"}')");
  fc = parse(sweepapi_build(&db, "unified-trains"));
  cJSON *m = meta(fc);
  assert(cJSON_GetObjectItem(m, "db_stations")->valueint == 2);
  assert(cJSON_GetObjectItem(m, "db_lines")->valueint >= 1);
  assert(cJSON_IsTrue(cJSON_GetObjectItem(m, "live")));
  assert(!strcmp(cJSON_GetObjectItem(m, "source")->valuestring, "transport_store:trains"));
  assert(nfeat(fc) == cJSON_GetObjectItem(m, "recordCount")->valueint);
  int pts = 0, lines = 0;
  cJSON_ArrayForEach(f, cJSON_GetObjectItem(fc, "features")) {
    const char *t = cJSON_GetObjectItem(cJSON_GetObjectItem(f, "geometry"), "type")->valuestring;
    if (!strcmp(t, "Point")) pts++; else if (!strcmp(t, "LineString")) lines++;
  }
  assert(pts == 2 && lines >= 1);
  cJSON_Delete(fc);
  /* another unified source's rows do not leak in */
  fc = parse(sweepapi_build(&db, "unified-subways"));
  assert(nfeat(fc) == 0);
  cJSON_Delete(fc);
  printf("  unified rebuild: points, stitched lines, counts, no leakage: ok\n");

  unsetenv("JO_SWEEP_LIMIT");
  db_close(&db);
  printf("test_sweepapi: all passed\n");
  return 0;
}
