/* core/visualsearchapi.c — see visualsearchapi.h. */
#include "visualsearchapi.h"
#include "camera_feed.h"
#include "camera_stills.h"
#include "image_vec.h"
#include "../lib/vecindex.h"
#include "../third_party/cJSON.h"
#include "../third_party/mongoose.h"
#include "../third_party/sqlite3.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VS_K_DEFAULT 10
#define VS_K_MAX     50
/* Frames the KNN may return per camera asked for. Many frames of one camera
 * sit close together in the space (a static scene drifts slowly), so asking
 * for k frames would often name one camera k times. */
#define VS_PER_CAMERA 20
/* vec0's own ceiling on k. */
#define VS_CAND_MAX  4096

static char *err_json(int *status, int code, const char *error, const char *reason) {
  *status = code;
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "error", error);
  if (reason) cJSON_AddStringToObject(o, "reason", reason);
  char *s = cJSON_PrintUnformatted(o);
  cJSON_Delete(o);
  return s;
}

static char *unavailable(int *status, const char *reason) {
  return err_json(status, 503, "visual_search_unavailable", reason);
}

/* 1 = got it, 0 = absent, -1 = too long for its buffer. */
static int qs_get(const char *qs, const char *k, char *out, size_t cap) {
  struct mg_str s = mg_str(qs ? qs : "");
  int n = mg_http_get_var(&s, k, out, cap);
  if (n <= 0) out[0] = 0;
  return n == -3 ? -1 : n > 0;
}

/* w,s,e,n with the ranges a coordinate can have. */
static int parse_bbox(const char *t, double v[4]) {
  const char *p = t;
  for (int i = 0; i < 4; i++) {
    char *end = NULL;
    v[i] = strtod(p, &end);
    if (end == p || !isfinite(v[i])) return 0;
    p = end;
    if (i < 3) { if (*p != ',') return 0; p++; }
  }
  if (*p) return 0;
  if (v[0] < -180 || v[0] > 180 || v[2] < -180 || v[2] > 180) return 0;
  if (v[1] < -90 || v[1] > 90 || v[3] < -90 || v[3] > 90 || v[1] > v[3]) return 0;
  return 1;
}

/* `since` in camera_stills.captured_at's format ("YYYY-MM-DD HH:MM:SS") and
 * as epoch seconds, via SQLite's own date parser so the two cannot disagree
 * with how the column is compared. 0 when SQLite cannot read it. */
static int parse_since(db_handle *db, const char *t, char out[32], long long *epoch) {
  sqlite3_stmt *q;
  int ok = 0;
  if (sqlite3_prepare_v2(db->h, "SELECT datetime(?1), CAST(strftime('%s', ?1) AS INTEGER)",
                         -1, &q, NULL) != SQLITE_OK) return 0;
  sqlite3_bind_text(q, 1, t, -1, SQLITE_TRANSIENT);
  if (sqlite3_step(q) == SQLITE_ROW && sqlite3_column_type(q, 0) != SQLITE_NULL) {
    snprintf(out, 32, "%s", (const char *)sqlite3_column_text(q, 0));
    *epoch = sqlite3_column_int64(q, 1);
    ok = 1;
  }
  sqlite3_finalize(q);
  return ok;
}

/* "N of M cameras indexed": M = capture targets with stills whose camera may
 * be indexed, N = those with at least one still in the index. */
static cJSON *camera_counts(db_handle *db) {
  long eligible = 0, indexed = 0, excluded = 0;
  sqlite3_stmt *q;
  if (sqlite3_prepare_v2(db->h,
      "SELECT c.camera_id, EXISTS (SELECT 1 FROM image_vec_ref r"
      "   WHERE r.kind='" IMAGE_VEC_KIND_STILL "' AND r.camera_uid=c.camera_id)"
      "  FROM (SELECT DISTINCT camera_id FROM camera_stills) c", -1, &q, NULL)
      == SQLITE_OK) {
    char *last_item = NULL;
    int last_ok = 0;
    while (sqlite3_step(q) == SQLITE_ROW) {
      const char *cam = (const char *)sqlite3_column_text(q, 0);
      const char *sfx = NULL;
      char *item = cam ? camera_feed_split(cam, &sfx) : NULL;
      if (!item) continue;
      /* views of one record are adjacent in DISTINCT order: one lookup each */
      if (!last_item || strcmp(last_item, item)) {
        free(last_item);
        last_item = strdup(item);
        last_ok = image_vec_item_indexable(db, item, 0);
      }
      free(item);
      if (!last_ok) { excluded++; continue; }
      eligible++;
      if (sqlite3_column_int(q, 1)) indexed++;
    }
    free(last_item);
    sqlite3_finalize(q);
  }
  cJSON *o = cJSON_CreateObject();
  cJSON_AddNumberToObject(o, "indexed", (double)indexed);
  cJSON_AddNumberToObject(o, "eligible", (double)eligible);
  cJSON_AddNumberToObject(o, "excluded_by_policy", (double)excluded);
  char note[96];
  snprintf(note, sizeof note, "%ld of %ld cameras indexed", indexed, eligible);
  cJSON_AddStringToObject(o, "note", note);
  return o;
}

typedef struct { char *item; int ok; double lat, lon; int has_geo; char *name; } vs_item;

static vs_item *item_get(db_handle *db, vs_item **cache, int *n, const char *uid) {
  for (int i = 0; i < *n; i++) if (!strcmp((*cache)[i].item, uid)) return &(*cache)[i];
  vs_item *c = realloc(*cache, (size_t)(*n + 1) * sizeof **cache);
  if (!c) return NULL;
  *cache = c;
  vs_item *it = &c[*n];
  memset(it, 0, sizeof *it);
  if (!(it->item = strdup(uid))) return NULL;
  (*n)++;
  it->ok = image_vec_item_indexable(db, uid, 0);
  sqlite3_stmt *q;
  if (it->ok && sqlite3_prepare_v2(db->h, "SELECT title, lat, lon FROM intel_items"
                                   " WHERE uid=?1", -1, &q, NULL) == SQLITE_OK) {
    sqlite3_bind_text(q, 1, uid, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(q) == SQLITE_ROW) {
      const char *t = (const char *)sqlite3_column_text(q, 0);
      it->name = t ? strdup(t) : NULL;
      if (sqlite3_column_type(q, 1) != SQLITE_NULL &&
          sqlite3_column_type(q, 2) != SQLITE_NULL) {
        it->has_geo = 1;
        it->lat = sqlite3_column_double(q, 1);
        it->lon = sqlite3_column_double(q, 2);
      }
    }
    sqlite3_finalize(q);
  }
  return it;
}

static int in_bbox(const vs_item *it, const double b[4]) {
  if (!it->has_geo) return 0;
  if (it->lat < b[1] || it->lat > b[3]) return 0;
  /* w > e is a box across the antimeridian */
  return b[0] <= b[2] ? (it->lon >= b[0] && it->lon <= b[2])
                      : (it->lon >= b[0] || it->lon <= b[2]);
}

static cJSON *find_camera(cJSON *data, const char *cam) {
  cJSON *x;
  cJSON_ArrayForEach(x, data) {
    cJSON *c = cJSON_GetObjectItem(x, "camera_id");
    if (cJSON_IsString(c) && !strcmp(c->valuestring, cam)) return x;
  }
  return NULL;
}

char *visualsearchapi_route(db_handle *db, const char *tenant, const char *id,
                            const char *qs, int is_operator, int *status) {
  (void)tenant; (void)id; (void)is_operator;
  const vecindex_spec *s = &image_vec_spec;
  char q[1024], bbox_s[128], since_s[64], k_s[16];
  if (qs_get(qs, "q", q, sizeof q) < 0 || qs_get(qs, "bbox", bbox_s, sizeof bbox_s) < 0 ||
      qs_get(qs, "since", since_s, sizeof since_s) < 0 || qs_get(qs, "k", k_s, sizeof k_s) < 0)
    return err_json(status, 414, "query_parameter_too_long", NULL);
  if (!q[0]) return err_json(status, 400, "bad_request", "q is required");
  int k = VS_K_DEFAULT;
  if (k_s[0]) {
    char *end = NULL;
    long v = strtol(k_s, &end, 10);
    if (!end || *end || v < 1) return err_json(status, 400, "bad_request", "k must be a positive integer");
    k = v > VS_K_MAX ? VS_K_MAX : (int)v;
  }
  double bb[4];
  int have_bbox = bbox_s[0] != 0;
  if (have_bbox && !parse_bbox(bbox_s, bb))
    return err_json(status, 400, "bad_request", "bbox must be w,s,e,n in degrees");
  char since[32] = "";
  long long since_epoch = 0;
  if (since_s[0] && !parse_since(db, since_s, since, &since_epoch))
    return err_json(status, 400, "bad_request", "since must be an ISO date or date-time");

  /* availability, most fundamental first, each with its own reason */
  if (!image_vec_base_url())
    return unavailable(status, "the vision sidecar is not configured "
                       "(JO_VISION_EMBED_URL is unset)");
  char why[600];
  if (vecindex_refused(db, s, why, sizeof why)) return unavailable(status, why);
  if (!image_vec_ready(db))
    return unavailable(status, "the camera image index has not been built yet "
                       "(the _image_embed pod has embedded no frame)");
  float *qv = NULL;
  int qdim = 0;
  char model[256], err[400];
  if (image_vec_embed_text(NULL, q, &qv, &qdim, model, sizeof model, err, sizeof err) != 0)
    return unavailable(status, err);
  if (vecindex_check_identity(db, s, model, qdim, why, sizeof why) != VI_ID_OK) {
    free(qv);
    return unavailable(status, why[0] ? why : "the index has no recorded model");
  }

  vecindex_val kv = VI_S(IMAGE_VEC_KIND_STILL), tv = VI_I(since_epoch);
  vecindex_filter f[2] = {
    { .col = "kind", .op = "=",  .vals = &kv, .nvals = 1 },
    { .col = "ts",   .op = ">=", .vals = &tv, .nvals = 1 },
  };
  int limit = k * VS_PER_CAMERA;
  if (limit > VS_CAND_MAX) limit = VS_CAND_MAX;
  vecindex_hit *h = NULL;
  int nh = vecindex_knn(db, s, qv, qdim, limit, f, since[0] ? 2 : 1, &h, err, sizeof err);
  if (nh < 0) { free(qv); return unavailable(status, err); }

  cJSON *data = cJSON_CreateArray();
  vs_item *cache = NULL;
  int ncache = 0, ncam = 0;
  sqlite3_stmt *rq = NULL;
  int prep = sqlite3_prepare_v2(db->h,
    "SELECT r.ref_id, r.camera_uid, r.item_uid, r.captured_at FROM image_vec_ref r"
    "  JOIN camera_stills st ON st.id=r.ref_id"
    " WHERE r.sha256=?1 AND r.kind='" IMAGE_VEC_KIND_STILL "'"
    "   AND (?2 = '' OR r.captured_at >= ?2)"
    " ORDER BY r.captured_at DESC, r.ref_id DESC", -1, &rq, NULL) == SQLITE_OK;
  for (int i = 0; prep && i < nh; i++) {
    sqlite3_reset(rq);
    sqlite3_bind_text(rq, 1, h[i].key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(rq, 2, since, -1, SQLITE_TRANSIENT);
    double cosv = NAN;
    while (sqlite3_step(rq) == SQLITE_ROW) {
      const char *sid = (const char *)sqlite3_column_text(rq, 0);
      const char *cam = (const char *)sqlite3_column_text(rq, 1);
      const char *item = (const char *)sqlite3_column_text(rq, 2);
      const char *at = (const char *)sqlite3_column_text(rq, 3);
      if (!sid || !cam || !item) continue;
      cJSON *have = find_camera(data, cam);
      if (have) {                  /* a nearer frame of this camera won */
        cJSON *fm = cJSON_GetObjectItem(have, "frames_matched");
        if (fm) cJSON_SetNumberValue(fm, fm->valuedouble + 1);
        continue;
      }
      if (ncam >= k) continue;
      vs_item *it = item_get(db, &cache, &ncache, item);
      if (!it || !it->ok) continue;               /* policy, at query time */
      if (have_bbox && !in_bbox(it, bb)) continue;
      if (isnan(cosv)) {
        float *sv = NULL; int sd = 0;
        cosv = (vecindex_load(db, s, h[i].key, &sv, &sd) == 0 && sd == qdim)
             ? vec_cosine(qv, sv, qdim) : 0.0;
        free(sv);
      }
      cJSON *o = cJSON_CreateObject();
      cJSON_AddStringToObject(o, "camera_id", cam);
      cJSON_AddStringToObject(o, "item_uid", item);
      const char *sfx = NULL;
      char *u = camera_feed_split(cam, &sfx);
      if (sfx) cJSON_AddStringToObject(o, "view", sfx);
      else     cJSON_AddNullToObject(o, "view");
      free(u);
      if (it->name) cJSON_AddStringToObject(o, "name", it->name);
      else          cJSON_AddNullToObject(o, "name");
      if (it->has_geo) {
        cJSON_AddNumberToObject(o, "lat", it->lat);
        cJSON_AddNumberToObject(o, "lon", it->lon);
      } else { cJSON_AddNullToObject(o, "lat"); cJSON_AddNullToObject(o, "lon"); }
      cJSON_AddNumberToObject(o, "distance", h[i].distance);
      cJSON_AddNumberToObject(o, "cosine", cosv);
      cJSON *st = cJSON_AddObjectToObject(o, "still");
      cJSON_AddStringToObject(st, "id", sid);
      if (at) cJSON_AddStringToObject(st, "captured_at", at);
      else    cJSON_AddNullToObject(st, "captured_at");
      cJSON_AddStringToObject(st, "sha256", h[i].key);
      cJSON_AddNumberToObject(o, "frames_matched", 1);
      cJSON_AddItemToArray(data, o);
      ncam++;
    }
  }
  if (rq) sqlite3_finalize(rq);
  for (int i = 0; i < ncache; i++) { free(cache[i].item); free(cache[i].name); }
  free(cache);
  free(qv);
  vecindex_hits_free(h, nh);

  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "data", data);
  cJSON *meta = cJSON_AddObjectToObject(root, "meta");
  cJSON_AddStringToObject(meta, "query", q);
  cJSON_AddNumberToObject(meta, "k", k);
  cJSON_AddNumberToObject(meta, "count", ncam);
  if (since[0]) cJSON_AddStringToObject(meta, "since", since);
  else          cJSON_AddNullToObject(meta, "since");
  if (have_bbox) {
    cJSON *b = cJSON_AddArrayToObject(meta, "bbox");
    for (int i = 0; i < 4; i++) cJSON_AddItemToArray(b, cJSON_CreateNumber(bb[i]));
  } else cJSON_AddNullToObject(meta, "bbox");
  cJSON_AddStringToObject(meta, "model", model);
  cJSON_AddNumberToObject(meta, "dim", qdim);
  cJSON_AddItemToObject(meta, "cameras", camera_counts(db));
  cJSON *cand = cJSON_AddObjectToObject(meta, "candidates");
  cJSON_AddNumberToObject(cand, "examined", nh);
  cJSON_AddNumberToObject(cand, "limit", limit);
  /* fewer than `limit` back means the index had no more frames to give */
  cJSON_AddBoolToObject(cand, "exhausted", ncam >= k || nh < limit);
  cJSON_AddItemToObject(meta, "coverage", vecindex_coverage_json(db, s));
  cJSON_AddStringToObject(meta, "note", "whole-frame scene similarity over public-"
                          "infrastructure cameras only; no person or face matching");
  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  *status = out ? 200 : 500;
  return out;
}
