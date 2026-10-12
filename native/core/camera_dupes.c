/* core/camera_dupes.c — see camera_dupes.h. */
#include "camera_dupes.h"
#include "camera_feed.h"
#include "camera_refframe.h"
#include "camera_stills.h"
#include "image_vec.h"
#include "media.h"                    /* media_phash_distance */
#include "../lib/vecindex.h"
#include "../third_party/cJSON.h"
#include "../third_party/mongoose.h"
#include "../third_party/sqlite3.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DUP_LIMIT_DEFAULT 50
#define DUP_LIMIT_MAX     500
#define DUP_SKIPPED_MAX   50          /* placeholder frames listed in meta    */

enum { SIG_SHA = 0, SIG_PHASH, SIG_COSINE };
static const char *const SIG_NAME[] = { "same_sha256", "phash", "cosine" };

typedef struct {
  char *cam, *item, *kind, *ref, *sha, *at, *name;
  int has_ph, has_geo, ok;
  uint64_t ph;
  double lat, lon;
  float *vec; int dim, vec_tried;
} dcam;

typedef struct {
  int a, b, sig;
  const char *ca, *cb;              /* the two camera ids, for the ordering */
  int phd;                          /* -1 = not measured                    */
  double cos, dist;                 /* NAN = not measured                   */
} dpair;

static char *err_json(int *status, int code, const char *error) {
  *status = code;
  char *s = malloc(64 + strlen(error));
  if (s) sprintf(s, "{\"error\":\"%s\"}", error);
  return s;
}

static char *dupcol(sqlite3_stmt *s, int i) {
  const char *t = (const char *)sqlite3_column_text(s, i);
  return t ? strdup(t) : NULL;
}

static void dcam_free(dcam *c) {
  free(c->cam); free(c->item); free(c->kind); free(c->ref); free(c->sha);
  free(c->at); free(c->name); free(c->vec);
}

/* Find or add the camera; the newer frame wins. */
static int add_frame(dcam **v, int *n, int *cap, const char *cam, const char *kind,
                     const char *ref, const char *sha, const char *at,
                     int has_ph, uint64_t ph) {
  for (int i = 0; i < *n; i++) {
    dcam *c = &(*v)[i];
    if (strcmp(c->cam, cam)) continue;
    if (strcmp(at ? at : "", c->at ? c->at : "") <= 0) return 0;
    free(c->kind); free(c->ref); free(c->sha); free(c->at);
    c->kind = strdup(kind); c->ref = strdup(ref); c->sha = strdup(sha);
    c->at = at ? strdup(at) : NULL;
    c->has_ph = has_ph; c->ph = ph;
    return 0;
  }
  if (*n == *cap) {
    int nc = *cap ? *cap * 2 : 256;
    dcam *a = realloc(*v, (size_t)nc * sizeof *a);
    if (!a) return -1;
    *v = a; *cap = nc;
  }
  dcam *c = &(*v)[*n];
  memset(c, 0, sizeof *c);
  c->cam = strdup(cam); c->kind = strdup(kind); c->ref = strdup(ref);
  c->sha = strdup(sha); c->at = at ? strdup(at) : NULL;
  c->has_ph = has_ph; c->ph = ph;
  if (!c->cam || !c->kind || !c->ref || !c->sha) { dcam_free(c); return -1; }
  (*n)++;
  return 0;
}

/* Every target's latest frame: newest still per target, then reference
 * frames (core/camera_refframe.h) where newer. */
static int load_frames(db_handle *db, dcam **v, int *n) {
  int cap = 0;
  sqlite3_stmt *q;
  if (sqlite3_prepare_v2(db->h,
      "SELECT s.camera_id, s.id, s.sha256, s.captured_at, s.phash"
      "  FROM camera_stills s WHERE s.captured_at ="
      "  (SELECT MAX(captured_at) FROM camera_stills WHERE camera_id=s.camera_id)"
      " ORDER BY s.camera_id, s.id DESC", -1, &q, NULL) != SQLITE_OK) return -1;
  while (sqlite3_step(q) == SQLITE_ROW) {
    const char *cam = (const char *)sqlite3_column_text(q, 0);
    const char *id = (const char *)sqlite3_column_text(q, 1);
    const char *sha = (const char *)sqlite3_column_text(q, 2);
    if (!cam || !id || !sha) continue;
    int hp = sqlite3_column_type(q, 4) != SQLITE_NULL;
    if (add_frame(v, n, &cap, cam, IMAGE_VEC_KIND_STILL, id, sha,
                  (const char *)sqlite3_column_text(q, 3), hp,
                  hp ? (uint64_t)sqlite3_column_int64(q, 4) : 0) != 0) break;
  }
  sqlite3_finalize(q);
  if (vecindex_table_exists(db, "camera_refframes") &&
      sqlite3_prepare_v2(db->h,
        "SELECT camera_uid, item_uid, sha256, fetched_at FROM camera_refframes"
        " WHERE sha256 IS NOT NULL", -1, &q, NULL) == SQLITE_OK) {
    while (sqlite3_step(q) == SQLITE_ROW) {
      const char *cam = (const char *)sqlite3_column_text(q, 0);
      const char *ref = (const char *)sqlite3_column_text(q, 1);
      const char *sha = (const char *)sqlite3_column_text(q, 2);
      if (!cam || !ref || !sha) continue;
      if (add_frame(v, n, &cap, cam, IMAGE_VEC_KIND_REFFRAME, ref, sha,
                    (const char *)sqlite3_column_text(q, 3), 0, 0) != 0) break;
    }
    sqlite3_finalize(q);
  }
  return 0;
}

/* Policy, name and location per camera, one lookup per record. */
static void load_items(db_handle *db, dcam *v, int n) {
  sqlite3_stmt *q = NULL;
  sqlite3_prepare_v2(db->h, "SELECT title, lat, lon FROM intel_items WHERE uid=?1",
                     -1, &q, NULL);
  for (int i = 0; i < n; i++) {
    const char *sfx = NULL;
    v[i].item = camera_feed_split(v[i].cam, &sfx);
    if (!v[i].item) continue;
    int same = i > 0 && v[i - 1].item && !strcmp(v[i - 1].item, v[i].item);
    v[i].ok = same ? v[i - 1].ok : image_vec_item_indexable(db, v[i].item, 0);
    if (!v[i].ok || !q) continue;
    sqlite3_reset(q);
    sqlite3_bind_text(q, 1, v[i].item, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(q) == SQLITE_ROW) {
      v[i].name = dupcol(q, 0);
      if (sqlite3_column_type(q, 1) != SQLITE_NULL &&
          sqlite3_column_type(q, 2) != SQLITE_NULL) {
        v[i].has_geo = 1;
        v[i].lat = sqlite3_column_double(q, 1);
        v[i].lon = sqlite3_column_double(q, 2);
      }
    }
  }
  if (q) sqlite3_finalize(q);
}

static double haversine_m(double la1, double lo1, double la2, double lo2) {
  const double r = 6371000.0, d2r = M_PI / 180.0;
  double dla = (la2 - la1) * d2r, dlo = (lo2 - lo1) * d2r;
  double a = sin(dla / 2) * sin(dla / 2) +
             cos(la1 * d2r) * cos(la2 * d2r) * sin(dlo / 2) * sin(dlo / 2);
  return 2 * r * asin(sqrt(a < 1 ? a : 1));
}

static int vec_of(db_handle *db, dcam *c) {
  if (!c->vec_tried) {
    c->vec_tried = 1;
    if (vecindex_load(db, &image_vec_spec, c->sha, &c->vec, &c->dim) != 0) {
      c->vec = NULL; c->dim = 0;
    }
  }
  return c->vec != NULL;
}

static int push(dpair **p, int *n, int *cap, dpair x) {
  if (*n == *cap) {
    int nc = *cap ? *cap * 2 : 256;
    dpair *a = realloc(*p, (size_t)nc * sizeof *a);
    if (!a) return -1;
    *p = a; *cap = nc;
  }
  (*p)[(*n)++] = x;
  return 0;
}

static int by_sha(const void *a, const void *b) {
  const dcam *x = *(dcam *const *)a, *y = *(dcam *const *)b;
  return strcmp(x->sha, y->sha);
}

static int by_lat(const void *a, const void *b) {
  const dcam *x = *(dcam *const *)a, *y = *(dcam *const *)b;
  return x->lat < y->lat ? -1 : x->lat > y->lat;
}

static int pair_cmp(const void *a, const void *b) {
  const dpair *x = a, *y = b;
  if (x->sig != y->sig) return x->sig - y->sig;
  if (x->sig == SIG_PHASH && x->phd != y->phd) return x->phd - y->phd;
  if (x->sig == SIG_COSINE && x->cos != y->cos) return x->cos > y->cos ? -1 : 1;
  int c = strcmp(x->ca, y->ca);
  return c ? c : strcmp(x->cb, y->cb);
}

static cJSON *cam_json(const dcam *c) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "camera_id", c->cam);
  cJSON_AddStringToObject(o, "item_uid", c->item);
  const char *sfx = NULL;
  char *u = camera_feed_split(c->cam, &sfx);
  if (sfx) cJSON_AddStringToObject(o, "view", sfx);
  else     cJSON_AddNullToObject(o, "view");
  free(u);
  if (c->name) cJSON_AddStringToObject(o, "name", c->name);
  else         cJSON_AddNullToObject(o, "name");
  if (c->has_geo) {
    cJSON_AddNumberToObject(o, "lat", c->lat);
    cJSON_AddNumberToObject(o, "lon", c->lon);
  } else { cJSON_AddNullToObject(o, "lat"); cJSON_AddNullToObject(o, "lon"); }
  cJSON *f = cJSON_AddObjectToObject(o, "frame");
  cJSON_AddStringToObject(f, "kind", c->kind);
  cJSON_AddStringToObject(f, "ref_id", c->ref);
  cJSON_AddStringToObject(f, "sha256", c->sha);
  if (c->at) cJSON_AddStringToObject(f, "captured_at", c->at);
  else       cJSON_AddNullToObject(f, "captured_at");
  if (c->has_ph) {
    /* 16 hex chars, never a JSON number (media.h: a 64-bit hash does not
     * survive an IEEE double) */
    char b[17];
    snprintf(b, sizeof b, "%016llx", (unsigned long long)c->ph);
    cJSON_AddStringToObject(f, "phash", b);
  } else cJSON_AddNullToObject(f, "phash");
  return o;
}

char *camera_dupes_route(db_handle *db, const char *tenant, const char *id,
                         const char *qs, int is_operator, int *status) {
  (void)tenant; (void)id;
  /* httpd.c gates the route; this is the second lock on the same door */
  if (!is_operator) return err_json(status, 403, "Platform operator role required");
  int limit = DUP_LIMIT_DEFAULT, offset = 0;
  {
    struct mg_str s = mg_str(qs ? qs : "");
    char b[24];
    if (mg_http_get_var(&s, "limit", b, sizeof b) > 0) {
      char *end = NULL; long v = strtol(b, &end, 10);
      if (!end || *end || v < 1) return err_json(status, 400, "limit must be a positive integer");
      limit = v > DUP_LIMIT_MAX ? DUP_LIMIT_MAX : (int)v;
    }
    if (mg_http_get_var(&s, "offset", b, sizeof b) > 0) {
      char *end = NULL; long v = strtol(b, &end, 10);
      if (!end || *end || v < 0) return err_json(status, 400, "offset must be a non-negative integer");
      offset = (int)v;
    }
  }
  camera_stills_migrate(db);
  camera_refframe_migrate(db);

  dcam *v = NULL;
  int n = 0;
  if (load_frames(db, &v, &n) != 0) { free(v); return err_json(status, 500, "server_error"); }
  load_items(db, v, n);

  /* the eligible cameras, by pointer, for two orderings */
  dcam **e = calloc((size_t)(n ? n : 1), sizeof *e);
  int ne = 0, with_vec = 0, with_ph = 0, no_geo = 0;
  /* vectors are LOADED only for the pairs that need one; this only counts */
  sqlite3_stmt *hv = NULL;
  if (vecindex_table_exists(db, "image_vec_done"))
    sqlite3_prepare_v2(db->h, "SELECT 1 FROM image_vec_done WHERE sha256=?1",
                       -1, &hv, NULL);
  for (int i = 0; e && i < n; i++) {
    if (!v[i].ok) continue;
    e[ne++] = &v[i];
    with_ph += v[i].has_ph;
    no_geo += !v[i].has_geo;
    if (hv) {
      sqlite3_reset(hv);
      sqlite3_bind_text(hv, 1, v[i].sha, -1, SQLITE_TRANSIENT);
      with_vec += sqlite3_step(hv) == SQLITE_ROW;
    }
  }
  if (hv) sqlite3_finalize(hv);
  dpair *p = NULL;
  int np = 0, pcap = 0;
  cJSON *skipped = cJSON_CreateArray();
  long n_skipped = 0;

  /* 1. byte-identical latest frames, grouped by sha */
  if (e && ne > 1) {
    qsort(e, (size_t)ne, sizeof *e, by_sha);
    for (int i = 0; i < ne;) {
      int j = i + 1;
      while (j < ne && !strcmp(e[j]->sha, e[i]->sha)) j++;
      int g = j - i;
      if (g > CAM_DUP_SHA_GROUP_MAX) {
        n_skipped++;
        if (cJSON_GetArraySize(skipped) < DUP_SKIPPED_MAX) {
          cJSON *s = cJSON_CreateObject();
          cJSON_AddStringToObject(s, "sha256", e[i]->sha);
          cJSON_AddNumberToObject(s, "cameras", g);
          cJSON_AddItemToArray(skipped, s);
        }
      } else if (g > 1) {
        for (int a = i; a < j; a++)
          for (int b = a + 1; b < j; b++) {
            if (!strcmp(e[a]->item, e[b]->item)) continue;   /* views of one record */
            dpair x = { (int)(e[a] - v), (int)(e[b] - v), SIG_SHA, NULL, NULL, -1, NAN, NAN };
            if (e[a]->has_geo && e[b]->has_geo)
              x.dist = haversine_m(e[a]->lat, e[a]->lon, e[b]->lat, e[b]->lon);
            if (e[a]->has_ph && e[b]->has_ph) x.phd = media_phash_distance(e[a]->ph, e[b]->ph);
            if (vec_of(db, e[a]) && vec_of(db, e[b]) && e[a]->dim == e[b]->dim)
              x.cos = vec_cosine(e[a]->vec, e[b]->vec, e[a]->dim);
            if (push(&p, &np, &pcap, x) != 0) break;
          }
      }
      i = j;
    }
  }

  /* 2. pHash, then cosine, between cameras within the radius */
  if (e && ne > 1) {
    qsort(e, (size_t)ne, sizeof *e, by_lat);
    double dlat = CAM_DUP_RADIUS_M / 111320.0;
    for (int i = 0; i < ne; i++) {
      if (!e[i]->has_geo) continue;
      for (int j = i + 1; j < ne; j++) {
        if (!e[j]->has_geo) continue;
        if (e[j]->lat - e[i]->lat > dlat) break;      /* sorted: none further */
        if (!strcmp(e[i]->item, e[j]->item)) continue;
        if (!strcmp(e[i]->sha, e[j]->sha)) continue;  /* step 1's, or a placeholder */
        double d = haversine_m(e[i]->lat, e[i]->lon, e[j]->lat, e[j]->lon);
        if (d > CAM_DUP_RADIUS_M) continue;
        dpair x = { (int)(e[i] - v), (int)(e[j] - v), -1, NULL, NULL, -1, NAN, d };
        if (e[i]->has_ph && e[j]->has_ph) x.phd = media_phash_distance(e[i]->ph, e[j]->ph);
        if (x.phd >= 0 && x.phd <= CAM_DUP_PHASH_MAX) x.sig = SIG_PHASH;
        else {
          /* no pHash verdict: the embedding decides (both measurements are
           * reported as evidence either way) */
          if (vec_of(db, e[i]) && vec_of(db, e[j]) && e[i]->dim == e[j]->dim)
            x.cos = vec_cosine(e[i]->vec, e[j]->vec, e[i]->dim);
          if (!isnan(x.cos) && x.cos >= CAM_DUP_COSINE_MIN) x.sig = SIG_COSINE;
        }
        if (x.sig >= 0 && push(&p, &np, &pcap, x) != 0) break;
      }
    }
  }
  /* a pair keeps its first camera first in a stable order */
  for (int i = 0; i < np; i++) {
    if (strcmp(v[p[i].a].cam, v[p[i].b].cam) > 0) { int t = p[i].a; p[i].a = p[i].b; p[i].b = t; }
    p[i].ca = v[p[i].a].cam;
    p[i].cb = v[p[i].b].cam;
  }
  if (np > 1) qsort(p, (size_t)np, sizeof *p, pair_cmp);

  cJSON *data = cJSON_CreateArray();
  int count = 0;
  for (int i = offset; i < np && count < limit; i++, count++) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "signal", SIG_NAME[p[i].sig]);
    cJSON *cs = cJSON_AddArrayToObject(o, "cameras");
    cJSON_AddItemToArray(cs, cam_json(&v[p[i].a]));
    cJSON_AddItemToArray(cs, cam_json(&v[p[i].b]));
    cJSON *ev = cJSON_AddObjectToObject(o, "evidence");
    cJSON_AddBoolToObject(ev, "same_sha256", !strcmp(v[p[i].a].sha, v[p[i].b].sha));
    if (p[i].phd >= 0) cJSON_AddNumberToObject(ev, "phash_distance", p[i].phd);
    else               cJSON_AddNullToObject(ev, "phash_distance");
    if (!isnan(p[i].cos)) cJSON_AddNumberToObject(ev, "cosine", p[i].cos);
    else                  cJSON_AddNullToObject(ev, "cosine");
    if (!isnan(p[i].dist)) cJSON_AddNumberToObject(ev, "distance_m", round(p[i].dist));
    else                   cJSON_AddNullToObject(ev, "distance_m");
    cJSON_AddItemToArray(data, o);
  }

  cJSON *root = cJSON_CreateObject();
  cJSON_AddItemToObject(root, "data", data);
  cJSON *pg = cJSON_AddObjectToObject(root, "page");
  cJSON_AddNumberToObject(pg, "limit", limit);
  cJSON_AddNumberToObject(pg, "offset", offset);
  cJSON_AddNumberToObject(pg, "count", count);
  cJSON_AddNumberToObject(pg, "total", np);
  cJSON_AddBoolToObject(pg, "has_more", offset + count < np);
  cJSON *meta = cJSON_AddObjectToObject(root, "meta");
  cJSON *th = cJSON_AddObjectToObject(meta, "thresholds");
  cJSON_AddNumberToObject(th, "phash_max_distance", CAM_DUP_PHASH_MAX);
  cJSON_AddNumberToObject(th, "cosine_min", CAM_DUP_COSINE_MIN);
  cJSON_AddNumberToObject(th, "radius_m", CAM_DUP_RADIUS_M);
  cJSON_AddNumberToObject(th, "sha_group_max", CAM_DUP_SHA_GROUP_MAX);
  cJSON_AddNumberToObject(meta, "cameras_examined", ne);
  cJSON_AddNumberToObject(meta, "cameras_excluded_by_policy", n - ne);
  cJSON_AddNumberToObject(meta, "cameras_with_vector", with_vec);
  cJSON_AddNumberToObject(meta, "cameras_with_phash", with_ph);
  cJSON_AddNumberToObject(meta, "cameras_without_location", no_geo);
  cJSON_AddItemToObject(meta, "shared_frames_skipped", skipped);
  cJSON_AddNumberToObject(meta, "shared_frames_skipped_total", (double)n_skipped);
  cJSON_AddStringToObject(meta, "note", "proposals only: nothing is merged or "
                          "hidden. Latest frame per camera; pHash/cosine pairs "
                          "only within radius_m; public-infrastructure cameras only");

  for (int i = 0; i < n; i++) dcam_free(&v[i]);
  free(v); free(e); free(p);
  char *out = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  *status = out ? 200 : 500;
  return out;
}
