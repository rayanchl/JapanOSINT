/* tests/unit/test_camera_stills_targets.c — the stills pod schedules capture
 * TARGETS resolved by core/camera_feed.c, across every camera keyspace.
 *
 * No network: every feed URL is on 127.0.0.x, which the camera destination
 * policy refuses (JO_CAMERA_ALLOW_LAN unset), so each capture fails CLOSED with
 * "blocked_destination" before a socket is opened — which is exactly the state
 * write this test reads back.
 *
 * WHAT IS BEING PROTECTED.
 *   1. Rows outside camera-discovery are captured: a trn_* station row, an
 *      hp-flattened row and a camera-discovery row all yield targets.
 *   2. A station's views[] are separate targets with their own state rows
 *      ("<uid>#view=views/<preset>"), so one view backing off does not hold
 *      the others back.
 *   3. A row with no feed is recorded as no_feed_url (and backed off), not
 *      skipped silently; an opted-OUT row is never targeted.
 *   4. The per-host per-tick ceiling defers, it does not drop: the deferred
 *      target has no state row and is still due next tick.
 *   5. The default batch sustains 100 targets at the default cadence.
 *   6. Listing a record returns every view's frames, each labelled with its
 *      view, plus per-target state in meta; listing a target returns its own.
 *   7. capture_one on a view that is no longer in the record says so.
 */
#include "../../core/camera_stills.c"

#include <assert.h>

static void exec_ok(db_handle *db, const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(db->h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "sql failed: %s\n  %s\n", e ? e : "?", sql); exit(1); }
}

static char *state_err(db_handle *db, const char *tid) {
  sqlite3_stmt *s; char *out = NULL;
  assert(sqlite3_prepare_v2(db->h,
    "SELECT last_error FROM camera_stills_state WHERE camera_id=?1", -1, &s, NULL) == SQLITE_OK);
  sqlite3_bind_text(s, 1, tid, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(s) == SQLITE_ROW) out = dupcol(s, 0);
  sqlite3_finalize(s);
  return out;
}

static int has_tid(const cs_targets *T, const char *tid) {
  for (int i = 0; i < T->n; i++) if (strcmp(T->v[i].tid, tid) == 0) return 1;
  return 0;
}

#define DT "digitraffic-weathercam-stations|C01502"

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  char evdir[] = "/tmp/jo-cs-targets-XXXXXX";
  assert(mkdtemp(evdir));
  setenv("JO_EVIDENCE_DIR", evdir, 1);
  unsetenv("JO_CAMERA_ALLOW_LAN");
  unsetenv("CAM_STILLS_BATCH");
  unsetenv("CAM_STILLS_HOST_PER_TICK");
  setenv("MEDIA_NO_TOOLS", "1", 1);

  db_handle db = {0};
  assert(db_open(&db, NULL, NULL) == 0);
  camera_stills_migrate(&db);

  exec_ok(&db,
    "INSERT INTO intel_items(uid,source_id,fetched_at,record_type,capture_stills,properties) VALUES"
    "('camera-discovery|cam1','camera-discovery',datetime('now'),'camera',1,"
    "  '{\"url\":\"http://127.0.0.1:9/page\",\"thumbnail_url\":\"http://127.0.0.1:9/t.jpg\","
    "    \"discovery_channel\":\"windy_api\"}'),"
    "('" DT "','digitraffic-weathercam-stations',datetime('now'),'traffic-camera',1,"
    "  '{\"station_id\":\"C01502\",\"views\":["
    "    {\"preset_id\":\"C0150201\",\"image_url\":\"http://127.0.0.2:9/C0150201.jpg\"},"
    "    {\"preset_id\":\"C0150202\",\"image_url\":\"http://127.0.0.2:9/C0150202.jpg\"}]}'),"
    "('JO29A_CAM_IOWA_DOT_TRAFFIC_CAMERAS|17','JO29A_CAM_IOWA_DOT_TRAFFIC_CAMERAS',datetime('now'),"
    "  'camera-location',1,"
    "  '{\"attributes.ImageURL\":\"http://127.0.0.3:9/DMTV07.jpg\","
    "    \"endpoint\":\"https://services.arcgis.com/q\"}'),"
    "('hp|nofeed','JO_X',datetime('now'),'camera',1,'{\"name\":\"no image here\"}'),"
    "('hp|optedout','JO_X',datetime('now'),'camera',0,'{\"image_url\":\"http://127.0.0.4:9/a.jpg\"}')");

  /* 5. Sizing: 100 targets / (900 s / 300 s) = 34 per tick. */
  assert(cfg_batch() == 48);
  assert(cfg_batch() * (cfg_interval() / 300) >= 100);

  /* 1-3. The due list spans every keyspace and every view. */
  cs_targets due;
  assert(collect_due(db.h, &due) == 5);
  assert(has_tid(&due, "camera-discovery|cam1"));
  assert(has_tid(&due, DT "#view=views/C0150201"));
  assert(has_tid(&due, DT "#view=views/C0150202"));
  assert(has_tid(&due, "JO29A_CAM_IOWA_DOT_TRAFFIC_CAMERAS|17"));
  assert(has_tid(&due, "hp|nofeed"));
  assert(!has_tid(&due, "hp|optedout"));
  targets_free(&due);

  assert(camera_stills_capture_batch(&db, NULL, 0, NULL) == 0);
  char *e;
  e = state_err(&db, "camera-discovery|cam1");
  assert(e && strcmp(e, "blocked_destination") == 0); free(e);
  e = state_err(&db, DT "#view=views/C0150202");
  assert(e && strcmp(e, "blocked_destination") == 0); free(e);
  e = state_err(&db, "JO29A_CAM_IOWA_DOT_TRAFFIC_CAMERAS|17");
  assert(e && strcmp(e, "blocked_destination") == 0); free(e);
  e = state_err(&db, "hp|nofeed");
  assert(e && strcmp(e, "no_feed_url") == 0); free(e);
  /* everything attempted is backed off now */
  assert(collect_due(db.h, &due) == 0);
  targets_free(&due);

  /* 4. Per-host ceiling: one attempt per host per tick. The station's second
   * view is deferred — no state row — and leads the next tick. */
  exec_ok(&db, "DELETE FROM camera_stills_state");
  setenv("CAM_STILLS_HOST_PER_TICK", "1", 1);
  camera_stills_capture_batch(&db, NULL, 0, NULL);
  assert(collect_due(db.h, &due) == 1);
  assert(strncmp(due.v[0].tid, DT "#view=", strlen(DT "#view=")) == 0);
  assert(due.v[0].next == NULL);
  targets_free(&due);
  unsetenv("CAM_STILLS_HOST_PER_TICK");

  /* 6. Listing. Two frames on view 1, one on view 2, one on another record. */
  exec_ok(&db,
    "INSERT INTO camera_stills(id,camera_id,captured_at,sha256,bytes) VALUES"
    "('s1','" DT "#view=views/C0150201','2026-10-10 10:00:00','aa',10),"
    "('s2','" DT "#view=views/C0150201','2026-10-10 10:15:00','bb',10),"
    "('s3','" DT "#view=views/C0150202','2026-10-10 10:05:00','cc',10),"
    "('s4','camera-discovery|cam1','2026-10-10 10:05:00','dd',10)");
  char *j = camera_stills_list(&db, DT, 50, NULL);
  assert(j);
  cJSON *r = cJSON_Parse(j);
  cJSON *data = cJSON_GetObjectItem(r, "data");
  assert(cJSON_GetArraySize(data) == 3);
  cJSON *first = cJSON_GetArrayItem(data, 0);
  assert(strcmp(cJSON_GetObjectItem(first, "id")->valuestring, "s2") == 0);
  assert(strcmp(cJSON_GetObjectItem(first, "view")->valuestring, "views/C0150201") == 0);
  cJSON *meta = cJSON_GetObjectItem(r, "meta");
  assert(cJSON_IsTrue(cJSON_GetObjectItem(meta, "capture_stills")));
  assert(cJSON_GetObjectItem(meta, "total")->valueint == 3);
  assert(cJSON_GetObjectItem(meta, "targets_total")->valueint == 2);
  cJSON *tg = cJSON_GetObjectItem(meta, "targets");
  assert(cJSON_GetArraySize(tg) == 2);
  assert(strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(tg, 0), "kind")->valuestring,
                "snapshot") == 0);
  cJSON_Delete(r); free(j);

  /* paging across views keeps the keyset order */
  j = camera_stills_list(&db, DT, 2, NULL);
  r = cJSON_Parse(j);
  const char *cur = cJSON_GetObjectItem(cJSON_GetObjectItem(r, "page"), "cursor")->valuestring;
  char *j2 = camera_stills_list(&db, DT, 2, cur);
  cJSON *r2 = cJSON_Parse(j2);
  data = cJSON_GetObjectItem(r2, "data");
  assert(cJSON_GetArraySize(data) == 1);
  assert(strcmp(cJSON_GetObjectItem(cJSON_GetArrayItem(data, 0), "id")->valuestring, "s1") == 0);
  cJSON_Delete(r2); free(j2); cJSON_Delete(r); free(j);

  /* one view only */
  j = camera_stills_list(&db, DT "#view=views/C0150202", 50, NULL);
  r = cJSON_Parse(j);
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(r, "data")) == 1);
  cJSON_Delete(r); free(j);

  /* the bare camera-discovery spelling still works */
  j = camera_stills_list(&db, "cam1", 50, NULL);
  r = cJSON_Parse(j);
  assert(cJSON_GetArraySize(cJSON_GetObjectItem(r, "data")) == 1);
  assert(cJSON_IsNull(cJSON_GetObjectItem(cJSON_GetArrayItem(cJSON_GetObjectItem(r, "data"), 0), "view")));
  cJSON_Delete(r); free(j);

  /* 7. capture_one */
  assert(camera_stills_capture_one(&db, NULL, DT "#view=views/C0150201") == 0);
  assert(camera_stills_capture_one(&db, NULL, DT "#view=views/GONE") == 0);
  e = state_err(&db, DT "#view=views/GONE");
  assert(e && strcmp(e, "view_not_in_record") == 0); free(e);
  assert(camera_stills_capture_one(&db, NULL, "hp|optedout") == 0);

  /* 8. Capabilities name the decoders and the coverage. A fake tesseract that
   * lists jpn is put first on PATH (the probe is once per process and has not
   * run yet); MEDIA_NO_TOOLS keeps the pod from ever invoking it. */
  {
    char bindir[] = "/tmp/jo-cs-bin-XXXXXX";
    assert(mkdtemp(bindir));
    char tp[256];
    snprintf(tp, sizeof tp, "%s/tesseract", bindir);
    FILE *f = fopen(tp, "w");
    assert(f);
    fputs("#!/bin/sh\necho 'List of available languages in \"/usr/share/tessdata/\" (3):'\n"
          "echo eng\necho jpn\necho osd\n", f);
    fclose(f);
    chmod(tp, 0755);
    const char *op = getenv("PATH");
    char np[4096];
    snprintf(np, sizeof np, "%s:%s", bindir, op ? op : "/usr/bin:/bin");
    setenv("PATH", np, 1);

    exec_ok(&db, "UPDATE camera_stills SET phash=12345 WHERE id='s1'");
    char *cj = camera_stills_capabilities(&db);
    assert(cj);
    cJSON *c = cJSON_Parse(cj);
    cJSON *d = cJSON_GetObjectItem(c, "data");
    cJSON *tz = cJSON_GetObjectItem(cJSON_GetObjectItem(d, "decoders"), "tesseract");
    assert(cJSON_IsTrue(cJSON_GetObjectItem(tz, "present")));
    assert(cJSON_IsTrue(cJSON_GetObjectItem(tz, "jpn")));
    assert(strcmp(cJSON_GetObjectItem(tz, "langs")->valuestring, "eng,jpn,osd") == 0);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(d, "decoders"),
                                            "media_tools_disabled")));
    cJSON *feat = cJSON_GetObjectItem(d, "features");
    assert(cJSON_GetObjectItem(feat, "phash") && cJSON_GetObjectItem(feat, "ocr") &&
           cJSON_GetObjectItem(feat, "rtsp_hls_stills"));
    cJSON *cov = cJSON_GetObjectItem(d, "coverage");
    assert(cJSON_GetObjectItem(cov, "stills")->valueint == 4);
    assert(strcmp(cJSON_GetObjectItem(cov, "phash_text")->valuestring, "pHash on 1 of 4") == 0);
    cJSON_Delete(c); free(cj);
    unlink(tp); rmdir(bindir);
  }

  db_close(&db);
  printf("test_camera_stills_targets: OK\n");
  return 0;
}
