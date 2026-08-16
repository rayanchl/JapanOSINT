/* collectors/sources/av_common.inc — shared static helpers for the av_*
 * aviation batch. Included, never compiled standalone (the Makefile globs
 * *.c, not *.inc — same trick as _jp_osint.inc / username_osint_platforms.inc).
 *
 * Every includer must already have pulled in <stdio.h>, <stdlib.h>, <string.h>
 * and <ctype.h>. Everything here is `static inline`, so each translation unit
 * gets its own copy (no duplicate symbols, no -Wunused-function noise).
 *
 * House rules honoured by every helper: nothing here invents a record, a name
 * or a coordinate. av_coords_geom() returns NULL when the upstream gave no
 * usable vertex — callers then emit the row with has_geo=0 rather than falling
 * back to an issuing station, a state centroid or the query point (R2). */
#ifndef JO_AV_COMMON_INC
#define JO_AV_COMMON_INC

#include <time.h>
#include "lib/geojson.h"
#include "_jp_osint.inc"

#define AV_UA_HDR "User-Agent: JapanOSINT/1.0 (aviation collector)"

/* Number-or-numeric-string field → double.
 * TRAP (quoted from the work list): the AWC products disagree on the JSON type
 * of a coordinate — isigmet/airsigmet/pirep return coords[].lat/lon as JSON
 * NUMBERS, cwa/gairmet return the same values as STRINGS. Both are real
 * upstream values; only the encoding differs. */
static inline int av_dbl(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return 0;
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *e = NULL;
    double d = strtod(v->valuestring, &e);
    if (e && e != v->valuestring) { *out = d; return 1; }
  }
  return 0;
}

/* Copy a JSON string/number field into a cJSON object only when it is really
 * present (never writes a placeholder). */
static inline void av_copy(cJSON *dst, const cJSON *src, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(src, k);
  if (!v || cJSON_IsNull(v)) return;
  if (cJSON_IsString(v)) {
    if (v->valuestring && v->valuestring[0])
      cJSON_AddStringToObject(dst, k, v->valuestring);
  } else if (cJSON_IsNumber(v)) {
    cJSON_AddNumberToObject(dst, k, v->valuedouble);
  } else if (cJSON_IsBool(v)) {
    cJSON_AddBoolToObject(dst, k, cJSON_IsTrue(v));
  }
}

/* ADS-B `flight` is a fixed-width 8-char, space-padded field ("TACO74  ").
 * Trim into buf; returns buf or NULL when the field is empty. */
static inline const char *av_trim(const char *s, char *buf, size_t n) {
  if (!s) return NULL;
  while (*s == ' ' || *s == '\t') s++;
  size_t L = strlen(s);
  while (L && (s[L - 1] == ' ' || s[L - 1] == '\t')) L--;
  if (!L) return NULL;
  if (L >= n) L = n - 1;
  memcpy(buf, s, L);
  buf[L] = 0;
  return buf;
}

/* Trim a fixed-width padded string in place (FAA/AWC pad heavily). */
static inline char *av_rtrim(char *s) {
  if (!s) return s;
  size_t L = strlen(s);
  while (L && (s[L - 1] == ' ' || s[L - 1] == '\t')) s[--L] = 0;
  return s;
}

/* "lat,lon" (optionally "lat,lon,extra") pivot entity → doubles.
 * Returns 1 on success; *extra is set only when a third component parses. */
static inline int av_latlon(const char *s, double *lat, double *lon,
                            double *extra) {
  if (!s || !*s) return 0;
  char buf[160];
  snprintf(buf, sizeof buf, "%.159s", s);
  char *c1 = strchr(buf, ',');
  if (!c1) return 0;
  *c1 = 0;
  char *rest = c1 + 1;
  char *c2 = strchr(rest, ',');
  if (c2) *c2 = 0;
  char *a = buf;   while (*a == ' ') a++;
  char *b = rest;  while (*b == ' ') b++;
  char *ea = NULL, *eb = NULL;
  double la = strtod(a, &ea), lo = strtod(b, &eb);
  if (ea == a || eb == b) return 0;
  if (la < -90 || la > 90 || lo < -180 || lo > 180) return 0;
  *lat = la; *lon = lo;
  if (extra && c2) {
    char *d = c2 + 1; while (*d == ' ') d++;
    char *ed = NULL;
    double x = strtod(d, &ed);
    if (ed != d) *extra = x;
  }
  return 1;
}

/* Epoch seconds → "YYYY-MM-DDTHH:MM:SSZ". Returns out, or NULL when t<=0. */
static inline const char *av_epoch_iso(double secs, char *out, size_t n) {
  if (!(secs > 0)) return NULL;
  time_t t = (time_t)secs;
  struct tm tmv;
#if defined(_WIN32)
  if (gmtime_s(&tmv, &t) != 0) return NULL;
#else
  if (!gmtime_r(&t, &tmv)) return NULL;
#endif
  if (strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &tmv) == 0) return NULL;
  return out;
}

/* AWC hazard vertex list (`coords`: [{lat,lon}, …]) → a real GeoJSON geometry.
 * `geom` is the product's own AREA/LINE/POINT hint. Returns NULL when no
 * vertex parsed — the caller must then emit with has_geo=0 (R2: a text-only
 * bulletin gets no geometry, never the FIR centroid or the issuing office). */
static inline cJSON *av_coords_geom(const cJSON *coords, const char *geom) {
  if (!cJSON_IsArray(coords)) return NULL;
  cJSON *pts = cJSON_CreateArray();
  int used = 0;
  double flat = 0, flon = 0, llat = 0, llon = 0;
  const cJSON *c;
  cJSON_ArrayForEach(c, coords) {
    double la, lo;
    if (!av_dbl(c, "lat", &la) || !av_dbl(c, "lon", &lo)) continue;
    if (la < -90 || la > 90 || lo < -180 || lo > 180) continue;
    cJSON *p = cJSON_CreateArray();
    cJSON_AddItemToArray(p, cJSON_CreateNumber(lo));   /* GeoJSON is lon,lat */
    cJSON_AddItemToArray(p, cJSON_CreateNumber(la));
    cJSON_AddItemToArray(pts, p);
    if (!used) { flat = la; flon = lo; }
    llat = la; llon = lo;
    used++;
  }
  if (used == 0) { cJSON_Delete(pts); return NULL; }

  cJSON *g = cJSON_CreateObject();
  int area = (geom && strcmp(geom, "AREA") == 0);
  if (used == 1) {
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *only = cJSON_DetachItemFromArray(pts, 0);
    cJSON_Delete(pts);
    cJSON_AddItemToObject(g, "coordinates", only);
    return g;
  }
  if (area && used >= 3) {
    if (flat != llat || flon != llon) {        /* close the ring if it is open */
      cJSON *p = cJSON_CreateArray();
      cJSON_AddItemToArray(p, cJSON_CreateNumber(flon));
      cJSON_AddItemToArray(p, cJSON_CreateNumber(flat));
      cJSON_AddItemToArray(pts, p);
    }
    cJSON *rings = cJSON_CreateArray();
    cJSON_AddItemToArray(rings, pts);
    cJSON_AddStringToObject(g, "type", "Polygon");
    cJSON_AddItemToObject(g, "coordinates", rings);
    return g;
  }
  cJSON_AddStringToObject(g, "type", "LineString");
  cJSON_AddItemToObject(g, "coordinates", pts);
  return g;
}

/* Build one GeoJSON Feature shell. `geom` is adopted (may be NULL → the
 * Feature carries "geometry": null, which lib/geojson.c treats as no
 * location). `props` is adopted. */
static inline cJSON *av_feature(cJSON *geom, cJSON *props) {
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  if (geom) cJSON_AddItemToObject(f, "geometry", geom);
  else      cJSON_AddNullToObject(f, "geometry");
  cJSON_AddItemToObject(f, "properties", props);
  return f;
}

/* ---------------------------------------------------------------- ArcGIS --
 * FAA Aeronautical Information Services publishes every layer below on one
 * ArcGIS Online org. TRAP (quoted from the work list): "ArcGIS FeatureServer
 * layers need f=geojson and outSR=4326 or you get Web Mercator metres, which
 * as lat/lon land nowhere near the asset." Both are pinned on every request.
 *
 * Second trap: the server sets properties.exceededTransferLimit=true whenever
 * a page was truncated — a single call is NEVER the whole layer. We page on
 * resultOffset/resultRecordCount until the server stops setting it, the page
 * comes back empty, or the per-source cap is reached (the cap is documented in
 * each source's header comment and logged, never silent). */
#define AV_ARC_BASE \
  "https://services6.arcgis.com/ssFJjBXIUyZDrSYZ/ArcGIS/rest/services/"
/* 1000, not the server's 2000 default: the high-vertex polygon layers
 * (Class_Airspace, Special_Use_Airspace) push multi-MB pages that can outrun
 * the HTTP timeout at 2000. */
#define AV_ARC_PAGE 1000

/* Called once per feature with its (mutable) properties object, so each layer
 * can add a real title/uid built from fields the server actually returned. */
typedef void (*av_arc_decor)(cJSON *props);

/* Returns #emitted, or -1 if the FIRST page could not be fetched/parsed. */
static inline int av_arcgis_run(const source_ctx *ctx, intel_sink *sink,
                                const char *service, int cap,
                                av_arc_decor decor, const char *tag) {
  int total = 0, offset = 0, page_no = 0, capped = 0;
  for (;;) {
    char url[768];
    snprintf(url, sizeof url,
             AV_ARC_BASE "%s/FeatureServer/0/query?where=1%%3D1&outFields=*"
             "&f=geojson&outSR=4326&resultOffset=%d&resultRecordCount=%d",
             service, offset, AV_ARC_PAGE);
    const char *hdrs[] = { "Accept: application/json", AV_UA_HDR, NULL };
    char *body = jo_get(ctx, url, hdrs, tag);
    if (!body) return page_no == 0 ? -1 : total;   /* R3: only a real fetch
                                                    * failure is an error */
    cJSON *doc = cJSON_Parse(body);
    free(body);
    if (!doc) return page_no == 0 ? -1 : total;

    cJSON *feats = cJSON_GetObjectItem(doc, "features");
    int nfeat = cJSON_IsArray(feats) ? cJSON_GetArraySize(feats) : 0;
    if (nfeat == 0) { cJSON_Delete(doc); break; }

    if (decor) {
      cJSON *f;
      cJSON_ArrayForEach(f, feats) {
        cJSON *p = cJSON_GetObjectItem(f, "properties");
        if (cJSON_IsObject(p)) decor(p);
      }
    }
    total += geojson_emit_features(sink, ctx->source_id, feats);

    cJSON *dp = cJSON_GetObjectItem(doc, "properties");
    cJSON *ex = dp ? cJSON_GetObjectItem(dp, "exceededTransferLimit") : NULL;
    int more = (ex && cJSON_IsTrue(ex));
    cJSON_Delete(doc);
    page_no++;
    offset += nfeat;
    if (!more) break;
    if (total >= cap) { capped = 1; break; }
  }
  if (capped)
    fprintf(stderr, "[%s] emitted %d — PER-RUN CAP reached, layer has more "
                    "(documented in the file header)\n", tag, total);
  else
    fprintf(stderr, "[%s] emitted %d (complete sweep)\n", tag, total);
  return total;
}

/* ------------------------------------------------------------ bbox sweep --
 * Several AWC endpoints are bbox queries that silently cap their result set
 * (~50 rows for /airport, ~400 for /stationinfo, /obstacle, /pirep). Quoting
 * the work list: "page by tiling the bbox, not by assuming full coverage."
 * This walks a quadtree: a tile that comes back AT the cap is split into four
 * and re-queried, so dense regions get resolved and empty ocean costs one
 * call. `budget` bounds the total number of HTTP requests per run. */
/* The callback fetches ONE tile, emits whatever it parsed, and returns the
 * number of records the UPSTREAM returned (not the number emitted) so the
 * at-the-cap test is honest; -1 means that tile's fetch failed. */
typedef int (*av_bbox_cb)(const source_ctx *, intel_sink *, double minLat,
                          double minLon, double maxLat, double maxLon,
                          void *user);

typedef struct { double a, b, c, d; int depth; } av_tile;
#define AV_TILE_QUEUE 512

/* Breadth-first refinement: every tile at the current resolution is queried
 * before any tile is split, so exhausting `budget` degrades resolution
 * UNIFORMLY instead of leaving whole regions unvisited. Returns the number of
 * HTTP requests actually spent; *ok is set to 0 if the very first tile failed
 * (i.e. the endpoint itself is down → the caller may report -1). */
static inline int av_bbox_sweep(const source_ctx *ctx, intel_sink *sink,
                                double minLat, double minLon, double maxLat,
                                double maxLon, int maxdepth, int cap,
                                int budget, av_bbox_cb cb, void *user,
                                int *ok) {
  av_tile q[AV_TILE_QUEUE];
  int head = 0, tail = 0, spent = 0;
  q[tail].a = minLat; q[tail].b = minLon; q[tail].c = maxLat; q[tail].d = maxLon;
  q[tail].depth = 0; tail++;
  if (ok) *ok = 1;
  while (head < tail && spent < budget) {
    av_tile t = q[head++];
    int n = cb(ctx, sink, t.a, t.b, t.c, t.d, user);
    spent++;
    if (n < 0) { if (spent == 1 && ok) *ok = 0; continue; }
    if (n < cap || t.depth >= maxdepth) continue;
    double mLat = (t.a + t.c) / 2, mLon = (t.b + t.d) / 2;
    const double ch[4][4] = {
      { t.a, t.b, mLat, mLon }, { t.a, mLon, mLat, t.d },
      { mLat, t.b, t.c, mLon }, { mLat, mLon, t.c, t.d } };
    for (int i = 0; i < 4 && tail < AV_TILE_QUEUE; i++) {
      q[tail].a = ch[i][0]; q[tail].b = ch[i][1];
      q[tail].c = ch[i][2]; q[tail].d = ch[i][3];
      q[tail].depth = t.depth + 1; tail++;
    }
  }
  return spent;
}

#endif /* JO_AV_COMMON_INC */
