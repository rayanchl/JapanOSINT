#include "geojson.h"
#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>

/* collectorMirror.NATIVE_ID_KEYS, exact order. */
static const char *NATIVE_ID_KEYS[] = {
  "camera_uid","station_uid","line_uid","cluster_uid","footprint_id",
  "post_uid","event_id","earthquake_id","buoy_id","station_id",
  "spring_id","stop_id","uid","id","uuid", NULL };

static const char *prop_str(cJSON *props, const char *k) {
  cJSON *v = cJSON_GetObjectItem(props, k);
  if (!v) return NULL;
  if (cJSON_IsString(v) && v->valuestring[0]) return v->valuestring;
  return NULL;
}

/* pickText(props, keys...) — first non-empty trimmed STRING. */
static const char *pick_text(cJSON *props, const char *const *keys) {
  for (int i = 0; keys[i]; i++) {
    cJSON *v = cJSON_GetObjectItem(props, keys[i]);
    if (v && cJSON_IsString(v)) {
      const char *s = v->valuestring;
      while (*s==' '||*s=='\t'||*s=='\n'||*s=='\r') s++;
      if (*s) return v->valuestring; /* Node returns trimmed; trim on use */
    }
  }
  return NULL;
}

/* JS String(v): numbers without quotes, strings raw. For uid we only hit
 * NATIVE keys/id which are strings or numbers. */
static void id_to_str(cJSON *v, char *out, size_t n) {
  if (cJSON_IsString(v)) snprintf(out, n, "%s", v->valuestring);
  else if (cJSON_IsNumber(v)) {
    /* Bound-check before the cast: (long long)d is undefined outside long
     * long's range, and a feature id of 1e308 is all it takes (UBSan flagged
     * exactly this here). (double)LLONG_MAX is exactly 2^63, so `<` excludes
     * the boundary; NaN fails the comparisons and falls through to %g. */
    double d = v->valuedouble;
    if (d >= (double)LLONG_MIN && d < (double)LLONG_MAX &&
        d == (double)(long long)d)
      snprintf(out, n, "%lld", (long long)d);
    else snprintf(out, n, "%g", d);
  } else out[0] = 0;
}

static void sha1_hex16(const char *s, char *out17) {
  unsigned char d[20]; SHA1((const unsigned char *)s, strlen(s), d);
  for (int i = 0; i < 8; i++) sprintf(out17 + i*2, "%02x", d[i]);
  out17[16] = 0;
}

/* featureUid: NATIVE_ID_KEYS → feature.id → sourceId|h:sha1(JSON.stringify
 * {g:geometry,p:props})[:16]. cJSON_PrintUnformatted preserves source key
 * order (like JS), so the fingerprint matches for the hash-fallback minority
 * (most features carry a natural id; per-source parity tests flag any drift).
 *
 * THE HASH FALLBACK IS A TRAP, and it was silent (audit defect #8). Because
 * the fingerprint covers `properties`, a collector that carries a CHANGING
 * MEASUREMENT there mints a brand-new uid on every run: the upsert stops
 * upserting and the table grows without bound, with no error anywhere.
 * hudson_rock_jp.c hit exactly this the moment it started carrying live
 * credential counts, and fixed it in the collector by adding a stable `uid`
 * property — which is the right fix, but nothing told the next author.
 *
 * The fix cannot be "hash something else": every existing hash-fallback row in
 * the database is keyed by THIS fingerprint, so changing it re-keys the fleet
 * and causes the very unbounded growth it is meant to prevent. What was
 * missing was the signal. *hash_fallbacks (may be NULL) counts the features
 * that landed here, and geojson_emit_features reports the count once per run
 * so a source in this state is visible instead of silent. A collector whose
 * count is non-zero AND whose properties change between runs must add one of
 * NATIVE_ID_KEYS (`uid` is the conventional choice) to its properties. */
static void feature_uid(cJSON *feat, const char *sid, char *out, size_t n,
                        int *hash_fallbacks) {
  cJSON *props = cJSON_GetObjectItem(feat, "properties");
  if (props) {
    for (int i = 0; NATIVE_ID_KEYS[i]; i++) {
      cJSON *v = cJSON_GetObjectItem(props, NATIVE_ID_KEYS[i]);
      if (v && !cJSON_IsNull(v)) {
        char idv[256]; id_to_str(v, idv, sizeof idv);
        if (idv[0]) { snprintf(out, n, "%s|%s", sid, idv); return; }
      }
    }
  }
  cJSON *fid = cJSON_GetObjectItem(feat, "id");
  if (fid && !cJSON_IsNull(fid)) {
    char idv[256]; id_to_str(fid, idv, sizeof idv);
    if (idv[0]) { snprintf(out, n, "%s|%s", sid, idv); return; }
  }
  cJSON *g = cJSON_GetObjectItem(feat, "geometry");
  cJSON *fp = cJSON_CreateObject();
  cJSON_AddItemToObject(fp, "g", g ? cJSON_Duplicate(g, 1) : cJSON_CreateNull());
  cJSON_AddItemToObject(fp, "p", props ? cJSON_Duplicate(props, 1) : cJSON_CreateObject());
  char *s = cJSON_PrintUnformatted(fp);
  char h[17]; sha1_hex16(s ? s : "", h);
  free(s); cJSON_Delete(fp);
  if (hash_fallbacks) (*hash_fallbacks)++;
  snprintf(out, n, "%s|h:%s", sid, h);
}

/* geometryCentroid → sets *lat,*lon; returns 1 if usable.
 *
 * The stored lat/lon is the ONLY coordinate the alerting/geofence read path
 * sees (core/alert_eval.c facts_from_row selects lat/lon and never the
 * geometry column), so the representative point must at least plausibly sit
 * on the feature. The bbox CENTRE does not: for an L-shaped or otherwise
 * concave polygon, and for a curved LineString, it can land well off the
 * geometry. So:
 *   - LineString / MultiLineString → midpoint of the MIDDLE SEGMENT, which is
 *     always on the line (mirrors GEO_MIDPOINT in lib/mlit_ksj.c).
 *   - Polygon / MultiPolygon → mean of the OUTER RING's vertices (mirrors
 *     GEO_CENTROID in lib/mlit_ksj.c), clamped back onto a real vertex if the
 *     mean somehow falls outside the ring's own bbox.
 *   - everything else (MultiPoint, unknown types) keeps the old bbox centre.
 * NOTE this is still a single representative point, not the full geometry:
 * a polygon-vertex mean can itself sit outside a strongly concave ring, so
 * true polygon-aware geofencing remains a follow-up on the read side. */
static void visit(cJSON *node, double *mnx, double *mxx, double *mny,
                   double *mxy, int *cnt) {
  if (!cJSON_IsArray(node)) return;
  cJSON *a = cJSON_GetArrayItem(node, 0);
  cJSON *b = cJSON_GetArrayItem(node, 1);
  if (a && b && cJSON_IsNumber(a) && cJSON_IsNumber(b) &&
      isfinite(a->valuedouble) && isfinite(b->valuedouble)) {
    if (a->valuedouble < *mnx) *mnx = a->valuedouble;
    if (a->valuedouble > *mxx) *mxx = a->valuedouble;
    if (b->valuedouble < *mny) *mny = b->valuedouble;
    if (b->valuedouble > *mxy) *mxy = b->valuedouble;
    (*cnt)++; return;
  }
  cJSON *ch; cJSON_ArrayForEach(ch, node) visit(ch, mnx, mxx, mny, mxy, cnt);
}

/* One [x,y] position → finite doubles; 0 if the position is malformed. */
static int coord_xy(cJSON *pt, double *x, double *y) {
  cJSON *a = cJSON_GetArrayItem(pt, 0), *b = cJSON_GetArrayItem(pt, 1);
  if (!a || !b || !cJSON_IsNumber(a) || !cJSON_IsNumber(b) ||
      !isfinite(a->valuedouble) || !isfinite(b->valuedouble)) return 0;
  *x = a->valuedouble; *y = b->valuedouble; return 1;
}

/* Midpoint of the middle segment of one line (array of positions). Always a
 * point ON the line. A 1-vertex line degenerates to that vertex; a malformed
 * middle segment falls back to the first usable vertex. */
static int line_point(cJSON *line, double *lon, double *lat) {
  if (!cJSON_IsArray(line)) return 0;
  int n = cJSON_GetArraySize(line);
  if (n <= 0) return 0;
  if (n > 1) {
    int m = (n - 1) / 2;                        /* segment [m, m+1] */
    double x0, y0, x1, y1;
    if (coord_xy(cJSON_GetArrayItem(line, m), &x0, &y0) &&
        coord_xy(cJSON_GetArrayItem(line, m + 1), &x1, &y1)) {
      *lon = (x0 + x1) / 2; *lat = (y0 + y1) / 2; return 1;
    }
  }
  for (int i = 0; i < n; i++)
    if (coord_xy(cJSON_GetArrayItem(line, i), lon, lat)) return 1;
  return 0;
}

/* Mean of a ring's vertices, clamped back onto the first usable vertex if the
 * mean lands outside the ring's own bbox (a guard against NaN/overflow —
 * a vertex mean is otherwise inside the hull by construction). The closing
 * vertex is included, matching GEO_CENTROID in lib/mlit_ksj.c. */
static int ring_point(cJSON *ring, double *lon, double *lat) {
  if (!cJSON_IsArray(ring)) return 0;
  int n = cJSON_GetArraySize(ring);
  if (n <= 0) return 0;
  double sx = 0, sy = 0, fx = 0, fy = 0;
  double mnx = 0, mxx = 0, mny = 0, mxy = 0;
  int used = 0;
  for (int i = 0; i < n; i++) {
    double x, y;
    if (!coord_xy(cJSON_GetArrayItem(ring, i), &x, &y)) continue;
    if (used == 0) { mnx = mxx = fx = x; mny = mxy = fy = y; }
    else {
      if (x < mnx) mnx = x;
      if (x > mxx) mxx = x;
      if (y < mny) mny = y;
      if (y > mxy) mxy = y;
    }
    sx += x; sy += y; used++;
  }
  if (used == 0) return 0;
  double cx = sx / used, cy = sy / used;
  if (!isfinite(cx) || !isfinite(cy) ||
      cx < mnx || cx > mxx || cy < mny || cy > mxy) { cx = fx; cy = fy; }
  *lon = cx; *lat = cy; return 1;
}

static int centroid(cJSON *geom, double *lat, double *lon) {
  if (!geom) return 0;
  cJSON *t = cJSON_GetObjectItem(geom, "type");
  cJSON *c = cJSON_GetObjectItem(geom, "coordinates");
  if (!t || !cJSON_IsString(t) || !c) return 0;
  const char *ty = t->valuestring;

  if (strcmp(ty, "Point") == 0) {
    cJSON *x = cJSON_GetArrayItem(c, 0), *y = cJSON_GetArrayItem(c, 1);
    if (x && y && cJSON_IsNumber(x) && cJSON_IsNumber(y) &&
        isfinite(x->valuedouble) && isfinite(y->valuedouble)) {
      *lon = x->valuedouble; *lat = y->valuedouble; return 1;
    }
    return 0;
  }

  /* On-geometry representative points; each falls through to the bbox centre
   * below only when the geometry is too malformed to yield one. */
  if (strcmp(ty, "LineString") == 0) {
    if (line_point(c, lon, lat)) return 1;
  } else if (strcmp(ty, "MultiLineString") == 0) {
    int nl = cJSON_IsArray(c) ? cJSON_GetArraySize(c) : 0;
    for (int i = 0; i < nl; i++)
      if (line_point(cJSON_GetArrayItem(c, i), lon, lat)) return 1;
  } else if (strcmp(ty, "Polygon") == 0) {
    if (ring_point(cJSON_GetArrayItem(c, 0), lon, lat)) return 1;
  } else if (strcmp(ty, "MultiPolygon") == 0) {
    int np = cJSON_IsArray(c) ? cJSON_GetArraySize(c) : 0;
    for (int i = 0; i < np; i++) {
      cJSON *poly = cJSON_GetArrayItem(c, i);
      if (ring_point(cJSON_GetArrayItem(poly, 0), lon, lat)) return 1;
    }
  }

  double mnx=1e308,mxx=-1e308,mny=1e308,mxy=-1e308; int cnt=0;
  visit(c, &mnx,&mxx,&mny,&mxy,&cnt);
  if (cnt == 0) return 0;
  *lon = (mnx+mxx)/2; *lat = (mny+mxy)/2; return 1;
}

static const char *T_TITLE[]  = {"title","name","name_ja","label",NULL};
static const char *T_SUMM[]   = {"summary","description","desc",NULL};
static const char *T_LINK[]   = {"link","url","href",NULL};
static const char *T_AUTH[]   = {"author","operator",NULL};
static const char *T_LANG[]   = {"language","lang",NULL};
static const char *T_PUB[]    = {"published_at","observed_at","time","timestamp",NULL};

int geojson_emit_features(intel_sink *sink, const char *sid, cJSON *features) {
  if (!cJSON_IsArray(features)) return 0;
  int n = 0, hashed = 0; cJSON *feat;
  cJSON_ArrayForEach(feat, features) {
    if (!cJSON_IsObject(feat)) continue;
    cJSON *props = cJSON_GetObjectItem(feat, "properties");
    cJSON *geom  = cJSON_GetObjectItem(feat, "geometry");
    char uid[600]; feature_uid(feat, sid, uid, sizeof uid, &hashed);
    double lat=0, lon=0; int geo = centroid(geom, &lat, &lon);

    const char *rt = props ? prop_str(props, "record_type") : NULL;
    if (!rt && props) rt = prop_str(props, "kind");
    if (!rt) rt = sid;
    const char *sub = props ? prop_str(props, "sub_source_id") : NULL;
    if (!sub && props) sub = prop_str(props, "channel");

    /* A GeoJSON Feature may carry `"geometry": null` — that is the spec's way
     * of saying "no location", and several collectors now emit it deliberately
     * where they used to invent a coordinate. Printing it yields the 4-byte
     * string "null", which passes every `geometry IS NOT NULL AND geometry<>''`
     * test downstream, so the row still reads as pinned. Treat null as absent. */
    char *gj = (geom && !cJSON_IsNull(geom)) ? cJSON_PrintUnformatted(geom) : NULL;
    char *pj = props ? cJSON_PrintUnformatted(props) : NULL;
    cJSON *tagsv = props ? cJSON_GetObjectItem(props, "tags") : NULL;
    char *tj = (tagsv && cJSON_IsArray(tagsv)) ? cJSON_PrintUnformatted(tagsv) : NULL;

    intel_item it = {0};
    it.uid = uid;
    it.title       = props ? pick_text(props, T_TITLE) : NULL;
    it.summary     = props ? pick_text(props, T_SUMM)  : NULL;
    it.link        = props ? pick_text(props, T_LINK)  : NULL;
    it.author      = props ? pick_text(props, T_AUTH)  : NULL;
    it.lang        = props ? pick_text(props, T_LANG)  : NULL;
    it.published_at= props ? pick_text(props, T_PUB)   : NULL;
    it.record_type = rt;
    it.sub_source_id = sub;
    it.has_geo = geo; it.lat = lat; it.lon = lon;
    it.geometry_geojson = gj;
    it.properties_json = pj ? pj : "{}";
    it.tags_json = tj ? tj : "[]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(gj); free(pj); free(tj);
  }
  /* See feature_uid(): these rows are keyed by a hash of their own contents,
   * so if this source's properties carry a changing measurement the row count
   * grows every run instead of the rows being updated. One line, once per
   * run, is what turns that from invisible into checkable. */
  if (hashed)
    fprintf(stderr, "[%s] %d/%d features had no native id — uid'd by content "
                    "hash (add a stable `uid` property if these change)\n",
            sid, hashed, n);
  return n;
}

cJSON *gj_point_feature(double lon, double lat) {
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);
  return f;
}

int geojson_emit_doc(intel_sink *sink, const char *sid, cJSON *doc) {
  if (!doc) return 0;
  if (cJSON_IsArray(doc)) return geojson_emit_features(sink, sid, doc);
  cJSON *f = cJSON_GetObjectItem(doc, "features");
  return f ? geojson_emit_features(sink, sid, f) : 0;
}
