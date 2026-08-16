/* collectors/telecom/sources/mapfan_api.c
 * Port of server/src/collectors/mapfanApi.js.
 * MapFan API POI/place data — paid, key-gated, NO public unauthenticated
 * feed. Gated honest-empty on MAPFAN_API_KEY. With key: attempt POI-search
 * over JP bbox (key in querystring), build point features; honest empty on
 * failure. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int coord(cJSON *r, double *lon, double *lat) {
  cJSON *v;
  if ((v = cJSON_GetObjectItem(r, "lon")) && cJSON_IsNumber(v)) *lon = v->valuedouble;
  else if ((v = cJSON_GetObjectItem(r, "longitude")) && cJSON_IsNumber(v)) *lon = v->valuedouble;
  else {
    cJSON *g = cJSON_GetObjectItem(r, "geometry");
    cJSON *c = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
    cJSON *x = c ? cJSON_GetArrayItem(c, 0) : NULL;
    if (!x || !cJSON_IsNumber(x)) return 0;
    *lon = x->valuedouble;
  }
  if ((v = cJSON_GetObjectItem(r, "lat")) && cJSON_IsNumber(v)) *lat = v->valuedouble;
  else if ((v = cJSON_GetObjectItem(r, "latitude")) && cJSON_IsNumber(v)) *lat = v->valuedouble;
  else {
    cJSON *g = cJSON_GetObjectItem(r, "geometry");
    cJSON *c = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
    cJSON *y = c ? cJSON_GetArrayItem(c, 1) : NULL;
    if (!y || !cJSON_IsNumber(y)) return 0;
    *lat = y->valuedouble;
  }
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("MAPFAN_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[mapfan-api] gated (no MAPFAN_API_KEY)\n");
    return 0;
  }
  /* JP_BBOX = [122,24,146,46]; key URL-encoded — keys are typically
   * alnum/-/_ so passthrough is safe; build querystring as Node does. */
  char url[512];
  snprintf(url, sizeof url,
    "https://api.mapfan.com/v1/poi/search"
    "?bbox=122,24,146,46&limit=200&key=%s", key);
  cJSON *data = feed_get_json(ctx->http, url, 20000);

  cJSON *rows = NULL;
  if (data) {
    cJSON *r = cJSON_GetObjectItem(data, "features");
    if (r && cJSON_IsArray(r)) rows = r;
    else if ((r = cJSON_GetObjectItem(data, "results")) && cJSON_IsArray(r)) rows = r;
    else if ((r = cJSON_GetObjectItem(data, "pois")) && cJSON_IsArray(r)) rows = r;
  }
  if (!rows || cJSON_GetArraySize(rows) == 0) {
    if (data) cJSON_Delete(data);
    fprintf(stderr, "[mapfan-api] unavailable (no rows)\n");
    return -1;
  }

  cJSON *features = cJSON_CreateArray();
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    double lon, lat;
    if (!coord(r, &lon, &lat)) continue;
    cJSON *f = gj_point_feature(lon, lat);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddItemToObject(p, "poi_id", jo_pick_dup(r, "id", "poi_id"));
    cJSON_AddItemToObject(p, "name", jo_pick_dup(r, "name", "title"));
    cJSON_AddItemToObject(p, "category", jo_pick_dup(r, "category", "genre"));
    cJSON *addr = cJSON_GetObjectItem(r, "address");
    cJSON_AddItemToObject(p, "address",
      (addr && !cJSON_IsNull(addr)) ? cJSON_Duplicate(addr, 1)
                                    : cJSON_CreateNull());
    cJSON_AddStringToObject(p, "source", "mapfan_api");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[mapfan-api] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def mapfan_api_def = {
  .id = "mapfan-api", .collector = "telecom",
  .name = "MapFan API", .name_ja = "MapFan API",
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(mapfan_api_def)
