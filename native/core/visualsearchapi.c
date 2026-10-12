/* core/visualsearchapi.c — see visualsearchapi.h. */
#include "visualsearchapi.h"
#include "camera_feed.h"
#include "camera_refframe.h"
#include "camera_stills.h"
#include "image_vec.h"
#include "media.h"                    /* media_sniff_type */
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

/* "N of M cameras indexed": M = capture targets with a still or a reference
 * frame (core/camera_refframe.h) whose camera may be indexed, N = those with
 * at least one of either in the index. */
static cJSON *camera_counts(db_handle *db) {
  long eligible = 0, indexed = 0, excluded = 0;
  sqlite3_stmt *q;
  if (sqlite3_prepare_v2(db->h,
      "SELECT c.camera_id, EXISTS (SELECT 1 FROM image_vec_ref r"
      "   WHERE r.kind IN ('" IMAGE_VEC_KIND_STILL "','" IMAGE_VEC_KIND_REFFRAME "')"
      "   AND r.camera_uid=c.camera_id)"
      "  FROM (SELECT camera_id FROM camera_stills"
      "        UNION SELECT camera_uid FROM camera_refframes WHERE sha256 IS NOT NULL) c",
      -1, &q, NULL)
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

/* k / bbox / since, shared by both query kinds. `primary` is read first and
 * `fallback` (the URL's query string) answers what it leaves out. NULL on
 * success; otherwise the error body, *status set. */
typedef struct {
  int k, have_bbox;
  double bb[4];
  char since[32];
  long long since_epoch;
} vs_params;

static int arg_get(const char *primary, const char *fallback, const char *k,
                   char *out, size_t cap) {
  int r = qs_get(primary, k, out, cap);
  if (r != 0) return r;
  return qs_get(fallback, k, out, cap);
}

static char *parse_params(db_handle *db, const char *primary, const char *fallback,
                          vs_params *p, int *status) {
  char bbox_s[128], since_s[64], k_s[16];
  memset(p, 0, sizeof *p);
  if (arg_get(primary, fallback, "bbox", bbox_s, sizeof bbox_s) < 0 ||
      arg_get(primary, fallback, "since", since_s, sizeof since_s) < 0 ||
      arg_get(primary, fallback, "k", k_s, sizeof k_s) < 0)
    return err_json(status, 414, "query_parameter_too_long", NULL);
  p->k = VS_K_DEFAULT;
  if (k_s[0]) {
    char *end = NULL;
    long v = strtol(k_s, &end, 10);
    if (!end || *end || v < 1) return err_json(status, 400, "bad_request", "k must be a positive integer");
    p->k = v > VS_K_MAX ? VS_K_MAX : (int)v;
  }
  p->have_bbox = bbox_s[0] != 0;
  if (p->have_bbox && !parse_bbox(bbox_s, p->bb))
    return err_json(status, 400, "bad_request", "bbox must be w,s,e,n in degrees");
  if (since_s[0] && !parse_since(db, since_s, p->since, &p->since_epoch))
    return err_json(status, 400, "bad_request", "since must be an ISO date or date-time");
  return NULL;
}

/* The index can answer at all: sidecar configured, not refused, built. */
static char *index_unavailable(db_handle *db, int *status) {
  if (!image_vec_base_url())
    return unavailable(status, "the vision sidecar is not configured "
                       "(JO_VISION_EMBED_URL is unset)");
  char why[600];
  if (vecindex_refused(db, &image_vec_spec, why, sizeof why)) return unavailable(status, why);
  if (!image_vec_ready(db))
    return unavailable(status, "the camera image index has not been built yet "
                       "(the _image_embed pod has embedded no frame)");
  camera_refframe_migrate(db);  /* an index built before reference frames */
  return NULL;
}

/* KNN for `qv`, grouped BY CAMERA, and the envelope both routes answer with.
 * `query` (consumed) becomes meta.query. `exclude_cam`, when set, is the
 * camera the query frame came from: it is left out of `data` (it would match
 * itself at distance 0) and named in meta.excluded_camera. */
static char *answer(db_handle *db, const float *qv, int qdim, const char *model,
                    const vs_params *p, cJSON *query, const char *exclude_cam,
                    int *status) {
  const vecindex_spec *s = &image_vec_spec;
  int k = p->k;
  char err[400];
  /* frames a camera answers with: its stills and its reference frame (never
   * a media asset, which is an item's image, not a view) */
  vecindex_val kv[2] = { VI_S(IMAGE_VEC_KIND_STILL), VI_S(IMAGE_VEC_KIND_REFFRAME) };
  vecindex_val tv = VI_I(p->since_epoch);
  vecindex_filter f[2] = {
    { .col = "kind", .op = "IN", .vals = kv, .nvals = 2 },
    { .col = "ts",   .op = ">=", .vals = &tv, .nvals = 1 },
  };
  int limit = (k + (exclude_cam ? 1 : 0)) * VS_PER_CAMERA;
  if (limit > VS_CAND_MAX) limit = VS_CAND_MAX;
  vecindex_hit *h = NULL;
  int nh = vecindex_knn(db, s, qv, qdim, limit, f, p->since[0] ? 2 : 1, &h, err, sizeof err);
  if (nh < 0) { cJSON_Delete(query); return unavailable(status, err); }

  cJSON *data = cJSON_CreateArray();
  vs_item *cache = NULL;
  int ncache = 0, ncam = 0;
  long self_frames = 0;
  sqlite3_stmt *rq = NULL;
  int prep = sqlite3_prepare_v2(db->h,
    "SELECT r.ref_id, r.camera_uid, r.item_uid, r.captured_at, r.kind,"
    "       (SELECT f.evidence_id FROM camera_refframes f WHERE f.item_uid=r.ref_id)"
    "  FROM image_vec_ref r"
    " WHERE r.sha256=?1 AND (?2 = '' OR r.captured_at >= ?2)"
    "   AND ((r.kind='" IMAGE_VEC_KIND_STILL "'"
    "         AND EXISTS (SELECT 1 FROM camera_stills st WHERE st.id=r.ref_id))"
    "     OR (r.kind='" IMAGE_VEC_KIND_REFFRAME "'"
    "         AND EXISTS (SELECT 1 FROM camera_refframes f WHERE f.item_uid=r.ref_id"
    "                     AND f.sha256=r.sha256)))"
    /* a still first: it is the frame a client can open and pivot on */
    " ORDER BY r.kind='" IMAGE_VEC_KIND_STILL "' DESC, r.captured_at DESC, r.ref_id DESC",
    -1, &rq, NULL) == SQLITE_OK;
  for (int i = 0; prep && i < nh; i++) {
    sqlite3_reset(rq);
    sqlite3_bind_text(rq, 1, h[i].key, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(rq, 2, p->since, -1, SQLITE_TRANSIENT);
    double cosv = NAN;
    while (sqlite3_step(rq) == SQLITE_ROW) {
      const char *sid = (const char *)sqlite3_column_text(rq, 0);
      const char *cam = (const char *)sqlite3_column_text(rq, 1);
      const char *item = (const char *)sqlite3_column_text(rq, 2);
      const char *at = (const char *)sqlite3_column_text(rq, 3);
      const char *rk = (const char *)sqlite3_column_text(rq, 4);
      const char *evid = (const char *)sqlite3_column_text(rq, 5);
      int is_still = rk && !strcmp(rk, IMAGE_VEC_KIND_STILL);
      if (!sid || !cam || !item || !rk) continue;
      if (exclude_cam && !strcmp(cam, exclude_cam)) { self_frames++; continue; }
      cJSON *have = find_camera(data, cam);
      if (have) {                  /* a nearer frame of this camera won */
        cJSON *fm = cJSON_GetObjectItem(have, "frames_matched");
        if (fm) cJSON_SetNumberValue(fm, fm->valuedouble + 1);
        continue;
      }
      if (ncam >= k) continue;
      vs_item *it = item_get(db, &cache, &ncache, item);
      if (!it || !it->ok) continue;               /* policy, at query time */
      if (p->have_bbox && !in_bbox(it, p->bb)) continue;
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
      if (is_still) {
        cJSON *st = cJSON_AddObjectToObject(o, "still");
        cJSON_AddStringToObject(st, "id", sid);
        if (at) cJSON_AddStringToObject(st, "captured_at", at);
        else    cJSON_AddNullToObject(st, "captured_at");
        cJSON_AddStringToObject(st, "sha256", h[i].key);
      } else cJSON_AddNullToObject(o, "still");
      /* the matched frame, whatever its kind: a reference frame has no still
       * row, only its evidence blob */
      cJSON *fr = cJSON_AddObjectToObject(o, "frame");
      cJSON_AddStringToObject(fr, "kind", rk);
      cJSON_AddStringToObject(fr, "ref_id", sid);
      if (at) cJSON_AddStringToObject(fr, "captured_at", at);
      else    cJSON_AddNullToObject(fr, "captured_at");
      cJSON_AddStringToObject(fr, "sha256", h[i].key);
      if (!is_still && evid) cJSON_AddStringToObject(fr, "evidence_id", evid);
      else                   cJSON_AddNullToObject(fr, "evidence_id");
      cJSON_AddNumberToObject(o, "frames_matched", 1);
      cJSON_AddItemToArray(data, o);
      ncam++;
    }
  }
  if (rq) sqlite3_finalize(rq);
  for (int i = 0; i < ncache; i++) { free(cache[i].item); free(cache[i].name); }
  free(cache);
  vecindex_hits_free(h, nh);

  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "data", data);
  cJSON *meta = cJSON_AddObjectToObject(root, "meta");
  cJSON_AddItemToObject(meta, "query", query);
  cJSON_AddNumberToObject(meta, "k", k);
  cJSON_AddNumberToObject(meta, "count", ncam);
  if (p->since[0]) cJSON_AddStringToObject(meta, "since", p->since);
  else             cJSON_AddNullToObject(meta, "since");
  if (p->have_bbox) {
    cJSON *b = cJSON_AddArrayToObject(meta, "bbox");
    for (int i = 0; i < 4; i++) cJSON_AddItemToArray(b, cJSON_CreateNumber(p->bb[i]));
  } else cJSON_AddNullToObject(meta, "bbox");
  if (exclude_cam) {
    cJSON *ex = cJSON_AddObjectToObject(meta, "excluded_camera");
    cJSON_AddStringToObject(ex, "camera_id", exclude_cam);
    cJSON_AddNumberToObject(ex, "frames_skipped", (double)self_frames);
  }
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

char *visualsearchapi_route(db_handle *db, const char *tenant, const char *id,
                            const char *qs, int is_operator, int *status) {
  (void)tenant; (void)id; (void)is_operator;
  const vecindex_spec *s = &image_vec_spec;
  char q[1024];
  if (qs_get(qs, "q", q, sizeof q) < 0)
    return err_json(status, 414, "query_parameter_too_long", NULL);
  vs_params p;
  char *e = parse_params(db, NULL, qs, &p, status);
  if (e) return e;
  if (!q[0]) return err_json(status, 400, "bad_request", "q is required");

  /* availability, most fundamental first, each with its own reason */
  if ((e = index_unavailable(db, status)) != NULL) return e;
  float *qv = NULL;
  int qdim = 0;
  char model[256], err[400], why[600];
  if (image_vec_embed_text(NULL, q, &qv, &qdim, model, sizeof model, err, sizeof err) != 0)
    return unavailable(status, err);
  if (vecindex_check_identity(db, s, model, qdim, why, sizeof why) != VI_ID_OK) {
    free(qv);
    return unavailable(status, why[0] ? why : "the index has no recorded model");
  }
  char *out = answer(db, qv, qdim, model, &p, cJSON_CreateString(q), NULL, status);
  free(qv);
  return out;
}

/* ---- POST /api/cameras/visual-search/by-image ---------------------------- */

static int role_analyst(const char *r) {
  return r && (!strcmp(r, "owner") || !strcmp(r, "admin") || !strcmp(r, "analyst"));
}

/* One url-encoded `k=v` onto out; 0, or -1 when it does not fit. */
static int put_arg(char *out, size_t cap, const char *k, const char *v) {
  size_t used = strlen(out);
  char enc[VS_BYIMAGE_ARGS_MAX];
  size_t n = mg_url_encode(v, strlen(v), enc, sizeof enc);
  if (n == 0 && *v) return -1;
  int w = snprintf(out + used, cap - used, "%s%s=%s", used ? "&" : "", k, enc);
  return (w < 0 || (size_t)w >= cap - used) ? -1 : 0;
}

char *visualsearchapi_by_image_args(const char *body, size_t len, const char *role,
                                    char *out, size_t cap, int *status) {
  out[0] = 0;
  if (!body || !len) return err_json(status, 400, "bad_request",
                                     "a JSON body with still_id or upload_id is required");
  if (len > 8192) return err_json(status, 413, "body_too_large", NULL);
  /* a bounded NUL-terminated copy: cJSON's number path runs strtod on the raw
   * pointer (uploadapi.h, BINARY BODIES) */
  char *copy = malloc(len + 1);
  if (!copy) return err_json(status, 500, "server_error", NULL);
  memcpy(copy, body, len);
  copy[len] = 0;
  cJSON *j = cJSON_Parse(copy);
  free(copy);
  if (!cJSON_IsObject(j)) {
    cJSON_Delete(j);
    return err_json(status, 400, "bad_request", "the body must be a JSON object");
  }
  cJSON *sid = cJSON_GetObjectItem(j, "still_id"), *uid = cJSON_GetObjectItem(j, "upload_id");
  int has_s = cJSON_IsString(sid) && sid->valuestring[0];
  int has_u = cJSON_IsString(uid) && uid->valuestring[0];
  char *e = NULL;
  char num[160];
  if (has_s == has_u)
    e = err_json(status, 400, "bad_request", "exactly one of still_id or upload_id");
  /* an upload in flight or committed is unreleased evidence: the same role
   * the upload routes themselves demand (uploadapi.h, ROUTES) */
  else if (has_u && !role_analyst(role))
    e = err_json(status, 403, "forbidden", "searching by an upload needs the analyst role");
  else if (put_arg(out, cap, has_s ? "still_id" : "upload_id",
                   has_s ? sid->valuestring : uid->valuestring) != 0)
    e = err_json(status, 414, "query_parameter_too_long", NULL);
  static const char *const keys[] = { "k", "bbox", "since" };
  for (size_t i = 0; !e && i < sizeof keys / sizeof *keys; i++) {
    cJSON *x = cJSON_GetObjectItem(j, keys[i]);
    if (!x || cJSON_IsNull(x)) continue;
    const char *v = NULL;
    if (cJSON_IsString(x)) v = x->valuestring;
    else if (cJSON_IsNumber(x) && i == 0) {
      snprintf(num, sizeof num, "%.17g", x->valuedouble);
      v = num;
    } else if (cJSON_IsArray(x) && i == 1 && cJSON_GetArraySize(x) == 4) {
      size_t o = 0;
      cJSON *c;
      cJSON_ArrayForEach(c, x) {
        if (!cJSON_IsNumber(c)) { o = 0; break; }
        o += (size_t)snprintf(num + o, sizeof num - o, "%s%.9g", o ? "," : "", c->valuedouble);
        if (o >= sizeof num) { o = 0; break; }
      }
      if (o) v = num;
    }
    if (!v) {
      char why[64];
      snprintf(why, sizeof why, "%s has the wrong type", keys[i]);
      e = err_json(status, 400, "bad_request", why);
    } else if (put_arg(out, cap, keys[i], v) != 0)
      e = err_json(status, 414, "query_parameter_too_long", NULL);
  }
  cJSON_Delete(j);
  if (e) out[0] = 0;
  return e;
}

/* A still's frame: its camera must pass the policy NOW, and its bytes must
 * already be embedded (this route sends nothing to the sidecar for a still). */
static char *by_still(db_handle *db, const char *still_id, const vs_params *p,
                      int *status) {
  const vecindex_spec *s = &image_vec_spec;
  char cam[640] = "", sha[65] = "";
  sqlite3_stmt *q;
  if (sqlite3_prepare_v2(db->h, "SELECT camera_id, sha256 FROM camera_stills"
                         " WHERE id=?1", -1, &q, NULL) == SQLITE_OK) {
    sqlite3_bind_text(q, 1, still_id, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(q) == SQLITE_ROW && sqlite3_column_text(q, 0) &&
        sqlite3_column_text(q, 1)) {
      snprintf(cam, sizeof cam, "%s", (const char *)sqlite3_column_text(q, 0));
      snprintf(sha, sizeof sha, "%s", (const char *)sqlite3_column_text(q, 1));
    }
    sqlite3_finalize(q);
  }
  if (!cam[0]) return err_json(status, 404, "not_found", "no such still");
  const char *sfx = NULL;
  char *item = camera_feed_split(cam, &sfx);
  int ok = item && image_vec_item_indexable(db, item, 0);
  free(item);
  if (!ok) return err_json(status, 409, "still_not_indexable",
                           "this camera is outside the public-infrastructure "
                           "allowlist (camera_privacy.h); its frames are never "
                           "indexed or searched");
  char *e = index_unavailable(db, status);
  if (e) return e;
  float *v = NULL;
  int dim = 0;
  if (vecindex_load(db, s, sha, &v, &dim) != 0)
    return err_json(status, 409, "still_not_embedded",
                    "this frame has no vector yet: the _image_embed pod has not "
                    "reached it, or its bytes were never preserved");
  char model[256] = "";
  vecindex_meta_get(db, s, "model", model, sizeof model);
  cJSON *qm = cJSON_CreateObject();
  cJSON_AddStringToObject(qm, "by", "still");
  cJSON_AddStringToObject(qm, "still_id", still_id);
  cJSON_AddStringToObject(qm, "camera_id", cam);
  cJSON_AddStringToObject(qm, "sha256", sha);
  char *out = answer(db, v, dim, model, p, qm, cam, status);
  free(v);
  return out;
}

/* An upload's bytes: this tenant's, committed, preserved. Embedded for the
 * query and NOT written anywhere — an upload never enters the index. */
static char *by_upload(db_handle *db, const char *tenant, const char *upload_id,
                       const vs_params *p, int *status) {
  const vecindex_spec *s = &image_vec_spec;
  char sha[65] = "", stt[16] = "";
  sqlite3_stmt *q;
  /* a tenant mismatch is 404, never 403 (uploadapi.h, ERROR MODEL) */
  if (tenant && *tenant && vecindex_table_exists(db, "uploads") &&
      sqlite3_prepare_v2(db->h, "SELECT status, sha256 FROM uploads"
                         " WHERE id=?1 AND tenant_id=?2", -1, &q, NULL) == SQLITE_OK) {
    sqlite3_bind_text(q, 1, upload_id, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(q, 2, tenant, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(q) == SQLITE_ROW) {
      const char *a = (const char *)sqlite3_column_text(q, 0);
      const char *b = (const char *)sqlite3_column_text(q, 1);
      snprintf(stt, sizeof stt, "%s", a ? a : "");
      snprintf(sha, sizeof sha, "%s", b ? b : "");
    }
    sqlite3_finalize(q);
  }
  if (!stt[0]) return err_json(status, 404, "not_found", "no such upload");
  if (strcmp(stt, "committed") || strlen(sha) != 64)
    return err_json(status, 409, "upload_not_committed", NULL);
  char *e = index_unavailable(db, status);
  if (e) return e;
  size_t len = 0;
  int big = 0;
  unsigned char *b = image_vec_read_frame(db, sha, NULL, &len, &big);
  if (!b)
    return err_json(status, 409, "upload_bytes_unavailable", big
                    ? "the upload is larger than the vision sidecar takes per image"
                    : "the upload's bytes are not in the evidence store "
                      "(too large to preserve, or reaped)");
  if (!media_sniff_type(b, len)) {
    free(b);
    return err_json(status, 415, "not_an_image", "the upload is not a recognised image");
  }
  float *v = NULL;
  int dim = 0;
  char model[256], err[400], why[600];
  int rc = image_vec_embed_image(NULL, b, len, &v, &dim, model, sizeof model, err, sizeof err);
  free(b);
  if (rc != 0) return unavailable(status, err);
  if (vecindex_check_identity(db, s, model, dim, why, sizeof why) != VI_ID_OK) {
    free(v);
    return unavailable(status, why[0] ? why : "the index has no recorded model");
  }
  cJSON *qm = cJSON_CreateObject();
  cJSON_AddStringToObject(qm, "by", "upload");
  cJSON_AddStringToObject(qm, "upload_id", upload_id);
  cJSON_AddStringToObject(qm, "sha256", sha);
  char *out = answer(db, v, dim, model, p, qm, NULL, status);
  free(v);
  return out;
}

char *visualsearchapi_by_image_route(db_handle *db, const char *tenant,
                                     const char *id, const char *qs,
                                     int is_operator, int *status) {
  (void)is_operator;
  char still_id[256], upload_id[64];
  if (qs_get(id, "still_id", still_id, sizeof still_id) < 0 ||
      qs_get(id, "upload_id", upload_id, sizeof upload_id) < 0)
    return err_json(status, 400, "bad_request", "id too long");
  vs_params p;
  char *e = parse_params(db, id, qs, &p, status);
  if (e) return e;
  if (still_id[0] && !upload_id[0]) return by_still(db, still_id, &p, status);
  if (upload_id[0] && !still_id[0]) return by_upload(db, tenant, upload_id, &p, status);
  return err_json(status, 400, "bad_request", "exactly one of still_id or upload_id");
}

