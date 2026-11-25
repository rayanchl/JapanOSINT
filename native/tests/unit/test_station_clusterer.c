/* tests/unit/test_station_clusterer.c — core/station_clusterer.c: the name
 * normaliser/fingerprint/Levenshtein it merges on, the union-find passes
 * (wikidata identity, spatial + name, trusted LLM merges), cluster
 * materialisation (stable uid, longest name, sorted unique sets), the
 * line-dot read, and snapping stations to the nearest coloured line.
 * Includes the .c so the static helpers are reachable; run.sh excludes
 * obj/core/station_clusterer.o from this test's link line. */
#include "../../core/station_clusterer.c"
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
static long scalar(const char *sql) {
  sqlite3_stmt *s;
  long v = -999;
  if (sqlite3_prepare_v2(db.h, sql, -1, &s, NULL) == SQLITE_OK && sqlite3_step(s) == SQLITE_ROW)
    v = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}
static void station(const char *src, const char *uid, const char *mode, const char *name, double lat, double lon, const char *props) {
  char q[900];
  snprintf(q, sizeof q,
           "INSERT INTO intel_items(uid,source_id,sub_source_id,fetched_at,record_type,tenant_id,title,lat,lon,properties) "
           "VALUES('%s|%s','%s','%s','2026-01-01','station','legacy','%s',%.6f,%.6f,'%s')",
           src, uid, src, mode, name, lat, lon, props);
  ex(q);
}
static const char *fp(const char *raw) {
  static char buf[8][256];
  static int k = 0;
  k = (k + 1) % 8;
  station_fingerprint(raw, buf[k], sizeof buf[k]);
  return buf[k];
}
static const char *nn(const char *raw) {
  static char buf[8][256];
  static int k = 0;
  k = (k + 1) % 8;
  norm_name(raw, buf[k], sizeof buf[k]);
  return buf[k];
}
static long cluster_of(const char *member_uid) {
  char q[400];
  snprintf(q, sizeof q, "SELECT COUNT(*) FROM station_clusters WHERE member_uids LIKE '%%\"%s\"%%'", member_uid);
  return scalar(q);
}
static int same_cluster(const char *a, const char *b) {
  char q[500];
  snprintf(q, sizeof q, "SELECT COUNT(*) FROM station_clusters WHERE member_uids LIKE '%%\"%s\"%%' AND member_uids LIKE '%%\"%s\"%%'", a, b);
  return scalar(q) == 1;
}

int main(void) {
  printf("-- name normalisation\n");
  ok(!strcmp(nn("Tokyo Station"), "tokyo"), "'Station' stripped, lower-cased, spaces removed");
  ok(!strcmp(nn("東京駅"), "東京"), "駅 stripped");
  ok(!strcmp(nn("新宿・西口 (Shinjuku)"), "新宿西口shinjuku"), "・ ( ) and spaces removed");
  ok(!strcmp(nn("A-B,C.D"), "abcd"), "- , . removed");
  ok(!strcmp(nn("市役所前停留所"), "市役所前"), "停留所 stripped");
  ok(!strcmp(nn("駅前バス停"), "前"), "バス停 stripped (and 駅)");
  ok(!strcmp(nn(""), "") && !strcmp(nn(NULL), ""), "empty / NULL -> empty");
  ok(!strcmp(nn("STATION"), ""), "a name that is only the token normalises to empty");
  ok(!strcmp(fp("東京駅前"), "東京"), "駅 then trailing 前 stripped");
  ok(!strcmp(fp("渋谷入口"), "渋谷"), "trailing 入口 stripped (one suffix)");
  ok(!strcmp(fp("渋谷口"), "渋谷"), "trailing 口 stripped");
  ok(!strcmp(fp("カタカナ"), "かたかな"), "katakana folded to hiragana");
  ok(!strcmp(fp("ＴＯＫＹＯ"), fp("ＴＯＫＹＯ")) && fp("ＴＯＫＹＯ")[0] != 0, "full-width input is accepted (no NFKC, documented)");

  printf("-- levenshtein / distance\n");
  ok(levenshtein_cp("abc", "abc") == 0 && levenshtein_cp("abc", "abd") == 1 && levenshtein_cp("abc", "") == 3 && levenshtein_cp("", "abc") == 3, "basic distances");
  ok(levenshtein_cp("kitten", "sitting") == 3, "kitten/sitting == 3");
  ok(levenshtein_cp("東京", "東亰") == 1 && levenshtein_cp("東京", "京東") == 2, "counted in codepoints, not bytes");
  ok(cp_len("東京abc") == 5, "cp_len");
  ok(dist_sq_m(139, 35, 139, 35) == 0, "same point 0");
  double d = sqrt(dist_sq_m(139.0, 35.0, 139.0, 35.001));
  ok(d > 110 && d < 112, "0.001 deg latitude is ~111 m");
  double dx = sqrt(dist_sq_m(139.0, 35.0, 139.001, 35.0));
  ok(dx > 88 && dx < 93, "0.001 deg longitude at 35N is ~91 m (cos-corrected)");

  printf("-- union-find\n");
  dsu_t ds;
  assert(dsu_init(&ds, 6) == 0);
  dsu_union(&ds, 0, 1); dsu_union(&ds, 2, 3); dsu_union(&ds, 1, 3);
  ok(dsu_find(&ds, 0) == dsu_find(&ds, 2) && dsu_find(&ds, 4) != dsu_find(&ds, 0) && dsu_find(&ds, 4) != dsu_find(&ds, 5), "union/find transitive; untouched stay apart");
  dsu_free(&ds);

  printf("-- clusterer: empty database\n");
  assert(db_open(&db, NULL, NULL) == 0);
  ex("DELETE FROM intel_items WHERE source_id LIKE 'unified-%'; DELETE FROM station_clusters; DELETE FROM station_line_dots; DELETE FROM llm_station_merges");
  ok(station_clusterer_run(&db) == 0, "no stations -> 0 clusters, not an error");
  char *js = station_clusterer_linedots_fc_json(&db);
  cJSON *fc = cJSON_Parse(js);
  free(js);
  ok(fc && cJSON_GetArraySize(cJSON_GetObjectItem(fc, "features")) == 0, "empty line-dot FeatureCollection");
  cJSON_Delete(fc);
  ok(station_snap_stations(&db, "tram") == -1 && station_snap_stations(&db, NULL) == -1, "snap: unknown/NULL mode -> -1");
  ok(station_snap_stations(&db, "train") == 0, "snap with no lines/stations -> 0 changed");

  printf("-- clusterer: merging rules\n");
  /* Tokyo: same fingerprint across modes, 50 m apart */
  station("unified-trains", "t-tokyo", "train", "東京駅", 35.6812, 139.7671, "{\"operator\":\"JR\",\"name_ja\":\"東京駅\"}");
  station("unified-subways", "s-tokyo", "subway", "東京", 35.68145, 139.7671, "{\"operator\":\"Metro\"}");
  /* Same name 5 km away: must stay separate */
  station("unified-trains", "t-far", "train", "東京", 35.7300, 139.7671, "{}");
  /* Levenshtein 1, length >= 3, 100 m apart */
  station("unified-trains", "t-shinjuku", "train", "Shinjuku", 35.6900, 139.7000, "{}");
  station("unified-subways", "s-shinjuko", "subway", "Shinjuko", 35.6905, 139.7005, "{}");
  /* two-char names differ by one: below the length-3 floor, not merged */
  station("unified-trains", "t-ab", "train", "Ab", 35.7000, 139.7000, "{}");
  station("unified-subways", "s-ac", "subway", "Ac", 35.7001, 139.7000, "{}");
  /* wikidata identity beats distance */
  station("unified-trains", "t-wd1", "train", "Alpha Central", 34.0, 135.0, "{\"wikidata\":\"Q42\"}");
  station("unified-trains", "t-wd2", "train", "Zeta Terminal", 36.0, 140.0, "{\"wikidata\":\"Q42\"}");
  /* LLM merges: 0.69 does not count, 0.7 does, same=0 never */
  station("unified-trains", "t-l1", "train", "Lunar One", 33.0, 131.0, "{}");
  station("unified-trains", "t-l2", "train", "Mars Two", 33.5, 131.5, "{}");
  station("unified-trains", "t-l3", "train", "Venus Three", 33.7, 131.7, "{}");
  station("unified-trains", "t-l4", "train", "Saturn Four", 33.9, 131.9, "{}");
  ex("INSERT INTO llm_station_merges(uid_a,uid_b,same,confidence) VALUES"
     "('t-l1','t-l2',1,0.69),('t-l1','t-l3',1,0.7),('t-l1','t-l4',0,0.99)");
  /* not a station source / no coordinates / non-point geometry: ignored */
  ex("INSERT INTO intel_items(uid,source_id,fetched_at,record_type,tenant_id,title,lat,lon) VALUES('other|x','other-src','2026-01-01','station','legacy','東京駅',35.6812,139.7671)");
  ex("INSERT INTO intel_items(uid,source_id,fetched_at,record_type,tenant_id,title) VALUES('unified-trains|nocoord','unified-trains','2026-01-01','station','legacy','Nowhere')");

  int n = station_clusterer_run(&db);
  ok(n == 9, "clusters: tokyo(2), tokyo-far, shinjuku(2), ab, ac, wikidata(2), l1+l3, l2, l4 = 9");
  ok(scalar("SELECT COUNT(*) FROM station_clusters") == n, "table holds exactly the clusters reported");
  ok(same_cluster("t-tokyo", "s-tokyo"), "same fingerprint across modes within 150 m merges (駅 stripped)");
  ok(!same_cluster("t-tokyo", "t-far"), "the same name 5 km away does not");
  ok(same_cluster("t-shinjuku", "s-shinjuko"), "Levenshtein <= 2 (len >= 3) within 150 m merges");
  ok(!same_cluster("t-ab", "s-ac"), "names under 3 code points need an exact fingerprint");
  ok(same_cluster("t-wd1", "t-wd2"), "wikidata identity merges regardless of distance and name");
  ok(same_cluster("t-l1", "t-l3") && !same_cluster("t-l1", "t-l2") && !same_cluster("t-l1", "t-l4"),
     "LLM merge trusted at confidence >= 0.7 and same=1 only");
  ok(cluster_of("nocoord") == 0 && cluster_of("x") == 0, "coordinate-less rows and foreign sources are not clustered");

  printf("-- cluster rows\n");
  sqlite3_stmt *s;
  assert(sqlite3_prepare_v2(db.h, "SELECT cluster_uid,name,name_ja,mode_set,operator_set,member_uids,lat,lon FROM station_clusters "
                                  "WHERE member_uids LIKE '%\"t-tokyo\"%'", -1, &s, NULL) == SQLITE_OK);
  ok(sqlite3_step(s) == SQLITE_ROW, "the tokyo cluster row exists");
  const char *cuid = (const char *)sqlite3_column_text(s, 0);
  ok(cuid && strlen(cuid) == 40, "cluster_uid is a 40-hex sha1");
  int hexok = 1;
  for (const char *p = cuid; p && *p; p++) if (!isxdigit((unsigned char)*p) || isupper((unsigned char)*p)) hexok = 0;
  ok(hexok, "lowercase hex (matches transitapi's is_cluster_uid)");
  ok(!strcmp((const char *)sqlite3_column_text(s, 1), "東京駅"), "name = the LONGEST member name");
  ok(!strcmp((const char *)sqlite3_column_text(s, 2), "東京駅"), "name_ja = first member name_ja");
  ok(!strcmp((const char *)sqlite3_column_text(s, 3), "[\"subway\",\"train\"]"), "mode_set sorted unique");
  ok(!strcmp((const char *)sqlite3_column_text(s, 4), "[\"JR\",\"Metro\"]"), "operator_set sorted unique, no nulls");
  ok(!strcmp((const char *)sqlite3_column_text(s, 5), "[\"s-tokyo\",\"t-tokyo\"]") ||
         strstr((const char *)sqlite3_column_text(s, 5), "s-tokyo") != NULL, "member_uids lists both");
  double clat = sqlite3_column_double(s, 6);
  ok(clat > 35.6812 && clat < 35.68145, "cluster position is between its members");
  char saved_uid[64];
  snprintf(saved_uid, sizeof saved_uid, "%s", cuid);
  sqlite3_finalize(s);

  printf("-- idempotence\n");
  ok(station_clusterer_run(&db) == n, "re-run: same cluster count (full rebuild, no accumulation)");
  char q[300];
  snprintf(q, sizeof q, "SELECT COUNT(*) FROM station_clusters WHERE cluster_uid='%s'", saved_uid);
  ok(scalar(q) == 1, "cluster_uid is stable across runs (hash of the sorted member uids)");
  ok(scalar("SELECT COUNT(*) FROM station_clusters") == n, "no duplicate rows");

  printf("-- snapping and line dots\n");
  ex("DELETE FROM station_clusters");
  /* a red and a blue line through the Tokyo stations' neighbourhood */
  ex("INSERT INTO intel_items(uid,source_id,fetched_at,record_type,tenant_id,geometry,properties) VALUES"
     "('unified-trains|lineR','unified-trains','2026-01-01','line','legacy',"
     "'{\"type\":\"LineString\",\"coordinates\":[[139.7600,35.6813],[139.7740,35.6813]]}','{\"line_color\":\"#ff0000\",\"line_uid\":\"R\"}'),"
     "('unified-trains|lineB','unified-trains','2026-01-01','line','legacy',"
     "'{\"type\":\"LineString\",\"coordinates\":[[139.7671,35.6700],[139.7671,35.6830]]}','{\"line_color\":\"#0000ff\",\"line_uid\":\"B\"}'),"
     "('unified-trains|lineN','unified-trains','2026-01-01','line','legacy',"
     "'{\"type\":\"LineString\",\"coordinates\":[[139.7600,35.6813],[139.7740,35.6813]]}','{\"line_uid\":\"nocolour\"}')");
  ex("UPDATE intel_items SET properties='{\"station_uid\":\"t-tokyo\"}' WHERE uid='unified-trains|t-tokyo'");
  ex("UPDATE intel_items SET properties='{\"station_uid\":\"t-far\"}' WHERE uid='unified-trains|t-far'");
  int changed = station_snap_stations(&db, "train");
  ok(changed >= 1, "stations near coloured lines get a colour written");
  assert(sqlite3_prepare_v2(db.h, "SELECT json_extract(properties,'$.line_color'), json_extract(properties,'$.line_colors') FROM intel_items WHERE uid='unified-trains|t-tokyo'", -1, &s, NULL) == SQLITE_OK);
  ok(sqlite3_step(s) == SQLITE_ROW && sqlite3_column_text(s, 0) != NULL, "t-tokyo has a line_color");
  const char *c1 = (const char *)sqlite3_column_text(s, 0);
  const char *cs = (const char *)sqlite3_column_text(s, 1);
  ok(c1 && !strcmp(c1, "#0000ff"), "line_color is the NEAREST line: blue passes through the station (0 m), red is ~11 m away");
  ok(cs && !strncmp(cs, "[\"#0000ff\",\"#ff0000\"", 20), "line_colors lists both lines in range, nearest first");
  sqlite3_finalize(s);
  assert(sqlite3_prepare_v2(db.h, "SELECT json_extract(properties,'$.line_color') FROM intel_items WHERE uid='unified-trains|t-far'", -1, &s, NULL) == SQLITE_OK);
  ok(sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 0) == SQLITE_NULL, "a station 5 km from every line is left uncoloured (no guess)");
  sqlite3_finalize(s);
  ok(station_snap_stations(&db, "train") == 0, "re-snap changes nothing (idempotent, no-op writes skipped)");

  ok(station_clusterer_run(&db) > 0, "clusterer after lines exist");
  js = station_clusterer_linedots_fc_json(&db);
  fc = cJSON_Parse(js);
  free(js);
  cJSON *feats = cJSON_GetObjectItem(fc, "features");
  ok(cJSON_GetArraySize(feats) >= 2, "one dot per (cluster, way): red and blue at the Tokyo cluster");
  cJSON *f0 = cJSON_GetArrayItem(feats, 0);
  ok(!strcmp(cJSON_GetObjectItem(cJSON_GetObjectItem(f0, "geometry"), "type")->valuestring, "Point") &&
         cJSON_GetObjectItem(cJSON_GetObjectItem(f0, "properties"), "line_color") != NULL, "dots are Points carrying their line colour");
  cJSON_Delete(fc);

  db_close(&db);
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
