/* tests/unit/test_station_footprints.c — core/station_footprints.c: upsert
 * (identity, bbox from the outer ring with non-finite vertices skipped,
 * first_seen preserved on update, skipped features not counted), cluster
 * linking (nearest candidate wins regardless of input order, re-link clears
 * stale links, malformed clusters skipped, an empty cluster list clears
 * everything) and the FeatureCollection read (mode_set joined from the
 * linked cluster, unparseable geometry dropped).  Reached through the
 * header. */
#include "../../core/station_footprints.h"
#include "../../third_party/sqlite3.h"
#include <assert.h>
#include <math.h>
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
static double num(const char *sql) {
  sqlite3_stmt *s;
  double v = -999;
  if (sqlite3_prepare_v2(db.h, sql, -1, &s, NULL) == SQLITE_OK && sqlite3_step(s) == SQLITE_ROW)
    v = sqlite3_column_double(s, 0);
  sqlite3_finalize(s);
  return v;
}
static char linkbuf[8][64];
static const char *link_of(const char *fid, int slot) {
  char q[200];
  sqlite3_stmt *s;
  snprintf(q, sizeof q, "SELECT cluster_uid FROM station_footprints WHERE footprint_id='%s'", fid);
  assert(sqlite3_prepare_v2(db.h, q, -1, &s, NULL) == SQLITE_OK);
  linkbuf[slot][0] = 0;
  const char *r = NULL;
  if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 0) != SQLITE_NULL) {
    snprintf(linkbuf[slot], 64, "%s", (const char *)sqlite3_column_text(s, 0));
    r = linkbuf[slot];
  }
  sqlite3_finalize(s);
  return r;
}
static cJSON *feat(const char *id_json, const char *ring, const char *name) {
  char b[900];
  snprintf(b, sizeof b,
           "{\"type\":\"Feature\",\"geometry\":{\"type\":\"Polygon\",\"coordinates\":[%s]},"
           "\"properties\":{\"footprint_id\":%s,\"name\":%s,\"name_ja\":\"駅\",\"source\":\"osm\"}}",
           ring, id_json, name);
  cJSON *j = cJSON_Parse(b);
  assert(j);
  return j;
}

int main(void) {
  assert(db_open(&db, NULL, NULL) == 0);
  ex("DELETE FROM station_footprints");
  ex("DELETE FROM station_clusters");

  printf("-- guards\n");
  ok(station_footprints_upsert_tx(NULL, NULL) == -1 && station_footprints_link_clusters_tx(NULL, NULL) == -1, "NULL db -> -1");
  ok(station_footprints_fc_json(NULL) == NULL && station_footprints_count(NULL) == -1, "NULL db: fc NULL, count -1");
  ok(station_footprints_upsert_tx(&db, NULL) == 0, "NULL features: empty run, 0");
  cJSON *notarr = cJSON_CreateObject();
  ok(station_footprints_upsert_tx(&db, notarr) == 0, "non-array features: empty run, 0");
  cJSON_Delete(notarr);
  ok(station_footprints_count(&db) == 0, "empty table counts 0");
  char *js = station_footprints_fc_json(&db);
  cJSON *fc = cJSON_Parse(js);
  free(js);
  ok(!strcmp(cJSON_GetObjectItem(fc, "type")->valuestring, "FeatureCollection") && cJSON_GetArraySize(cJSON_GetObjectItem(fc, "features")) == 0,
     "empty table: FeatureCollection with features []");
  cJSON_Delete(fc);

  printf("-- upsert\n");
  cJSON *arr = cJSON_CreateArray();
  cJSON_AddItemToArray(arr, feat("\"fp-a\"", "[[139.0,35.0],[139.2,35.0],[139.2,35.2],[139.0,35.2],[139.0,35.0]]", "\"Alpha\""));
  cJSON_AddItemToArray(arr, feat("\"fp-b\"", "[[139.1,35.1],[139.15,35.1],[139.15,35.15],[139.1,35.15],[139.1,35.1]]", "\"Beta\""));
  cJSON_AddItemToArray(arr, feat("\"\"", "[[0,0],[1,0],[1,1],[0,0]]", "\"empty id\""));
  cJSON_AddItemToArray(arr, feat("null", "[[0,0],[1,0],[1,1],[0,0]]", "\"null id\""));
  cJSON_AddItemToArray(arr, feat("\"fp-noring\"", "", "\"no ring\""));
  cJSON_AddItemToArray(arr, feat("\"fp-badpts\"", "[[\"x\",\"y\"],[null,1]]", "\"no finite vertex\""));
  cJSON_AddItemToArray(arr, feat("77", "[[140.0,36.0],[140.1,36.0],[140.1,36.1],[140.0,36.0]]", "null"));
  cJSON_AddItemToArray(arr, cJSON_CreateNull());
  cJSON_AddItemToArray(arr, cJSON_Parse("{\"type\":\"Feature\",\"properties\":{\"footprint_id\":\"fp-nogeom\"}}"));
  int n = station_footprints_upsert_tx(&db, arr);
  ok(n == 3, "3 written (fp-a, fp-b, numeric id 77); empty/null id, empty ring, no-finite-vertex, null entry, no geometry are skipped, not counted");
  ok(station_footprints_count(&db) == 3, "table holds exactly those 3");
  ok(num("SELECT bbox_min_lon FROM station_footprints WHERE footprint_id='fp-a'") == 139.0 &&
         num("SELECT bbox_max_lat FROM station_footprints WHERE footprint_id='fp-a'") == 35.2 &&
         num("SELECT bbox_min_lat FROM station_footprints WHERE footprint_id='fp-a'") == 35.0 &&
         num("SELECT bbox_max_lon FROM station_footprints WHERE footprint_id='fp-a'") == 139.2, "bbox from the outer ring");
  ok(num("SELECT COUNT(*) FROM station_footprints WHERE footprint_id='77' AND name IS NULL AND source='osm'") == 1, "numeric id stored as text, null name stored as NULL");
  cJSON_Delete(arr);

  /* mixed finite / non-finite vertices: the finite ones define the box */
  arr = cJSON_CreateArray();
  cJSON_AddItemToArray(arr, feat("\"fp-mixed\"", "[[10,10],[\"bad\",5],[12,11],[null,null]]", "\"Mixed\""));
  ok(station_footprints_upsert_tx(&db, arr) == 1, "ring with some bad vertices still written");
  ok(num("SELECT bbox_min_lon FROM station_footprints WHERE footprint_id='fp-mixed'") == 10 &&
         num("SELECT bbox_max_lon FROM station_footprints WHERE footprint_id='fp-mixed'") == 12 &&
         num("SELECT bbox_max_lat FROM station_footprints WHERE footprint_id='fp-mixed'") == 11, "bad vertices skipped in the box (not read as 0)");
  cJSON_Delete(arr);
  ex("DELETE FROM station_footprints WHERE footprint_id='fp-mixed'");

  /* update: first_seen preserved, geometry/name refreshed */
  ex("UPDATE station_footprints SET first_seen_at='2020-01-01 00:00:00', last_seen_at='2020-01-01 00:00:00' WHERE footprint_id='fp-b'");
  arr = cJSON_CreateArray();
  cJSON_AddItemToArray(arr, feat("\"fp-b\"", "[[139.1,35.1],[139.18,35.1],[139.18,35.18],[139.1,35.1]]", "\"Beta 2\""));
  ok(station_footprints_upsert_tx(&db, arr) == 1, "re-upsert counts as written");
  cJSON_Delete(arr);
  ok(station_footprints_count(&db) == 3, "no duplicate row");
  ok(num("SELECT COUNT(*) FROM station_footprints WHERE footprint_id='fp-b' AND name='Beta 2' AND first_seen_at='2020-01-01 00:00:00' AND last_seen_at<>'2020-01-01 00:00:00'") == 1,
     "first_seen kept, last_seen and name refreshed");
  ok(num("SELECT bbox_max_lon FROM station_footprints WHERE footprint_id='fp-b'") == 139.18, "bbox follows the new geometry");

  printf("-- link clusters\n");
  ok(station_footprints_link_clusters_tx(&db, NULL) == 0, "NULL clusters: clears links, 0 linked");
  /* fp-a box [139.0..139.2]x[35.0..35.2]; fp-b box [139.1..139.18]x[35.1..35.18] */
  ex("INSERT INTO station_clusters(cluster_uid,name,lat,lon,member_uids,mode_set) VALUES"
     "('cl-near-a',  'A', 35.02, 139.02, '[]', '[\"train\"]'),"
     "('cl-near-b',  'B', 35.14, 139.14, '[]', '[\"subway\",\"train\"]'),"
     "('cl-edge-a',  'C', 35.19, 139.19, '[]', '[\"bus\"]')");
  cJSON *cl = cJSON_Parse("[{\"cluster_uid\":\"cl-edge-a\",\"lat\":35.19,\"lon\":139.19},"
                          "{\"cluster_uid\":\"cl-near-b\",\"lat\":35.14,\"lon\":139.14},"
                          "{\"cluster_uid\":\"cl-near-a\",\"lat\":35.02,\"lon\":139.02},"
                          "{\"cluster_uid\":\"cl-bad\",\"lat\":\"x\",\"lon\":139.1},"
                          "{\"cluster_uid\":\"cl-nolat\",\"lon\":139.1},"
                          "{\"cluster_uid\":\"cl-outside\",\"lat\":10,\"lon\":10}]");
  int linked = station_footprints_link_clusters_tx(&db, cl);
  ok(linked == 2, "fp-a and fp-b linked (2); fp-77 contains no cluster; malformed and outside clusters ignored");
  ok(link_of("fp-a", 0) && !strcmp(link_of("fp-a", 0), "cl-near-b"),
     "fp-a's box holds cl-near-a, cl-near-b and cl-edge-a: the one NEAREST the footprint centre (139.1,35.1) wins, not the first or last in array order");
  ok(link_of("fp-b", 1) && !strcmp(link_of("fp-b", 1), "cl-near-b"), "fp-b linked to the cluster inside it");
  ok(link_of("77", 2) == NULL, "a footprint with no cluster inside stays unlinked");
  /* order independence */
  cJSON *rev = cJSON_Parse("[{\"cluster_uid\":\"cl-near-a\",\"lat\":35.02,\"lon\":139.02},"
                           "{\"cluster_uid\":\"cl-near-b\",\"lat\":35.14,\"lon\":139.14},"
                           "{\"cluster_uid\":\"cl-edge-a\",\"lat\":35.19,\"lon\":139.19}]");
  station_footprints_link_clusters_tx(&db, rev);
  ok(!strcmp(link_of("fp-a", 3), "cl-near-b") && !strcmp(link_of("fp-b", 4), "cl-near-b"), "the same winners in reverse input order (deterministic)");
  cJSON_Delete(rev);

  js = station_footprints_fc_json(&db);
  fc = cJSON_Parse(js);
  free(js);
  cJSON *feats = cJSON_GetObjectItem(fc, "features"), *it, *fa = NULL, *f77 = NULL;
  cJSON_ArrayForEach(it, feats) {
    cJSON *pr = cJSON_GetObjectItem(it, "properties");
    const char *id = cJSON_GetObjectItem(pr, "footprint_id")->valuestring;
    if (!strcmp(id, "fp-a")) fa = pr;
    if (!strcmp(id, "77")) f77 = pr;
  }
  ok(cJSON_GetArraySize(feats) == 3, "3 features served");
  ok(fa && cJSON_GetArraySize(cJSON_GetObjectItem(fa, "mode_set")) == 2 &&
         !strcmp(cJSON_GetArrayItem(cJSON_GetObjectItem(fa, "mode_set"), 0)->valuestring, "subway"), "mode_set joined from the linked cluster");
  ok(f77 && cJSON_IsNull(cJSON_GetObjectItem(f77, "cluster_uid")) && cJSON_IsArray(cJSON_GetObjectItem(f77, "mode_set")) &&
         cJSON_GetArraySize(cJSON_GetObjectItem(f77, "mode_set")) == 0 && cJSON_IsNull(cJSON_GetObjectItem(f77, "name")),
     "unlinked: cluster_uid null, mode_set [] (not absent), null name stays null");
  cJSON_Delete(fc);

  /* an unparseable stored geometry is dropped from the read, not served as null */
  ex("UPDATE station_footprints SET geometry='{not json' WHERE footprint_id='fp-b'");
  js = station_footprints_fc_json(&db);
  fc = cJSON_Parse(js);
  free(js);
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(fc, "features")) == 2, "a row whose geometry will not parse is omitted from the collection");
  cJSON_Delete(fc);

  /* relink with an empty list clears every link */
  cJSON *empty = cJSON_CreateArray();
  ok(station_footprints_link_clusters_tx(&db, empty) == 0 && link_of("fp-a", 5) == NULL, "an empty cluster list clears all links");
  cJSON_Delete(empty);
  cJSON_Delete(cl);

  db_close(&db);
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
