/* test_image_vec.c — the camera image index (core/image_vec.c) against the
 * real vision sidecar in stub mode (vision_stub.h).
 *
 * WHAT IS BEING PROTECTED.
 *   1. inert: with JO_VISION_EMBED_URL unset nothing is created or sent;
 *   2. only cameras passing camera_record_indexable() are embedded — an
 *      insecam frame with perfectly good bytes is never sent;
 *   3. keyed by content: byte-identical frames are embedded ONCE and each
 *      still gets its own ref; a later identical frame costs no request;
 *   4. a frame the sidecar will not embed is recorded as failed (by sha) and
 *      not retried; a frame with no preserved bytes is counted, not guessed;
 *   5. a media asset of a camera record is indexed under kind "media";
 *   6. stills retention removes the pruned frame's ref, and the vector only
 *      once no ref is left;
 *   7. image vectors never land in a text index;
 *   8. a sidecar whose dimension changed is refused, not mixed;
 *   9. GET /api/cameras/visual-search: one result per camera with its newest
 *      matching still, bbox/since/k honoured, the privacy policy applied at
 *      QUERY time, "N of M cameras indexed" in-band, and a 503 that says why
 *      whenever the sidecar or the index cannot answer.
 *
 * Includes image_vec.c directly, so run.sh links every object except
 * obj/core/image_vec.o. */
#include "../../core/image_vec.c"
#include "core/visualsearchapi.h"
#include "vision_stub.h"

#include <assert.h>
#include <sys/stat.h>

static db_handle g_db;
static char g_evdir[256];

static long count(const char *sql) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(g_db.h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}

static void exec(const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(g_db.h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "SQL failed: %s\n  %s\n", e ? e : "?", sql);
    assert(0);
  }
}

static void hexsha(const unsigned char *b, size_t n, char out[65]) {
  unsigned char d[SHA256_DIGEST_LENGTH];
  SHA256(b, n, d);
  for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) snprintf(out + i * 2, 3, "%02x", d[i]);
}

/* A "frame": a JPEG signature (all the stub checks) plus a distinct tail. */
static size_t frame(unsigned char *buf, const char *tag, int jpeg) {
  size_t n = 0;
  if (jpeg) { buf[n++] = 0xFF; buf[n++] = 0xD8; buf[n++] = 0xFF; buf[n++] = 0xE0; }
  n += (size_t)snprintf((char *)buf + n, 200, "frame-%s-padding-padding", tag);
  return n;
}

/* Write the bytes into the evidence store the way evidence.c lays it out and
 * return the relative path. */
static void blob(const unsigned char *b, size_t n, char sha[65], char rel[160]) {
  hexsha(b, n, sha);
  char dir[512], path[700];
  snprintf(dir, sizeof dir, "%s/%.2s", g_evdir, sha);
  mkdir(dir, 0700);
  snprintf(rel, 160, "%.2s/%s", sha, sha);
  snprintf(path, sizeof path, "%s/%s", g_evdir, rel);
  FILE *f = fopen(path, "wb");
  assert(f);
  fwrite(b, 1, n, f);
  fclose(f);
}

static void camera(const char *uid, const char *channel, const char *title,
                   double lat, double lon) {
  char sql[1024];
  snprintf(sql, sizeof sql,
    "INSERT INTO intel_items(uid,source_id,title,fetched_at,properties,"
    " record_type,lat,lon) VALUES('%s','camera-discovery','%s',datetime('now'),"
    " '{\"discovery_channel\":\"%s\",\"snapshot_url\":\"https://cam.example/%s.jpg\"}',"
    " 'camera',%f,%f)", uid, title, channel, title, lat, lon);
  exec(sql);
}

/* One still; `rel` NULL = bytes never preserved. */
static void still(const char *id, const char *cam, const char *sha,
                  const char *rel, const char *at) {
  char sql[1024];
  snprintf(sql, sizeof sql,
    "INSERT INTO camera_stills(id,camera_id,captured_at,sha256,blob_path,bytes)"
    " VALUES('%s','%s','%s','%s',%s%s%s,30)", id, cam, at, sha,
    rel ? "'" : "", rel ? rel : "NULL", rel ? "'" : "");
  exec(sql);
}

/* visualsearchapi_route() → parsed JSON; *st gets the status. */
static cJSON *vsearch(const char *qs, int *st) {
  *st = 0;
  char *b = visualsearchapi_route(&g_db, "", "", qs, 0, st);
  assert(b);
  cJSON *j = cJSON_Parse(b);
  assert(j);
  free(b);
  return j;
}

static const char *jstr(cJSON *o, const char *path) {
  char buf[256];
  snprintf(buf, sizeof buf, "%s", path);
  cJSON *x = o;
  for (char *t = strtok(buf, "."); t && x; t = strtok(NULL, "."))
    x = cJSON_IsArray(x) ? cJSON_GetArrayItem(x, atoi(t)) : cJSON_GetObjectItem(x, t);
  return cJSON_IsString(x) ? x->valuestring : NULL;
}

static double jnum(cJSON *o, const char *path) {
  char buf[256];
  snprintf(buf, sizeof buf, "%s", path);
  cJSON *x = o;
  for (char *t = strtok(buf, "."); t && x; t = strtok(NULL, "."))
    x = cJSON_IsArray(x) ? cJSON_GetArrayItem(x, atoi(t)) : cJSON_GetObjectItem(x, t);
  return cJSON_IsNumber(x) ? x->valuedouble : -1;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  snprintf(g_evdir, sizeof g_evdir, "%s.evidence", dbp);
  mkdir(g_evdir, 0700);
  setenv("JO_EVIDENCE_DIR", g_evdir, 1);
  unsetenv("JO_VISION_EMBED_URL");
  unsetenv("JO_CAMERA_INDEX_CHANNELS");
  assert(db_open(&g_db, NULL, NULL) == 0);
  camera_stills_migrate(&g_db);

  camera("camera-discovery|river1", "mlit_river", "Tama River", 35.6, 139.6);
  camera("camera-discovery|open1", "insecam_scrape", "Open Cam", 35.7, 139.7);

  unsigned char b1[256], b2[256], b3[256], bx[256], bm[256];
  size_t n1 = frame(b1, "one", 1), n2 = frame(b2, "two", 1),
         n3 = frame(b3, "three", 1), nx = frame(bx, "html", 0),
         nm = frame(bm, "media", 1);
  char s1[65], s2[65], s3[65], sx[65], sm[65];
  char r1[160], r2[160], r3[160], rx[160], rm[160];
  blob(b1, n1, s1, r1); blob(b2, n2, s2, r2); blob(b3, n3, s3, r3);
  blob(bx, nx, sx, rx); blob(bm, nm, sm, rm);

  still("st1", "camera-discovery|river1", s1, r1, "2026-10-12 01:00:00");
  still("st2", "camera-discovery|river1", s2, r2, "2026-10-12 01:15:00");
  still("st3", "camera-discovery|river1", s1, r1, "2026-10-12 01:30:00");  /* = st1 */
  still("st4", "camera-discovery|river1", sx, rx, "2026-10-12 01:45:00");  /* not an image */
  still("st5", "camera-discovery|river1", s3, NULL, "2026-10-12 02:00:00"); /* no bytes */
  still("op1", "camera-discovery|open1", s3, r3, "2026-10-12 01:00:00");   /* denied */
  {
    char sql[1024];
    snprintf(sql, sizeof sql,
      "INSERT INTO media_assets(id,item_uid,url,sha256,bytes,content_type,analyzed_at)"
      " VALUES('ma1','camera-discovery|river1','https://cam.example/m.jpg','%s',%zu,"
      " 'image/jpeg','2026-10-12T00:30:00Z')", sm, nm);
    exec(sql);
    snprintf(sql, sizeof sql,
      "INSERT INTO evidence(id,item_uid,source_id,content_sha256,content_bytes,"
      " content_type,blob_path) VALUES('ev1','camera-discovery|river1','media','%s',"
      " %zu,'image/jpeg','%s')", sm, nm, rm);
    exec(sql);
  }

  /* 1. inert */
  assert(image_vec_tick(&g_db, NULL, 0) == 0);
  assert(!vecindex_exists(&g_db, &image_vec_spec));
  assert(!image_vec_ready(&g_db));
  {
    int st; cJSON *j = vsearch("q=snow", &st);
    assert(st == 503 && !strcmp(jstr(j, "error"), "visual_search_unavailable"));
    assert(strstr(jstr(j, "reason"), "JO_VISION_EMBED_URL"));
    cJSON_Delete(j);
  }
  printf("  inert: no sidecar, no table, nothing sent, search 503 says why: ok\n");

  if (vstub_start(18620, 64) != 0) {
    printf("  SKIP: vision sidecar stub did not start (python3?)\n");
    return 0;
  }

  {
    int st; cJSON *j = vsearch("q=snow", &st);
    assert(st == 503 && strstr(jstr(j, "reason"), "not been built"));
    cJSON_Delete(j);
  }

  /* 2-5. the first real tick */
  int e = image_vec_tick(&g_db, NULL, 0);
  assert(e == 3);                                  /* s1, s2 (stills), sm (media) */
  assert(image_vec_ready(&g_db));
  assert(count("SELECT count(*) FROM image_vec_done") == 3);   /* s1, s2, sm */
  {
    char q[256];
    snprintf(q, sizeof q, "SELECT count(*) FROM image_vec_done WHERE sha256='%s'", s3);
    assert(count(q) == 0);                         /* denied + no-bytes: never */
    snprintf(q, sizeof q, "SELECT count(*) FROM image_vec_failed WHERE sha256='%s'"
             " AND reason='not_an_image'", sx);
    assert(count(q) == 1);
  }
  assert(count("SELECT count(*) FROM image_vec_ref WHERE kind='still'") == 3);
  assert(count("SELECT count(*) FROM image_vec_ref WHERE kind='media'") == 1);
  assert(count("SELECT count(*) FROM image_vec_ref WHERE ref_id='op1'") == 0);
  assert(count("SELECT count(*) FROM image_vec_ref WHERE camera_uid='camera-discovery|open1'") == 0);
  assert(count("SELECT count(*) FROM image_vec WHERE kind='media'") == 1);
  assert(count("SELECT count(*) FROM image_vec WHERE kind='still'") == 2);
  {
    char model[64] = "";
    vecindex_meta_get(&g_db, &image_vec_spec, "model", model, sizeof model);
    assert(!strcmp(model, "stub-sha256-v1"));
    assert(vecindex_meta_long(&g_db, &image_vec_spec, "dim") == 64);
  }
  /* identical frames went over the wire once: st1 and st3 share one image */
  int posts = vstub_posts("/embed/image");
  assert(posts == 1);
  printf("  embed: 3 distinct frames, 4 refs, insecam never sent, not_an_image "
         "recorded, no-bytes counted, 1 request: ok\n");
  /* 9. visual search */
  {
    int st; cJSON *j = vsearch("q=snow%20on%20the%20road", &st);
    assert(st == 200);
    cJSON *d = cJSON_GetObjectItem(j, "data");
    assert(cJSON_GetArraySize(d) == 1);           /* river1 only: open1 denied */
    assert(!strcmp(jstr(j, "data.0.camera_id"), "camera-discovery|river1"));
    assert(!strcmp(jstr(j, "data.0.name"), "Tama River"));
    assert(jnum(j, "data.0.frames_matched") == 3);   /* st1, st2, st3 */
    const char *sid = jstr(j, "data.0.still.id");
    const char *sha = jstr(j, "data.0.still.sha256");
    /* the newest still of the nearest frame */
    assert((!strcmp(sha, s1) && !strcmp(sid, "st3")) ||
           (!strcmp(sha, s2) && !strcmp(sid, "st2")));
    assert(jnum(j, "data.0.distance") >= 0 && jnum(j, "data.0.lat") > 35);
    assert(jnum(j, "meta.cameras.indexed") == 1 && jnum(j, "meta.cameras.eligible") == 1);
    assert(jnum(j, "meta.cameras.excluded_by_policy") == 1);
    assert(!strcmp(jstr(j, "meta.cameras.note"), "1 of 1 cameras indexed"));
    assert(!strcmp(jstr(j, "meta.model"), "stub-sha256-v1"));
    assert(jnum(j, "meta.coverage.embedded_count") == 3);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(
      cJSON_GetObjectItem(j, "meta"), "candidates"), "exhausted")));
    cJSON_Delete(j);

    j = vsearch("q=snow&bbox=0,0,1,1", &st);
    assert(st == 200 && cJSON_GetArraySize(cJSON_GetObjectItem(j, "data")) == 0);
    cJSON_Delete(j);
    j = vsearch("q=snow&bbox=139,35,140,36", &st);
    assert(st == 200 && cJSON_GetArraySize(cJSON_GetObjectItem(j, "data")) == 1);
    cJSON_Delete(j);

    j = vsearch("q=snow&since=2026-10-12T01:20:00", &st);
    assert(st == 200 && cJSON_GetArraySize(cJSON_GetObjectItem(j, "data")) == 1);
    assert(!strcmp(jstr(j, "data.0.still.id"), "st3"));      /* st1/st2 too old */
    assert(jnum(j, "data.0.frames_matched") == 1);
    assert(!strcmp(jstr(j, "meta.since"), "2026-10-12 01:20:00"));
    cJSON_Delete(j);
    j = vsearch("q=snow&since=2027-01-01", &st);
    assert(st == 200 && cJSON_GetArraySize(cJSON_GetObjectItem(j, "data")) == 0);
    cJSON_Delete(j);

    /* the policy is read at QUERY time */
    setenv("JO_CAMERA_INDEX_CHANNELS", "volcano", 1);
    j = vsearch("q=snow", &st);
    assert(st == 200 && cJSON_GetArraySize(cJSON_GetObjectItem(j, "data")) == 0);
    assert(jnum(j, "meta.cameras.eligible") == 0);
    cJSON_Delete(j);
    unsetenv("JO_CAMERA_INDEX_CHANNELS");

    j = vsearch("bbox=0,0,1,1", &st); assert(st == 400); cJSON_Delete(j);
    j = vsearch("q=x&bbox=1,2,3", &st); assert(st == 400); cJSON_Delete(j);
    j = vsearch("q=x&bbox=10,50,20,40", &st); assert(st == 400); cJSON_Delete(j);
    j = vsearch("q=x&k=0", &st); assert(st == 400); cJSON_Delete(j);
    j = vsearch("q=x&since=yesterday-ish", &st); assert(st == 400); cJSON_Delete(j);
    assert(vstub_posts("/embed/text") == 6);  /* rejected requests sent nothing */
  }
  printf("  visual search: grouped by camera, newest still of the nearest frame, "
         "bbox/since honoured, policy at query time, 400s send nothing: ok\n");

  /* the image index is not a text index */
  assert(!vecindex_table_exists(&g_db, "intel_vec") ||
         count("SELECT count(*) FROM intel_vec") == 0);

  /* nothing new → nothing sent; a later identical frame is a ref, not a POST */
  assert(image_vec_tick(&g_db, NULL, 0) == 0);
  still("st6", "camera-discovery|river1", s2, r2, "2026-10-12 02:15:00");
  assert(image_vec_tick(&g_db, NULL, 0) == 0);
  assert(vstub_posts("/embed/image") == posts);
  assert(count("SELECT count(*) FROM image_vec_ref WHERE ref_id='st6'") == 1);
  {
    /* the vector's ts follows the NEWEST ref (st6) */
    char q[256];
    snprintf(q, sizeof q, "SELECT ts FROM image_vec WHERE sha256='%s'", s2);
    assert(count(q) == count("SELECT CAST(strftime('%s','2026-10-12 02:15:00') AS INTEGER)"));
  }
  printf("  re-tick: no request for unchanged or identical frames, ts follows the "
         "newest ref: ok\n");

  /* 6. retention: keep the newest 2 frames of river1 (st5 no-bytes, st6=s2) */
  setenv("CAM_STILLS_MAX_PER_CAM", "2", 1);
  setenv("CAM_STILLS_RETAIN_DAYS", "365", 1);
  exec("BEGIN");
  assert(camera_stills_prune_camera(&g_db, "camera-discovery|river1") == 4);
  exec("COMMIT");
  unsetenv("CAM_STILLS_MAX_PER_CAM");
  assert(count("SELECT count(*) FROM image_vec_ref WHERE kind='still'") == 1);
  {
    char q[256];
    snprintf(q, sizeof q, "SELECT count(*) FROM image_vec_done WHERE sha256='%s'", s1);
    assert(count(q) == 0);                         /* st1+st3 gone → vector gone */
    snprintf(q, sizeof q, "SELECT count(*) FROM image_vec WHERE sha256='%s'", s1);
    assert(count(q) == 0);
    snprintf(q, sizeof q, "SELECT count(*) FROM image_vec_done WHERE sha256='%s'", s2);
    assert(count(q) == 1);                         /* st6 still points at s2 */
  }
  /* gc: a media asset deleted by another route */
  exec("DELETE FROM media_assets WHERE id='ma1'");
  assert(image_vec_gc(&g_db, 0) == 1);
  assert(count("SELECT count(*) FROM image_vec_done") == 1);
  printf("  retention: pruned frames lose their ref, a vector goes when its last "
         "ref does, gc mops up media: ok\n");

  /* the query side of the sidecar */
  {
    float *qv = NULL; int qd = 0; char model[64], err[256];
    assert(image_vec_embed_text(NULL, "snow on the road", &qv, &qd, model,
                                sizeof model, err, sizeof err) == 0);
    assert(qd == 64 && !strcmp(model, "stub-sha256-v1"));
    free(qv);
  }

  /* 8. a sidecar with a different width is refused, not mixed */
  vstub_stop();
  assert(vstub_start(18640, 32) == 0);
  still("st7", "camera-discovery|river1", s3, r3, "2026-10-12 03:00:00");
  assert(image_vec_tick(&g_db, NULL, 0) < 0);
  assert(vecindex_refused(&g_db, &image_vec_spec, NULL, 0));
  assert(!image_vec_ready(&g_db));
  assert(count("SELECT count(*) FROM image_vec_ref WHERE ref_id='st7'") == 0);
  {
    int st; cJSON *j = vsearch("q=snow", &st);
    assert(st == 503 && strstr(jstr(j, "reason"), "refusing to mix"));
    cJSON_Delete(j);
  }
  printf("  identity: a 32-d sidecar against a 64-d index is refused, search 503: ok\n");

  vstub_stop();
  db_close(&g_db);
  printf("test_image_vec: all passed\n");
  return 0;
}
