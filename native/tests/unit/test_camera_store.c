/* tests/unit/test_camera_store.c — core/camera_store.c: camera_upsert's merge
 * rules (existing non-null wins, fresh geo_precision wins, channels are a
 * de-duplicated union, seen_count/first_seen accumulate), its refusal of
 * malformed Features, the FeatureCollection read (camera keyspace only, rows
 * without coordinates excluded, record count disclosed) and the keyset
 * discovery feed (paging, channel filter, malformed cursor, limit clamps).
 * JO_SINK_BATCH=1 makes emit() synchronous so return codes and read-after-write
 * are deterministic.  Reached through the header. */
#include "../../core/camera_store.h"
#include "../../core/intel.h"
#include "../../lib/camfeature.h"
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
static long scalar(const char *sql) {
  sqlite3_stmt *s;
  long v = -999;
  if (sqlite3_prepare_v2(db.h, sql, -1, &s, NULL) == SQLITE_OK && sqlite3_step(s) == SQLITE_ROW)
    v = (long)sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return v;
}
/* A sink de-duplicates a uid repeated within ONE run (the scheduler makes a new
 * sink per run), so every upsert here gets its own, as production does. */
static int up(cJSON *f, const char *ch) {
  intel_sink k = intel_sink_make(&db, "cam-unit", "legacy");
  int rc = camera_upsert(&db, &k, f, ch);
  intel_sink_free(&k);
  return rc;
}
static cJSON *pj(char *s) {
  assert(s);
  cJSON *j = cJSON_Parse(s);
  free(s);
  assert(j);
  return j;
}
static cJSON *props_of(const char *uid) {
  sqlite3_stmt *s;
  cJSON *p = NULL;
  assert(sqlite3_prepare_v2(db.h, "SELECT properties FROM intel_items WHERE uid=?1", -1, &s, NULL) == SQLITE_OK);
  sqlite3_bind_text(s, 1, uid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) p = cJSON_Parse((const char *)sqlite3_column_text(s, 0));
  sqlite3_finalize(s);
  return p;
}
static const char *S(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return cJSON_IsString(v) ? v->valuestring : NULL;
}

int main(void) {
  setvbuf(stdout, NULL, _IONBF, 0);
  setenv("JO_SINK_BATCH", "1", 1);
  assert(db_open(&db, NULL, NULL) == 0);
  char *e = NULL;
  assert(sqlite3_exec(db.h, "DELETE FROM intel_items", NULL, NULL, &e) == SQLITE_OK);

  printf("-- camera_fc_json on an empty store\n");
  cJSON *fc = pj(camera_fc_json(&db));
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(fc, "features")) == 0 &&
         cJSON_GetObjectItem(cJSON_GetObjectItem(fc, "_meta"), "recordCount")->valueint == 0,
     "empty FeatureCollection with recordCount 0");
  cJSON_Delete(fc);
  cJSON *feed = pj(camera_discovery_feed(&db, 10, NULL, NULL));
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(feed, "events")) == 0 && cJSON_IsNull(cJSON_GetObjectItem(feed, "cursor")),
     "empty discovery feed: no events, null cursor");
  cJSON_Delete(feed);

  printf("-- camera_upsert: refusals\n");
  ok(up(NULL, "ch") == -1, "NULL feature");
  cJSON *f = cJSON_Parse("{\"type\":\"Feature\",\"geometry\":{\"type\":\"Point\",\"coordinates\":[139,35]},\"properties\":{}}");
  ok(up(f, "ch") == -1, "no camera_uid"); cJSON_Delete(f);
  f = cJSON_Parse("{\"geometry\":{\"coordinates\":[139,35]},\"properties\":{\"camera_uid\":\"\"}}");
  ok(up(f, "ch") == -1, "empty camera_uid"); cJSON_Delete(f);
  f = cJSON_Parse("{\"properties\":{\"camera_uid\":\"u\"}}");
  ok(up(f, "ch") == -1, "no geometry"); cJSON_Delete(f);
  f = cJSON_Parse("{\"geometry\":{\"coordinates\":[139]},\"properties\":{\"camera_uid\":\"u\"}}");
  ok(up(f, "ch") == -1, "one coordinate"); cJSON_Delete(f);
  f = cJSON_Parse("{\"geometry\":{\"coordinates\":[\"139\",\"35\"]},\"properties\":{\"camera_uid\":\"u\"}}");
  ok(up(f, "ch") == -1, "string coordinates"); cJSON_Delete(f);
  f = cJSON_Parse("{\"geometry\":{\"coordinates\":[1e999,35]},\"properties\":{\"camera_uid\":\"u\"}}");
  ok(up(f, "ch") == -1, "infinite coordinate"); cJSON_Delete(f);
  ok(scalar("SELECT COUNT(*) FROM intel_items") == 0, "no refusal wrote a row");

  printf("-- camera_upsert: first write and merge\n");
  cam_kv x1[] = { { .k = "url", .sv = "https://cams.example/a" }, { .k = "operator", .sv = "Acme" },
                  { .k = "geo_precision", .sv = "city" }, { .k = "custom", .sv = "keep-me" } };
  f = cam_make_feature(35.6895, 139.6917, "Shibuya Crossing", "ptz", "chan-a", x1, 4);
  char uid[300];
  snprintf(uid, sizeof uid, "%s", cJSON_GetObjectItem(cJSON_GetObjectItem(f, "properties"), "camera_uid")->valuestring);
  char master[700];
  snprintf(master, sizeof master, "camera-discovery|%s", uid);
  ok(up(f, "chan-a") == 1, "new camera -> 1");
  cJSON *p = props_of(master);
  ok(p && !strcmp(S(p, "name"), "Shibuya Crossing") && !strcmp(S(p, "operator"), "Acme"), "stored under the camera-discovery| keyspace");
  ok(cJSON_GetObjectItem(p, "seen_count")->valueint == 1 && S(p, "first_seen_at") && S(p, "last_seen_at"), "seen_count=1, first/last seen stamped");
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(p, "discovery_channels")) == 1, "one channel");
  char first_seen[64];
  snprintf(first_seen, sizeof first_seen, "%s", S(p, "first_seen_at"));
  cJSON_Delete(p);
  ok(scalar("SELECT COUNT(*) FROM intel_items WHERE record_type='camera' AND link='https://cams.example/a' AND lat IS NOT NULL") == 1,
     "the camera page URL is on the item (link), geometry columns set");
  cJSON_Delete(f);

  /* second sighting from another channel with a worse name, a fresher precision */
  cam_kv x2[] = { { .k = "url", .sv = "https://cams.example/a" }, { .k = "geo_precision", .sv = "exact" },
                  { .k = "thumbnail_url", .sv = "https://cams.example/a.jpg" } };
  f = cam_make_feature(35.6895, 139.6917, "Different Name", "static", "chan-b", x2, 3);
  cJSON_ReplaceItemInObject(cJSON_GetObjectItem(f, "properties"), "camera_uid", cJSON_CreateString(uid));
  int rc2 = up(f, "chan-b");
  printf("   rc2=%d\n", rc2);
  ok(rc2 == 0, "same uid again -> 0 (updated)");
  cJSON_Delete(f);
  p = props_of(master);
  ok(!strcmp(S(p, "name"), "Shibuya Crossing") && !strcmp(S(p, "camera_type"), "ptz"), "existing non-empty name/type win over the new sighting's");
  ok(!strcmp(S(p, "geo_precision"), "exact"), "fresh geo_precision wins over the stored one (derived, not accumulated)");
  ok(!strcmp(S(p, "thumbnail_url"), "https://cams.example/a.jpg"), "a field the row lacked is filled from the new sighting");
  ok(!strcmp(S(p, "custom"), "keep-me"), "unrelated stored keys survive the merge");
  ok(cJSON_GetObjectItem(p, "seen_count")->valueint == 2 && !strcmp(S(p, "first_seen_at"), first_seen), "seen_count 2, first_seen unchanged");
  cJSON *chs = cJSON_GetObjectItem(p, "discovery_channels");
  ok(cJSON_GetArraySize(chs) == 2 && !strcmp(cJSON_GetArrayItem(chs, 0)->valuestring, "chan-a") &&
         !strcmp(cJSON_GetArrayItem(chs, 1)->valuestring, "chan-b"), "channels: ordered union");
  cJSON_Delete(p);
  f = cam_make_feature(35.6895, 139.6917, "Again", "x", "chan-a", NULL, 0);
  cJSON_ReplaceItemInObject(cJSON_GetObjectItem(f, "properties"), "camera_uid", cJSON_CreateString(uid));
  up(f, "chan-a");
  up(f, NULL);
  cJSON_Delete(f);
  p = props_of(master);
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(p, "discovery_channels")) == 2 && cJSON_GetObjectItem(p, "seen_count")->valueint == 4,
     "a repeated channel (and a NULL one) adds nothing; seen_count keeps counting");
  cJSON_Delete(p);
  ok(scalar("SELECT COUNT(*) FROM intel_items WHERE record_type='camera'") == 1, "still one row (identity is the camera uid)");

  printf("-- camera_fc_json\n");
  for (int i = 0; i < 4; i++) {
    char nm[40];
    snprintf(nm, sizeof nm, "Cam %d", i);
    cam_kv ex[] = { { .k = "url", .sv = nm } };
    char ch[16];
    snprintf(ch, sizeof ch, "chan-%c", i % 2 ? 'b' : 'a');
    f = cam_make_feature(35.0 + i, 139.0 + i, nm, "static", ch, ex, 1);
    assert(up(f, ch) == 1);
    cJSON_Delete(f);
  }
  assert(sqlite3_exec(db.h, "INSERT INTO intel_items(uid,source_id,fetched_at,record_type,tenant_id,lat,lon)"
                            " VALUES('camera-discovery|nogeo','cam-x','2026-01-01','camera','legacy',NULL,NULL),"
                            "('other|x','cam-x','2026-01-01','camera','legacy',1,2),"
                            "('camera-discovery|notcam','cam-x','2026-01-01','poi','legacy',1,2)", NULL, NULL, NULL) == SQLITE_OK);
  fc = pj(camera_fc_json(&db));
  cJSON *feats = cJSON_GetObjectItem(fc, "features");
  ok(cJSON_GetArraySize(feats) == 5 && cJSON_GetObjectItem(cJSON_GetObjectItem(fc, "_meta"), "recordCount")->valueint == 5,
     "5 cameras served; no-coordinate, foreign-keyspace and non-camera rows are not");
  cJSON *ft0 = cJSON_GetArrayItem(feats, 0);
  ok(!strcmp(S(ft0, "type"), "Feature") && cJSON_GetObjectItem(ft0, "geometry") && !strcmp(S(cJSON_GetObjectItem(ft0, "properties"), "record_type"), "camera"),
     "Feature shape with the row's own record_type overlaid");
  ok(!strcmp(S(cJSON_GetObjectItem(fc, "_meta"), "served_from"), "intel_items"), "meta states where it was served from");
  cJSON_Delete(fc);

  printf("-- camera_discovery_feed\n");
  char qs_cur[1200] = "";
  int pages = 0, total = 0, dup = 0;
  char seen[8][600];
  char *cursor = NULL;
  for (;;) {
    feed = pj(camera_discovery_feed(&db, 2, cursor, NULL));
    cJSON *evs = cJSON_GetObjectItem(feed, "events"), *ev;
    cJSON_ArrayForEach(ev, evs) {
      const char *cu = S(cJSON_GetObjectItem(cJSON_GetObjectItem(ev, "camera"), "properties"), "camera_uid");
      for (int i = 0; i < total; i++) if (!strcmp(seen[i], cu ? cu : "")) dup++;
      if (total < 8) snprintf(seen[total++], 600, "%s", cu ? cu : "");
      if (strcmp(S(ev, "kind"), "historical") || !S(ev, "ts") || !S(ev, "channel")) dup += 100;
    }
    cJSON *c = cJSON_GetObjectItem(feed, "cursor");
    free(cursor);
    cursor = cJSON_IsString(c) ? strdup(c->valuestring) : NULL;
    cJSON_Delete(feed);
    pages++;
    if (!cursor || pages > 6) break;
  }
  (void)qs_cur;
  ok(total == 5 && dup == 0 && pages == 3, "5 cameras over limit=2: three pages, each camera once, cursor ends null");
  feed = pj(camera_discovery_feed(&db, 100, NULL, "chan-b"));
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(feed, "events")) == 3, "channel filter matches discovery_channels membership (the merged camera + 2 b-cameras)");
  cJSON_Delete(feed);
  feed = pj(camera_discovery_feed(&db, 100, NULL, "no-such-channel"));
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(feed, "events")) == 0, "unknown channel: empty, not everything");
  cJSON_Delete(feed);
  feed = pj(camera_discovery_feed(&db, 2, "!!!not-base64!!!", NULL));
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(feed, "events")) == 2, "malformed cursor ignored: first page");
  cJSON_Delete(feed);
  feed = pj(camera_discovery_feed(&db, 2, "bm8tYmFyLWhlcmU=", NULL));   /* "no-bar-here" */
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(feed, "events")) == 2, "cursor without the ts|uid separator ignored");
  cJSON_Delete(feed);
  feed = pj(camera_discovery_feed(&db, -3, NULL, NULL));
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(feed, "events")) == 5, "non-positive limit -> default (500), all 5");
  cJSON_Delete(feed);
  feed = pj(camera_discovery_feed(&db, 1, NULL, "x' OR '1'='1"));
  ok(cJSON_GetArraySize(cJSON_GetObjectItem(feed, "events")) == 0, "channel is bound as a value (injection text matches nothing)");
  cJSON_Delete(feed);

  db_close(&db);
  printf(g_fail ? "FAILED: %d\n" : "all passed\n", g_fail);
  return g_fail ? 1 : 0;
}
