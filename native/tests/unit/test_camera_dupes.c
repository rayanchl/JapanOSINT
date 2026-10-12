/* test_camera_dupes.c — GET /api/admin/cameras/duplicates (core/camera_dupes.c).
 *
 * WHAT IS BEING PROTECTED.
 *   1. byte-identical latest frames are proposed as same_sha256 whatever the
 *      distance; an OLDER frame never counts;
 *   2. pHash and cosine pairs are proposed only within CAM_DUP_RADIUS_M, at
 *      the named thresholds, in that order of strength;
 *   3. a frame shared by more than CAM_DUP_SHA_GROUP_MAX cameras is a
 *      placeholder: not proposed, and listed in meta (never a silent drop);
 *   4. only cameras passing camera_record_indexable() take part;
 *   5. paged with a measured total; proposals only (nothing is written);
 *   6. the route refuses a non-operator even if httpd's gate were bypassed.
 *
 * Includes camera_dupes.c directly, so run.sh links every object except
 * obj/core/camera_dupes.o. */
#include "../../core/camera_dupes.c"

#include <assert.h>

static db_handle g_db;

static void exec(const char *sql) {
  char *e = NULL;
  if (sqlite3_exec(g_db.h, sql, NULL, NULL, &e) != SQLITE_OK) {
    fprintf(stderr, "SQL failed: %s\n  %s\n", e ? e : "?", sql);
    assert(0);
  }
}

static long count(const char *sql) {
  sqlite3_stmt *s; long n = -1;
  assert(sqlite3_prepare_v2(g_db.h, sql, -1, &s, NULL) == SQLITE_OK);
  if (sqlite3_step(s) == SQLITE_ROW) n = sqlite3_column_int64(s, 0);
  sqlite3_finalize(s);
  return n;
}

static void camera(const char *id, const char *channel, double lat, double lon) {
  char sql[1024];
  snprintf(sql, sizeof sql,
    "INSERT INTO intel_items(uid,source_id,title,fetched_at,properties,"
    " record_type,lat,lon) VALUES('camera-discovery|%s','camera-discovery','%s',"
    " datetime('now'),'{\"discovery_channel\":\"%s\",\"snapshot_url\":"
    "\"https://cam.example/%s.jpg\"}','camera',%f,%f)", id, id, channel, id, lat, lon);
  exec(sql);
}

/* sha is a 64-char fill of `c`; phash 0 = none */
static void still(const char *id, const char *cam, char c, const char *at,
                  long long phash) {
  char sha[65], sql[1024];
  memset(sha, c, 64); sha[64] = 0;
  char ph[32];
  if (phash) snprintf(ph, sizeof ph, "%lld", phash); else snprintf(ph, sizeof ph, "NULL");
  snprintf(sql, sizeof sql,
    "INSERT INTO camera_stills(id,camera_id,captured_at,sha256,bytes,phash)"
    " VALUES('%s','camera-discovery|%s','%s','%s',10,%s)", id, cam, at, sha, ph);
  exec(sql);
}

static void vec(char c, const float *v) {
  char sha[65];
  memset(sha, c, 64); sha[64] = 0;
  vecindex_writer w;
  exec("BEGIN");
  assert(vecindex_writer_open(&w, &g_db, &image_vec_spec) == 0);
  vecindex_val vals[2] = { VI_I(0), VI_S(IMAGE_VEC_KIND_STILL) };
  assert(vecindex_writer_put(&w, sha, v, 4, vals, 1, 1, "t") == 0);
  vecindex_writer_close(&w);
  exec("COMMIT");
}

static cJSON *dupes(const char *qs, int op, int *st) {
  char *b = camera_dupes_route(&g_db, "", "", qs, op, st);
  assert(b);
  cJSON *j = cJSON_Parse(b);
  free(b);
  assert(j);
  return j;
}

static double num(cJSON *o, const char *a, const char *b) {
  cJSON *x = cJSON_GetObjectItem(cJSON_GetObjectItem(o, a), b);
  return cJSON_IsNumber(x) ? x->valuedouble : -1;
}

/* the proposal pairing these two cameras, or NULL */
static cJSON *pair(cJSON *j, const char *a, const char *b) {
  char ia[80], ib[80];
  snprintf(ia, sizeof ia, "camera-discovery|%s", a);
  snprintf(ib, sizeof ib, "camera-discovery|%s", b);
  cJSON *x;
  cJSON_ArrayForEach(x, cJSON_GetObjectItem(j, "data")) {
    cJSON *cs = cJSON_GetObjectItem(x, "cameras");
    const char *c0 = cJSON_GetObjectItem(cJSON_GetArrayItem(cs, 0), "camera_id")->valuestring;
    const char *c1 = cJSON_GetObjectItem(cJSON_GetArrayItem(cs, 1), "camera_id")->valuestring;
    if ((!strcmp(c0, ia) && !strcmp(c1, ib)) || (!strcmp(c0, ib) && !strcmp(c1, ia)))
      return x;
  }
  return NULL;
}

static const char *sig(cJSON *p) {
  return cJSON_GetObjectItem(p, "signal")->valuestring;
}

int main(void) {
  const char *dbp = getenv("JO_DB");
  if (!dbp || !*dbp) { fprintf(stderr, "set JO_DB to a scratch path\n"); return 2; }
  unsetenv("JO_CAMERA_INDEX_CHANNELS");
  assert(db_open(&g_db, NULL, NULL) == 0);
  camera_stills_migrate(&g_db);
  image_vec_migrate(&g_db);
  assert(vecindex_create(&g_db, &image_vec_spec, 4) == 0);

  /* A, B, C within ~110 m; D far away; E an insecam camera next to A */
  camera("A", "mlit_road", 35.0000, 139.0);
  camera("B", "mlit_road", 35.0010, 139.0);
  camera("C", "mlit_road", 35.0005, 139.0);
  camera("D", "mlit_road", 36.0000, 140.0);
  camera("E", "insecam_scrape", 35.0001, 139.0);
  still("a0", "A", '0', "2026-10-01 00:00:00", 0x1234);  /* older: ignored */
  still("a1", "A", '1', "2026-10-12 00:00:00", 0x0F0F0F0FLL);
  still("b1", "B", '1', "2026-10-12 00:05:00", 0x0F0F0F0FLL);
  still("c1", "C", '2', "2026-10-12 00:00:00", 0x0F0F0F08LL);  /* 3 bits off */
  still("d1", "D", '1', "2026-10-12 00:00:00", 0);
  still("e1", "E", '1', "2026-10-12 00:00:00", 0x0F0F0F0FLL);  /* denied */
  /* G, H: no pHash, near-identical vectors; I: near G, a different vector */
  camera("G", "mlit_river", 34.0000, 135.0);
  camera("H", "mlit_river", 34.0020, 135.0);
  camera("I", "mlit_river", 34.0030, 135.0);
  camera("K", "mlit_river", 34.0300, 135.0);              /* 3.3 km: never */
  still("g1", "G", '3', "2026-10-12 00:00:00", 0);
  still("h1", "H", '4', "2026-10-12 00:00:00", 0);
  still("i1", "I", '5', "2026-10-12 00:00:00", 0);
  still("k1", "K", '6', "2026-10-12 00:00:00", 0);
  float vg[4] = {1, 0, 0, 0}, vh[4] = {0.99f, 0.05f, 0, 0}, vi[4] = {0, 1, 0, 0};
  vec('3', vg); vec('4', vh); vec('5', vi); vec('6', vg);   /* K = G, but far */
  /* nine cameras far apart sharing one "offline" frame */
  for (int i = 0; i < 9; i++) {
    char id[16], sid[16];
    snprintf(id, sizeof id, "P%d", i);
    snprintf(sid, sizeof sid, "p%d", i);
    camera(id, "nzta_road", 40.0 + i, 150.0);
    still(sid, id, '9', "2026-10-12 00:00:00", 0);
  }
  long rows_before = count("SELECT count(*) FROM camera_stills");

  int st;
  cJSON *j = dupes("", 0, &st);
  assert(st == 403);
  cJSON_Delete(j);

  j = dupes("", 1, &st);
  assert(st == 200);
  cJSON *p;
  assert((p = pair(j, "A", "B")) && !strcmp(sig(p), "same_sha256"));
  assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(p, "evidence"), "same_sha256")));
  assert(num(p, "evidence", "phash_distance") == 0);
  assert((p = pair(j, "A", "D")) && !strcmp(sig(p), "same_sha256"));   /* far: still */
  assert((p = pair(j, "B", "D")) && !strcmp(sig(p), "same_sha256"));
  assert((p = pair(j, "A", "C")) && !strcmp(sig(p), "phash"));
  assert(num(p, "evidence", "phash_distance") == 3);
  assert(num(p, "evidence", "distance_m") > 50 && num(p, "evidence", "distance_m") < 60);
  assert((p = pair(j, "B", "C")) && !strcmp(sig(p), "phash"));
  assert((p = pair(j, "G", "H")) && !strcmp(sig(p), "cosine"));
  assert(num(p, "evidence", "cosine") >= CAM_DUP_COSINE_MIN);
  assert(!pair(j, "G", "I"));                               /* not alike */
  assert(!pair(j, "G", "K"));                               /* too far */
  assert(!pair(j, "A", "E") && !pair(j, "B", "E"));         /* policy */
  assert(!pair(j, "P0", "P1"));                             /* placeholder */
  assert(num(j, "page", "total") == 6 && num(j, "page", "count") == 6);
  /* ordered by strength: the shas first, then pHash, then cosine */
  assert(!strcmp(sig(cJSON_GetArrayItem(cJSON_GetObjectItem(j, "data"), 0)), "same_sha256"));
  assert(!strcmp(sig(cJSON_GetArrayItem(cJSON_GetObjectItem(j, "data"), 5)), "cosine"));
  cJSON *meta = cJSON_GetObjectItem(j, "meta");
  cJSON *sk = cJSON_GetObjectItem(meta, "shared_frames_skipped");
  assert(cJSON_GetArraySize(sk) == 1);
  assert(cJSON_GetObjectItem(cJSON_GetArrayItem(sk, 0), "cameras")->valuedouble == 9);
  assert(num(j, "meta", "cameras_examined") == 17);         /* E is out */
  assert(num(j, "meta", "cameras_excluded_by_policy") == 1);
  assert(num(j, "meta", "cameras_with_phash") == 3);
  assert(num(j, "meta", "cameras_with_vector") == 4);
  cJSON_Delete(j);

  j = dupes("limit=2&offset=4", 1, &st);
  assert(st == 200 && num(j, "page", "count") == 2 && num(j, "page", "total") == 6);
  assert(cJSON_IsFalse(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "page"), "has_more")));
  cJSON_Delete(j);
  j = dupes("limit=2", 1, &st);
  assert(cJSON_IsTrue(cJSON_GetObjectItem(cJSON_GetObjectItem(j, "page"), "has_more")));
  cJSON_Delete(j);
  j = dupes("limit=0", 1, &st); assert(st == 400); cJSON_Delete(j);

  assert(count("SELECT count(*) FROM camera_stills") == rows_before);  /* read-only */
  db_close(&g_db);
  printf("test_camera_dupes: re-host by sha (any distance), pHash then cosine "
         "within the radius, placeholders listed not proposed, policy, paging: ok\n");
  return 0;
}
