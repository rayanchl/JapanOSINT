/* lib/overpass.c — port of _liveHelpers.js Overpass helpers. See overpass.h. */
#include "overpass.h"
#include "geojson.h"
#include "../core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* OVERPASS_ENDPOINTS, exact order. */
static const char *ENDPOINTS[] = {
  "https://overpass-api.de/api/interpreter",
  "https://overpass.kumi.systems/api/interpreter",
  "https://overpass.openstreetmap.ru/api/interpreter",
  "https://overpass.private.coffee/api/interpreter",
};
#define N_ENDPOINTS 4

/* JAPAN_TILES [south,west,north,east], exact values from _liveHelpers.js. */
static const double TILES[12][4] = {
  {41.35, 139.33, 45.55, 145.85}, {41.35, 139.33, 45.55, 148.00},
  {37.73, 139.33, 41.60, 142.05}, {34.55, 138.50, 37.80, 141.30},
  {33.80, 135.20, 37.80, 138.60}, {33.10, 131.90, 35.50, 135.30},
  {31.50, 129.80, 34.40, 132.40}, {31.20, 130.20, 33.80, 132.30},
  {32.50, 132.00, 34.60, 134.80}, {24.00, 122.80, 28.50, 131.60},
  {26.00, 140.80, 35.00, 142.30}, {33.00, 128.00, 36.00, 130.50},
};

/* encodeURIComponent: keep A-Za-z0-9 and - _ . ! ~ * ' ( ) ; %-encode rest. */
static char *uri_encode(const char *s) {
  static const char *keep = "-_.!~*'()";
  size_t L = strlen(s);
  char *o = malloc(L * 3 + 1);
  if (!o) return NULL;
  char *w = o;
  for (size_t i = 0; i < L; i++) {
    unsigned char c = (unsigned char)s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(keep, c))
      *w++ = (char)c;
    else { sprintf(w, "%%%02X", c); w += 3; }
  }
  *w = 0;
  return o;
}

/* POST data=<urlencoded query>; return parsed JSON doc or NULL. */
static cJSON *overpass_post(http_client *http, const char *endpoint,
                            const char *query, int timeout_ms) {
  char *enc = uri_encode(query);
  if (!enc) return NULL;
  size_t blen = strlen(enc) + 5;
  char *body = malloc(blen + 1);
  if (!body) { free(enc); return NULL; }
  snprintf(body, blen + 1, "data=%s", enc);
  free(enc);
  const char *hdrs[] = {
    "Content-Type: application/x-www-form-urlencoded",
    "User-Agent: JapanOSINT/1.0 (+https://github.com/rayanchl/JapanOSINT)",
    NULL };
  http_response r = {0};
  int rc = http_request(http, "POST", endpoint, hdrs, body, strlen(body),
                        timeout_ms > 0 ? timeout_ms : 60000, 0, &r);
  free(body);
  cJSON *doc = NULL;
  if (rc == 0 && r.status >= 200 && r.status < 300 && r.body)
    doc = cJSON_Parse(r.body);
  http_response_free(&r);
  return doc;
}

const char *ov_tag(cJSON *el, const char *k) {
  cJSON *tags = el ? cJSON_GetObjectItem(el, "tags") : NULL;
  cJSON *v = tags ? cJSON_GetObjectItem(tags, k) : NULL;
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static int num_ok(cJSON *v) { return v && cJSON_IsNumber(v); }

/* el.lat!=null ? [el.lon,el.lat] : [el.center.lon,el.center.lat]; 0 if none. */
static int el_coords(cJSON *el, double *lon, double *lat) {
  cJSON *la = cJSON_GetObjectItem(el, "lat");
  cJSON *lo = cJSON_GetObjectItem(el, "lon");
  if (num_ok(la) && num_ok(lo)) { *lat = la->valuedouble; *lon = lo->valuedouble; return 1; }
  cJSON *c = cJSON_GetObjectItem(el, "center");
  if (c) {
    cJSON *cla = cJSON_GetObjectItem(c, "lat");
    cJSON *clo = cJSON_GetObjectItem(c, "lon");
    if (num_ok(cla) && num_ok(clo)) { *lat = cla->valuedouble; *lon = clo->valuedouble; return 1; }
  }
  return 0;
}

/* data.elements filtered to those with usable coords (overpassRequest). */
static cJSON *filter_point_elements(cJSON *doc) {
  cJSON *els = doc ? cJSON_GetObjectItem(doc, "elements") : NULL;
  if (!els || !cJSON_IsArray(els)) return NULL;
  cJSON *out = cJSON_CreateArray();
  cJSON *e;
  cJSON_ArrayForEach(e, els) {
    double lo, la;
    if (el_coords(e, &lo, &la)) cJSON_AddItemToArray(out, cJSON_Duplicate(e, 1));
  }
  return out;
}

/* data.elements filtered to ways with geometry[>=2] (overpassWayRequest). */
static cJSON *filter_way_elements(cJSON *doc) {
  cJSON *els = doc ? cJSON_GetObjectItem(doc, "elements") : NULL;
  if (!els || !cJSON_IsArray(els)) return NULL;
  cJSON *out = cJSON_CreateArray();
  cJSON *e;
  cJSON_ArrayForEach(e, els) {
    cJSON *t = cJSON_GetObjectItem(e, "type");
    cJSON *g = cJSON_GetObjectItem(e, "geometry");
    if (t && cJSON_IsString(t) && strcmp(t->valuestring, "way") == 0 &&
        g && cJSON_IsArray(g) && cJSON_GetArraySize(g) >= 2)
      cJSON_AddItemToArray(out, cJSON_Duplicate(e, 1));
  }
  return out;
}

/* "type/id" dedupe key into buf; 0 if not derivable (always kept). */
static int el_key(cJSON *el, char *buf, size_t n) {
  cJSON *t = cJSON_GetObjectItem(el, "type");
  cJSON *id = cJSON_GetObjectItem(el, "id");
  if (t && cJSON_IsString(t) && id && cJSON_IsNumber(id)) {
    snprintf(buf, n, "%s/%lld", t->valuestring, (long long)id->valuedouble);
    return 1;
  }
  return 0;
}

static int seen_has(char **seen, int n, const char *k) {
  for (int i = 0; i < n; i++) if (strcmp(seen[i], k) == 0) return 1;
  return 0;
}

/* Map point elements via `map` into a features array; emit through geojson. */
static int emit_points(const source_ctx *ctx, intel_sink *sink,
                       cJSON *elements, overpass_map map, void *ud) {
  cJSON *features = cJSON_CreateArray();
  int i = 0;
  cJSON *el;
  cJSON_ArrayForEach(el, elements) {
    double lo, la;
    if (!el_coords(el, &lo, &la)) continue;
    cJSON *f = map(el, i, lo, la, ud);
    if (f) cJSON_AddItemToArray(features, f);
    i++;
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  return n;
}

int overpass_collect(const source_ctx *ctx, intel_sink *sink,
                     const char *body, int query_timeout, int timeout_ms,
                     overpass_map map, void *ud) {
  int qt = query_timeout > 0 ? query_timeout : 180;
  size_t qlen = strlen(body) + 160;
  char *query = malloc(qlen);
  if (!query) return -1;
  snprintf(query, qlen,
    "[out:json][timeout:%d];area[\"ISO3166-1\"=\"JP\"][admin_level=2]->.jp;(%s);out center;",
    qt, body);

  cJSON *elements = NULL;
  for (int e = 0; e < N_ENDPOINTS; e++) {
    cJSON *doc = overpass_post(ctx->http, ENDPOINTS[e], query, timeout_ms);
    cJSON *els = filter_point_elements(doc);
    if (doc) cJSON_Delete(doc);
    if (els && cJSON_GetArraySize(els) > 0) { elements = els; break; }
    if (els) cJSON_Delete(els);
  }
  free(query);
  if (!elements) {
    fprintf(stderr, "[overpass] %s no elements\n", ctx->source_id);
    return -1;
  }
  int n = emit_points(ctx, sink, elements, map, ud);
  cJSON_Delete(elements);
  fprintf(stderr, "[overpass] %s emitted %d\n", ctx->source_id, n);
  return n;
}

/* Shared tile fanout for point (out center) and way (out geom) queries. */
static cJSON *tiled_fetch(const source_ctx *ctx, overpass_bodyfn bodyfn,
                          int query_timeout, int timeout_ms, int ways,
                          void *ud) {
  int qt = query_timeout > 0 ? query_timeout : 180;
  cJSON *all = cJSON_CreateArray();
  char **seen = calloc(200000, sizeof(char *));
  int nseen = 0;
  for (int i = 0; i < 12; i++) {
    char bbox[96];
    snprintf(bbox, sizeof bbox, "%g,%g,%g,%g",
             TILES[i][0], TILES[i][1], TILES[i][2], TILES[i][3]);
    char tbody[4096];
    bodyfn(bbox, tbody, sizeof tbody, ud);
    size_t qlen = strlen(tbody) + 96;
    char *q = malloc(qlen);
    if (!q) continue;
    snprintf(q, qlen, "[out:json][timeout:%d];(%s);out %s;",
             qt, tbody, ways ? "geom" : "center");
    cJSON *got = NULL;
    for (int ei = 0; ei < N_ENDPOINTS; ei++) {
      const char *ep = ENDPOINTS[(ei + i) % N_ENDPOINTS];
      cJSON *doc = overpass_post(ctx->http, ep, q,
                                 timeout_ms > 0 ? timeout_ms
                                                : (ways ? 120000 : 60000));
      got = ways ? filter_way_elements(doc) : filter_point_elements(doc);
      if (doc) cJSON_Delete(doc);
      if (got && cJSON_GetArraySize(got) > 0) break;
      if (got) { cJSON_Delete(got); got = NULL; }
    }
    free(q);
    if (!got) continue;
    cJSON *el;
    cJSON_ArrayForEach(el, got) {
      char k[64];
      if (el_key(el, k, sizeof k)) {
        if (seen_has(seen, nseen, k)) continue;
        if (nseen < 200000) seen[nseen++] = strdup(k);
      }
      cJSON_AddItemToArray(all, cJSON_Duplicate(el, 1));
    }
    cJSON_Delete(got);
  }
  for (int i = 0; i < nseen; i++) free(seen[i]);
  free(seen);
  if (cJSON_GetArraySize(all) == 0) { cJSON_Delete(all); return NULL; }
  return all;
}

int overpass_tiled_collect(const source_ctx *ctx, intel_sink *sink,
                           overpass_bodyfn bodyfn, int query_timeout,
                           int timeout_ms, overpass_map map, void *ud) {
  cJSON *all = tiled_fetch(ctx, bodyfn, query_timeout, timeout_ms, 0, ud);
  if (!all) { fprintf(stderr, "[overpass] %s no elements\n", ctx->source_id); return -1; }
  int n = emit_points(ctx, sink, all, map, ud);
  cJSON_Delete(all);
  fprintf(stderr, "[overpass-tiled] %s emitted %d\n", ctx->source_id, n);
  return n;
}

int overpass_ways_collect(const source_ctx *ctx, intel_sink *sink,
                          overpass_bodyfn bodyfn, int query_timeout,
                          int timeout_ms, overpass_way_map map, void *ud) {
  cJSON *all = tiled_fetch(ctx, bodyfn, query_timeout, timeout_ms, 1, ud);
  if (!all) { fprintf(stderr, "[overpass] %s no ways\n", ctx->source_id); return -1; }
  cJSON *features = cJSON_CreateArray();
  int i = 0;
  cJSON *el;
  cJSON_ArrayForEach(el, all) {
    cJSON *geom = cJSON_GetObjectItem(el, "geometry");
    cJSON *coords = cJSON_CreateArray();
    cJSON *nd;
    cJSON_ArrayForEach(nd, geom) {
      cJSON *la = cJSON_GetObjectItem(nd, "lat");
      cJSON *lo = cJSON_GetObjectItem(nd, "lon");
      if (num_ok(la) && num_ok(lo)) {
        cJSON *pair = cJSON_CreateArray();
        cJSON_AddItemToArray(pair, cJSON_CreateNumber(lo->valuedouble));
        cJSON_AddItemToArray(pair, cJSON_CreateNumber(la->valuedouble));
        cJSON_AddItemToArray(coords, pair);
      }
    }
    if (cJSON_GetArraySize(coords) < 2) { cJSON_Delete(coords); i++; continue; }
    cJSON *f = map(el, i, coords, ud);
    cJSON_Delete(coords);
    if (f) cJSON_AddItemToArray(features, f);
    i++;
  }
  cJSON_Delete(all);
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[overpass-ways] %s emitted %d\n", ctx->source_id, n);
  return n;
}
