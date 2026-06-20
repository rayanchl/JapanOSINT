/* collectors/geospatial/sources/japan_api_prefectures.c
 * Port of server/src/collectors/japanApiPrefectures.js. japan-api community
 * REST API → one Feature per prefecture. SEED branch dropped (rule 8). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>

#define API_URL "https://japanapi.curtisbarnard.com/api/v1/prefectures"

/* JS: a ?? b ?? null  → first non-null cJSON value (Duplicate) or NULL. */
static cJSON *coalesce2(cJSON *a, cJSON *b) {
  if (a && !cJSON_IsNull(a)) return cJSON_Duplicate(a, 1);
  if (b && !cJSON_IsNull(b)) return cJSON_Duplicate(b, 1);
  return cJSON_CreateNull();
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *data = feed_get_json(ctx->http, API_URL, 10000);
  if (!data) return -1;

  /* list = Array.isArray(data) ? data : (data?.data ?? data?.prefectures ?? []) */
  cJSON *list = NULL;
  if (cJSON_IsArray(data)) list = data;
  else {
    cJSON *d = cJSON_GetObjectItem(data, "data");
    if (cJSON_IsArray(d)) list = d;
    else {
      cJSON *pr = cJSON_GetObjectItem(data, "prefectures");
      if (cJSON_IsArray(pr)) list = pr;
    }
  }

  cJSON *features = cJSON_CreateArray();
  if (list) {
    cJSON *p;
    cJSON_ArrayForEach(p, list) {
      cJSON *lat = cJSON_GetObjectItem(p, "latitude");
      if (!lat || cJSON_IsNull(lat)) lat = cJSON_GetObjectItem(p, "lat");
      cJSON *lon = cJSON_GetObjectItem(p, "longitude");
      if (!lon || cJSON_IsNull(lon)) lon = cJSON_GetObjectItem(p, "lon");
      if (!lat || cJSON_IsNull(lat) || !lon || cJSON_IsNull(lon)) continue;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(cJSON_GetNumberValue(lon)));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(cJSON_GetNumberValue(lat)));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *pr = cJSON_CreateObject();          /* EXACT JS key order */
      cJSON_AddItemToObject(pr, "name",
        coalesce2(cJSON_GetObjectItem(p, "name"), cJSON_GetObjectItem(p, "english")));
      cJSON_AddItemToObject(pr, "name_ja",
        coalesce2(cJSON_GetObjectItem(p, "japanese"), cJSON_GetObjectItem(p, "nameJa")));
      cJSON *rg = cJSON_GetObjectItem(p, "region");
      cJSON_AddItemToObject(pr, "region",
        (rg && !cJSON_IsNull(rg)) ? cJSON_Duplicate(rg, 1) : cJSON_CreateNull());
      cJSON *po = cJSON_GetObjectItem(p, "population");
      cJSON_AddItemToObject(pr, "population",
        (po && !cJSON_IsNull(po)) ? cJSON_Duplicate(po, 1) : cJSON_CreateNull());
      cJSON_AddStringToObject(pr, "source", "japan_api");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
    }
  }
  cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[japan-api-prefectures] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def japan_api_prefectures_def = {
  .id = "japan-api-prefectures", .collector = "geospatial",
  .name = "Japan Prefectures REST API", .name_ja = "都道府県 REST API",
   .update_interval_sec = 604800, .run = run,
};
REGISTER_SOURCE(japan_api_prefectures_def)
