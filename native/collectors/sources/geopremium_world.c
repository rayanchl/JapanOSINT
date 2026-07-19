/* collectors/sources/geopremium_world.c
 * OSINT services — key-gated premium geocoding, pivot on ctx->entity.
 * Honest-empty when the key is unset.
 *
 *   WHAT3WORDS_CONVERT  api.what3words.com/v3/convert-to-coordinates  W3W_API_KEY
 *   OPENCAGE_GEOCODE    api.opencagedata.com/geocode/v1/json          OPENCAGE_KEY */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *GP_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run_w3w(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("W3W_API_KEY");
  if (!k) { fprintf(stderr, "[WHAT3WORDS_CONVERT] no W3W_API_KEY\n"); return 0; }
  char url[512];
  snprintf(url, sizeof url,
    "https://api.what3words.com/v3/convert-to-coordinates?words=%s&key=%s", enc, k);
  const char *hdrs[] = { "Accept: application/json", GP_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "WHAT3WORDS_CONVERT");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *coords = cJSON_GetObjectItem(root, "coordinates");
  const cJSON *lat = coords ? cJSON_GetObjectItem(coords, "lat") : NULL;
  const cJSON *lng = coords ? cJSON_GetObjectItem(coords, "lng") : NULL;
  if (!cJSON_IsNumber(lat) || !cJSON_IsNumber(lng)) { cJSON_Delete(root); return 0; }
  const char *words = jo_sv(root, "words");
  const char *nearest = jo_sv(root, "nearestPlace");
  const char *country = jo_sv(root, "country");
  cJSON *d = cJSON_CreateObject();
  if (words) cJSON_AddStringToObject(d, "words", words);
  cJSON_AddNumberToObject(d, "lat", lat->valuedouble);
  cJSON_AddNumberToObject(d, "lon", lng->valuedouble);
  if (nearest) cJSON_AddStringToObject(d, "nearest_place", nearest);
  if (country) cJSON_AddStringToObject(d, "country", country);
  cJSON_AddStringToObject(d, "source", "what3words");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "WHAT3WORDS_CONVERT");
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char title[200];
  snprintf(title, sizeof title, "///%s → %.5f,%.5f", words ? words : ctx->entity, lat->valuedouble, lng->valuedouble);
  intel_item it = {0};
  it.remote_key = words ? words : ctx->entity; it.title = title; it.summary = nearest;
  it.body = bj; it.lang = "en"; it.has_geo = 1; it.lat = lat->valuedouble; it.lon = lng->valuedouble;
  it.record_type = "what3words";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"WHAT3WORDS_CONVERT\"]";
  int rc = sink->emit(sink, &it); free(bj); free(pj); cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run_opencage(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("OPENCAGE_KEY");
  if (!k) { fprintf(stderr, "[OPENCAGE_GEOCODE] no OPENCAGE_KEY\n"); return 0; }
  char url[512];
  snprintf(url, sizeof url,
    "https://api.opencagedata.com/geocode/v1/json?q=%s&key=%s&limit=10&no_annotations=1", enc, k);
  const char *hdrs[] = { "Accept: application/json", GP_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "OPENCAGE_GEOCODE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const char *formatted = jo_sv(r, "formatted");
      const cJSON *geom = cJSON_GetObjectItem(r, "geometry");
      const cJSON *lat = geom ? cJSON_GetObjectItem(geom, "lat") : NULL;
      const cJSON *lng = geom ? cJSON_GetObjectItem(geom, "lng") : NULL;
      if (!formatted || !cJSON_IsNumber(lat) || !cJSON_IsNumber(lng)) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "formatted", formatted);
      cJSON_AddNumberToObject(d, "lat", lat->valuedouble);
      cJSON_AddNumberToObject(d, "lon", lng->valuedouble);
      cJSON_AddStringToObject(d, "source", "OpenCage");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "OPENCAGE_GEOCODE");
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      intel_item it = {0};
      it.remote_key = formatted; it.title = formatted; it.body = bj; it.lang = "en";
      it.has_geo = 1; it.lat = lat->valuedouble; it.lon = lng->valuedouble;
      it.record_type = "geocode-result";
      it.properties_json = pj; it.tags_json = "[\"osint-search\",\"OPENCAGE_GEOCODE\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "WHAT3WORDS_CONVERT")) n = run_w3w(ctx, sink, enc);
  else if (!strcmp(sid, "OPENCAGE_GEOCODE"))   n = run_opencage(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define GP_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "geospatial", \
    .type = "api", .url = "internal://osint/geopremium", .description = DESC, \
    .layer = NULL, .free_tier = 0 }; \
  REGISTER_SOURCE(sym)

GP_DEF(gp_w3w_def, "WHAT3WORDS_CONVERT", "what3words to Coordinates",
  "what3words (W3W_API_KEY): convert a 3-word address to lat/lon + nearest place.");
GP_DEF(gp_opencage_def, "OPENCAGE_GEOCODE", "OpenCage Geocode",
  "OpenCage (OPENCAGE_KEY): forward/reverse geocode a place to coordinates.");
