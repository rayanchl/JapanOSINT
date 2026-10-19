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
 *      whenever the sidecar or the index cannot answer;
 *  10. embedding drift: a target's first frame NULL, then 1-cos to the
 *      previous EMBEDDED frame and to the mean of the recent ones, written
 *      when the vector lands and exposed by the stills list beside (not in
 *      place of) scene_changed;
 *  11. POST /api/cameras/visual-search/by-image: a still queries with its
 *      STORED vector (nothing sent) and its own camera left out; an upload is
 *      embedded for the query only, never indexed, readable by its tenant
 *      only; a committed image upload becomes a tenant-scoped media row the
 *      media pod analyses from the evidence store and the index never walks;
 *  12. reference frames (core/camera_refframe.c): one frame per INDEXABLE
 *      camera record through the stills fetch path, failures recorded, at
 *      most weekly, inert without the sidecar, embedded as kind refframe,
 *      answering the search, and a refresh moving the ref to the new bytes.
 *
 * Includes image_vec.c directly, so run.sh links every object except
 * obj/core/image_vec.o. */
#include "../../core/image_vec.c"
#include "core/visualsearchapi.h"
#include "core/media.h"
#include "core/uploadapi.h"
#include "core/camera_refframe.h"
#include "vision_stub.h"

#include <assert.h>
#include <math.h>
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

/* camera_stills.<col> for one still; NAN when NULL. */
static double still_col(const char *id, const char *col) {
  char sql[256];
  snprintf(sql, sizeof sql, "SELECT %s FROM camera_stills WHERE id='%s'", col, id);
  sqlite3_stmt *s; double v = NAN;
  assert(sqlite3_prepare_v2(g_db.h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_type(s, 0) != SQLITE_NULL)
    v = sqlite3_column_double(s, 0);
  sqlite3_finalize(s);
  return v;
}

static double cos_of(const char *a, const char *b) {
  float *va = NULL, *vb = NULL; int da = 0, db = 0;
  assert(vecindex_load(&g_db, &image_vec_spec, a, &va, &da) == 0);
  assert(vecindex_load(&g_db, &image_vec_spec, b, &vb, &db) == 0 && da == db);
  double c = vec_cosine(va, vb, da);
  free(va); free(vb);
  return c;
}

/* cos(x, a+b): the direction of the mean of a and b */
static double cos_mean2(const char *x, const char *a, const char *b) {
  float *vx = NULL, *va = NULL, *vb = NULL; int d = 0;
  assert(vecindex_load(&g_db, &image_vec_spec, x, &vx, &d) == 0);
  assert(vecindex_load(&g_db, &image_vec_spec, a, &va, &d) == 0);
  assert(vecindex_load(&g_db, &image_vec_spec, b, &vb, &d) == 0);
  for (int i = 0; i < d; i++) va[i] += vb[i];
  double c = vec_cosine(vx, va, d);
  free(vx); free(va); free(vb);
  return c;
}

#define NEAR(a, b) (fabs((a) - (b)) < 1e-6)

/* A plain local web server for the reference-frame fetches. */
static pid_t web_pid = 0;

static int web_answers(int port) {
  char url[128];
  snprintf(url, sizeof url, "http://127.0.0.1:%d/", port);
  http_client *h = http_client_new();
  http_response r = {0};
  int ok = h && http_request(h, "GET", url, NULL, NULL, 0, 500, 0, &r) == 0 &&
           r.status == 200;
  http_response_free(&r);
  if (h) http_client_free(h);
  return ok;
}

static void web_stop(void) {
  if (web_pid > 0) { kill(web_pid, SIGKILL); waitpid(web_pid, NULL, 0); }
  web_pid = 0;
}

static int web_start(const char *dir, int port) {
  for (int tries = 0; tries < 40 && web_answers(port); tries++) port++;
  fflush(stdout);
  pid_t p = fork();
  if (p < 0) return -1;
  if (p == 0) {
    char ps[16];
    snprintf(ps, sizeof ps, "%d", port);
#ifdef __linux__
    prctl(PR_SET_PDEATHSIG, SIGKILL);   /* sys/prctl.h via vision_stub.h */
#endif
    if (!freopen("/dev/null", "w", stdout)) { /* harmless */ }
    if (!freopen("/dev/null", "w", stderr)) { /* harmless */ }
    execlp("python3", "python3", "-m", "http.server", ps, "--bind", "127.0.0.1",
           "--directory", dir, (char *)NULL);
    _exit(127);
  }
  web_pid = p;
  for (int i = 0; i < 100; i++) {
    if (web_answers(port)) return port;
    struct timespec ts = { 0, 50 * 1000000L };
    nanosleep(&ts, NULL);
  }
  web_stop();
  return -1;
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

  /* 10. drift, written as each vector landed (st1, st2, st3 in order) */
  {
    assert(isnan(still_col("st1", "embed_drift")));        /* first frame */
    assert(isnan(still_col("st1", "drift_from_mean")));
    double c12 = cos_of(s1, s2);
    assert(NEAR(still_col("st2", "embed_drift"), 1.0 - c12));
    assert(NEAR(still_col("st2", "drift_from_mean"), 1.0 - c12));  /* mean of one */
    assert(NEAR(still_col("st3", "embed_drift"), 1.0 - c12));      /* vs st2 */
    assert(NEAR(still_col("st3", "drift_from_mean"), 1.0 - cos_mean2(s1, s1, s2)));
    assert(still_col("st3", "drift_from_mean") < still_col("st3", "embed_drift"));
    assert(isnan(still_col("st4", "embed_drift")));        /* never embedded */

    char *js = camera_stills_list(&g_db, "camera-discovery|river1", 50, NULL);
    assert(js);
    cJSON *j = cJSON_Parse(js);
    free(js);
    assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "meta"),
                                            "embed_drift_available")));
    int seen = 0;
    cJSON *x;
    cJSON_ArrayForEach(x, cJSON_GetObjectItem(j, "data")) {
      const char *id = cJSON_GetObjectItem(x, "id")->valuestring;
      cJSON *ed = cJSON_GetObjectItem(x, "embed_drift");
      cJSON *dm = cJSON_GetObjectItem(x, "drift_from_mean");
      assert(ed && dm && cJSON_GetObjectItem(x, "scene_changed"));
      if (!strcmp(id, "st3")) {
        assert(cJSON_IsNumber(ed) && NEAR(ed->valuedouble, 1.0 - c12));
        assert(cJSON_IsNumber(dm));
        seen++;
      }
      if (!strcmp(id, "st1")) { assert(cJSON_IsNull(ed) && cJSON_IsNull(dm)); seen++; }
    }
    assert(seen == 2);
    cJSON_Delete(j);
  }
  printf("  drift: first frame null, previous-frame and recent-mean cosine "
         "distances, exposed by the stills list: ok\n");

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
  /* a frame that arrives as a ref (bytes already embedded) gets drift too,
   * against the previous EMBEDDED frame (st3; st4/st5 have no vector) */
  assert(NEAR(still_col("st6", "embed_drift"), 1.0 - cos_of(s2, s1)));
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

  /* 11. image→image: POST /api/cameras/visual-search/by-image */
  {
    camera("camera-discovery|river2", "mlit_river", "Asa River", 35.61, 139.61);
    unsigned char b4[256];
    size_t n4 = frame(b4, "four", 1);
    char s4[65], r4[160];
    blob(b4, n4, s4, r4);
    still("r2a", "camera-discovery|river2", s4, r4, "2026-10-12 04:00:00");
    still("r2b", "camera-discovery|river2", s2, r2, "2026-10-12 04:15:00"); /* = st6's frame */
    int before = vstub_posts("/embed/image");
    assert(image_vec_tick(&g_db, NULL, 0) == 1);           /* s4; s2 is a ref */
    assert(vstub_posts("/embed/image") == before + 1);
    long done_n = count("SELECT count(*) FROM image_vec_done");

    /* the args step: parse + role, no I/O */
    char va[VS_BYIMAGE_ARGS_MAX];
    int st = 0;
    char *e;
#define ARGS(body, role) visualsearchapi_by_image_args(body, strlen(body), role, va, sizeof va, &st)
    assert((e = ARGS("{}", "viewer")) && st == 400); free(e);
    assert((e = ARGS("not json", "viewer")) && st == 400); free(e);
    assert((e = ARGS("{\"still_id\":\"a\",\"upload_id\":\"b\"}", "owner")) && st == 400); free(e);
    assert((e = ARGS("{\"upload_id\":\"b\"}", "viewer")) && st == 403); free(e);
    assert((e = ARGS("{\"still_id\":\"a\",\"k\":true}", "viewer")) && st == 400); free(e);
    assert(!ARGS("{\"still_id\":\"st6\",\"k\":5,\"bbox\":[139,35,140,36]}", "viewer"));
    assert(strstr(va, "still_id=st6") && strstr(va, "k=5") && strstr(va, "bbox=139%2c35%2c140%2c36"));

    /* by a still: its vector is loaded, nothing is sent, its own camera is out */
    int posts_now = vstub_posts("/embed/image");
    char *b = visualsearchapi_by_image_route(&g_db, "t1", va, "", 0, &st);
    assert(b && st == 200);
    cJSON *j = cJSON_Parse(b);
    free(b);
    assert(cJSON_GetArraySize(cJSON_GetObjectItem(j, "data")) == 1);
    assert(!strcmp(jstr(j, "data.0.camera_id"), "camera-discovery|river2"));
    assert(!strcmp(jstr(j, "data.0.still.id"), "r2b"));    /* the identical frame */
    assert(!strcmp(jstr(j, "data.0.still.sha256"), s2));
    assert(NEAR(jnum(j, "data.0.cosine"), 1.0));
    assert(jnum(j, "data.0.frames_matched") == 2);
    assert(!strcmp(jstr(j, "meta.query.by"), "still"));
    assert(!strcmp(jstr(j, "meta.query.camera_id"), "camera-discovery|river1"));
    assert(!strcmp(jstr(j, "meta.excluded_camera.camera_id"), "camera-discovery|river1"));
    assert(jnum(j, "meta.k") == 5 && jnum(j, "meta.cameras.indexed") == 2);
    cJSON_Delete(j);
    assert(vstub_posts("/embed/image") == posts_now);
    /* bbox from the query string when the body has none; body wins otherwise */
    b = visualsearchapi_by_image_route(&g_db, "", "still_id=st6", "bbox=0,0,1,1", 0, &st);
    j = cJSON_Parse(b); free(b);
    assert(st == 200 && cJSON_GetArraySize(cJSON_GetObjectItem(j, "data")) == 0);
    cJSON_Delete(j);

    b = visualsearchapi_by_image_route(&g_db, "", "still_id=op1", "", 0, &st);
    assert(st == 409 && strstr(b, "still_not_indexable")); free(b);
    b = visualsearchapi_by_image_route(&g_db, "", "still_id=st5", "", 0, &st);
    assert(st == 409 && strstr(b, "still_not_embedded")); free(b);
    b = visualsearchapi_by_image_route(&g_db, "", "still_id=nope", "", 0, &st);
    assert(st == 404); free(b);

    /* an upload, end to end through uploadapi: the commit records a pending
     * media row of ITS tenant only */
    char updir[300];
    snprintf(updir, sizeof updir, "%s.uploads", getenv("JO_DB"));
    setenv("JO_UPLOAD_DIR", updir, 1);
    uploadapi_migrate(&g_db);
    tenant_ctx t1, t2;
    memset(&t1, 0, sizeof t1);
    snprintf(t1.tenant_id, sizeof t1.tenant_id, "t1");
    snprintf(t1.user_id, sizeof t1.user_id, "u1");
    snprintf(t1.role, sizeof t1.role, "analyst");
    t2 = t1;
    snprintf(t2.tenant_id, sizeof t2.tenant_id, "t2");
    char beg[160];
    snprintf(beg, sizeof beg, "{\"filename\":\"q.jpg\",\"content_type\":\"image/jpeg\","
             "\"total_bytes\":%zu}", n4);
    b = uploadapi(&g_db, &t1, "POST", "begin", "", NULL, beg, strlen(beg), &st);
    assert(b && st == 201);
    j = cJSON_Parse(b); free(b);
    char upid[64];
    snprintf(upid, sizeof upid, "%s", jstr(j, "data.upload_id"));
    cJSON_Delete(j);
    b = uploadapi(&g_db, &t1, "POST", upid, "part", "seq=0", (const char *)b4, n4, &st);
    assert(b && st == 200); free(b);
    b = uploadapi(&g_db, &t1, "POST", upid, "commit", "", NULL, 0, &st);
    assert(b && st == 200);
    j = cJSON_Parse(b); free(b);
    assert(cJSON_GetArraySize(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "data"), "media")) == 1);
    assert(!strcmp(jstr(j, "data.media.0.sha256"), s4));
    cJSON_Delete(j);
    char muid[80];
    snprintf(muid, sizeof muid, "upload:%s", upid);
    {
      char q[256];
      snprintf(q, sizeof q, "SELECT count(*) FROM media_assets WHERE item_uid='%s'"
               " AND tenant_id='t1' AND analyzed_at IS NULL", muid);
      assert(count(q) == 1);
    }
    /* the plain-auth item route never lists it; the other tenant never sees it */
    char *ml = media_list_for_item(&g_db, muid, 0);
    j = cJSON_Parse(ml); free(ml);
    assert(cJSON_GetArraySize(cJSON_GetObjectItem(j, "data")) == 0); cJSON_Delete(j);
    ml = media_list_for_upload(&g_db, "t2", muid, 0);
    j = cJSON_Parse(ml); free(ml);
    assert(cJSON_GetArraySize(cJSON_GetObjectItem(j, "data")) == 0); cJSON_Delete(j);
    b = uploadapi(&g_db, &t2, "GET", upid, "", NULL, NULL, 0, &st);
    assert(st == 404); free(b);

    /* the media pod analyses it from the evidence store, no fetch */
    setenv("MEDIA_NO_TOOLS", "1", 1);
    assert(media_analyze_batch(&g_db, NULL, 0, NULL) == 1);
    {
      char q[256];
      snprintf(q, sizeof q, "SELECT count(*) FROM media_assets WHERE item_uid='%s'"
               " AND analyzed_at IS NOT NULL AND content_type='image/jpeg'", muid);
      assert(count(q) == 1);
    }
    /* and the image index never walks it */
    assert(image_vec_tick(&g_db, NULL, 0) == 0);
    {
      char q[256];
      snprintf(q, sizeof q, "SELECT count(*) FROM image_vec_ref r JOIN media_assets m"
               " ON m.id=r.ref_id WHERE m.item_uid='%s'", muid);
      assert(count(q) == 0);
    }

    /* by an upload: embedded for the query, written nowhere */
    snprintf(beg, sizeof beg, "{\"upload_id\":\"%s\"}", upid);
    assert(!ARGS(beg, "analyst"));
    posts_now = vstub_posts("/embed/image");
    b = visualsearchapi_by_image_route(&g_db, "t1", va, "", 0, &st);
    assert(b && st == 200);
    j = cJSON_Parse(b); free(b);
    assert(vstub_posts("/embed/image") == posts_now + 1);
    assert(!strcmp(jstr(j, "data.0.camera_id"), "camera-discovery|river2"));
    assert(!strcmp(jstr(j, "data.0.still.id"), "r2a"));    /* the same bytes */
    assert(NEAR(jnum(j, "data.0.cosine"), 1.0));
    assert(!strcmp(jstr(j, "meta.query.by"), "upload"));
    assert(!cJSON_GetObjectItem(cJSON_GetObjectItem(j, "meta"), "excluded_camera"));
    cJSON_Delete(j);
    assert(count("SELECT count(*) FROM image_vec_done") == done_n);
    /* another tenant's upload id is 404, never 403 */
    b = visualsearchapi_by_image_route(&g_db, "t2", va, "", 0, &st);
    assert(st == 404); free(b);
#undef ARGS
  }
  printf("  by-image: a still's stored vector (own camera excluded, nothing sent), "
         "an upload embedded per query and never indexed, tenant-scoped media "
         "rows, 403/404/409s: ok\n");

  /* 12. reference frames: one per indexable camera record, fetched through
   * the stills fetch path from a local server (JO_CAMERA_ALLOW_LAN=1) */
  {
    char webdir[300], path[400];
    snprintf(webdir, sizeof webdir, "%s.web", getenv("JO_DB"));
    mkdir(webdir, 0700);
    /* a COMPLETE jpeg frame, as the stills trim demands: SOI, an APP0 segment
     * by length, SOS with entropy bytes, EOI */
    unsigned char rf[256];
    size_t nrf = 0;
    const char *tag = "road-ref-frame";
    rf[nrf++] = 0xFF; rf[nrf++] = 0xD8; rf[nrf++] = 0xFF; rf[nrf++] = 0xE0;
    rf[nrf++] = 0; rf[nrf++] = (unsigned char)(2 + strlen(tag));
    memcpy(rf + nrf, tag, strlen(tag)); nrf += strlen(tag);
    rf[nrf++] = 0xFF; rf[nrf++] = 0xDA; rf[nrf++] = 0; rf[nrf++] = 2;
    memcpy(rf + nrf, "entropy-bytes", 13); nrf += 13;
    rf[nrf++] = 0xFF; rf[nrf++] = 0xD9;
    snprintf(path, sizeof path, "%s/road.jpg", webdir);
    FILE *f = fopen(path, "wb"); assert(f); fwrite(rf, 1, nrf, f); fclose(f);
    char rfsha[65];
    hexsha(rf, nrf, rfsha);
    int port = web_start(webdir, 18680);
    if (port < 0) {
      printf("  SKIP refframes: no local web server (python3 -m http.server)\n");
    } else {
      setenv("JO_CAMERA_ALLOW_LAN", "1", 1);
      char sql[1200];
      snprintf(sql, sizeof sql,
        "INSERT INTO intel_items(uid,source_id,title,fetched_at,properties,"
        " record_type,lat,lon) VALUES"
        " ('camera-discovery|road1','camera-discovery','Route 1',datetime('now'),"
        "  '{\"discovery_channel\":\"mlit_road\",\"snapshot_url\":\"http://127.0.0.1:%d/road.jpg\"}',"
        "  'camera',35.62,139.62),"
        " ('camera-discovery|road2','camera-discovery','Route 2',datetime('now'),"
        "  '{\"discovery_channel\":\"mlit_road\",\"snapshot_url\":\"http://127.0.0.1:%d/gone.jpg\"}',"
        "  'camera',35.63,139.63),"
        " ('camera-discovery|open2','camera-discovery','Open 2',datetime('now'),"
        "  '{\"discovery_channel\":\"insecam_scrape\",\"snapshot_url\":\"http://127.0.0.1:%d/road.jpg\"}',"
        "  'camera',35.64,139.64)", port, port, port);
      exec(sql);

      /* inert without the sidecar */
      char keep[128];
      snprintf(keep, sizeof keep, "%s", getenv("JO_VISION_EMBED_URL"));
      unsetenv("JO_VISION_EMBED_URL");
      assert(camera_refframe_tick(&g_db, NULL, NULL) == 0);
      assert(!vecindex_table_exists(&g_db, "camera_refframes") ||
             count("SELECT count(*) FROM camera_refframes") == 0);
      setenv("JO_VISION_EMBED_URL", keep, 1);

      assert(camera_refframe_tick(&g_db, NULL, NULL) == 1);
      /* river1/river2 (cam.example does not resolve) are recorded, not skipped */
      assert(count("SELECT count(*) FROM camera_refframes WHERE"
                   " last_error='blocked_destination'") == 2);
      {
        char q[300];
        snprintf(q, sizeof q, "SELECT count(*) FROM camera_refframes WHERE"
                 " item_uid='camera-discovery|road1' AND sha256='%s'"
                 " AND evidence_id IS NOT NULL AND last_error IS NULL", rfsha);
        assert(count(q) == 1);
      }
      assert(count("SELECT count(*) FROM camera_refframes WHERE"
                   " item_uid='camera-discovery|road2' AND sha256 IS NULL"
                   " AND last_error='http_404' AND fail_count=1") == 1);
      /* the insecam record is never fetched */
      assert(count("SELECT count(*) FROM camera_refframes WHERE"
                   " item_uid='camera-discovery|open2'") == 0);
      /* attempted this week: nothing is due */
      assert(camera_refframe_tick(&g_db, NULL, NULL) == 0);
      assert(count("SELECT fail_count FROM camera_refframes WHERE"
                   " item_uid='camera-discovery|road2'") == 1);

      /* the image pod embeds it as kind refframe */
      assert(image_vec_tick(&g_db, NULL, 0) == 1);
      assert(count("SELECT count(*) FROM image_vec_ref WHERE kind='refframe'") == 1);
      assert(count("SELECT count(*) FROM image_vec WHERE kind='refframe'") == 1);
      int st;
      cJSON *j = vsearch("q=road&k=10", &st);
      assert(st == 200);
      int seen = 0;
      cJSON *x;
      cJSON_ArrayForEach(x, cJSON_GetObjectItem(j, "data")) {
        cJSON *c = cJSON_GetObjectItem(x, "camera_id");
        if (strcmp(c->valuestring, "camera-discovery|road1")) continue;
        seen++;
        assert(cJSON_IsNull(cJSON_GetObjectItem(x, "still")));
        cJSON *fr = cJSON_GetObjectItem(x, "frame");
        assert(!strcmp(cJSON_GetObjectItem(fr, "kind")->valuestring, "refframe"));
        assert(cJSON_IsString(cJSON_GetObjectItem(fr, "evidence_id")));
      }
      assert(seen == 1);
      assert(jnum(j, "meta.cameras.indexed") == 3);    /* river1, river2, road1 */
      cJSON_Delete(j);

      /* coverage reports both pods' numbers */
      cJSON *cov = vecindex_coverage_json(&g_db, &image_vec_spec);
      assert(jnum(cov, "extra.refs.refframe") == 1);
      assert(jnum(cov, "extra.refframes.cameras_with_frame") == 1);
      assert(jnum(cov, "extra.refframes.failing") >= 1);
      assert(jnum(cov, "extra.refframes.last_tick.walked") >= 3);
      cJSON_Delete(cov);

      /* a week later the camera shows something else: the ref moves to the
       * new bytes and the old vector goes */
      rf[25] ^= 0x01;                                /* entropy, still no FF */
      f = fopen(path, "wb"); assert(f); fwrite(rf, 1, nrf, f); fclose(f);
      char rfsha2[65];
      hexsha(rf, nrf, rfsha2);
      exec("UPDATE camera_refframes SET attempted_at=datetime('now','-8 days')"
           " WHERE item_uid='camera-discovery|road1'");
      exec("UPDATE image_vec_meta SET v='0' WHERE k='refframe_cursor'");
      assert(camera_refframe_tick(&g_db, NULL, NULL) == 1);
      assert(image_vec_tick(&g_db, NULL, 0) == 1);
      {
        char q[300];
        snprintf(q, sizeof q, "SELECT count(*) FROM image_vec_ref WHERE"
                 " kind='refframe' AND sha256='%s'", rfsha2);
        assert(count(q) == 1);
        snprintf(q, sizeof q, "SELECT count(*) FROM image_vec_done WHERE sha256='%s'", rfsha);
        assert(count(q) == 0);
      }
      unsetenv("JO_CAMERA_ALLOW_LAN");
      web_stop();
      printf("  refframes: one frame per indexable record via the stills fetch "
             "path, insecam never fetched, failures recorded, weekly, embedded as "
             "refframe, searchable, refreshed bytes move the ref: ok\n");
    }
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
