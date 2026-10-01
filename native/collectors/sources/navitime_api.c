/* collectors/transport/sources/navitime_api.c
 * Port of server/src/collectors/navitimeApi.js.
 * Navitime RapidAPI transport_node search around 5 major metros, gated on
 * NAVITIME_API_KEY (RapidAPI key, sent via X-RapidAPI-* headers). No key-free
 * fallback — honest empty FeatureCollection without the key or on failure. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* METROS, URL-encoded (東京 大阪 名古屋 福岡 札幌). */
static const char *METROS[] = {
  "%E6%9D%B1%E4%BA%AC",                 /* 東京 */
  "%E5%A4%A7%E9%98%AA",                 /* 大阪 */
  "%E5%90%8D%E5%8F%A4%E5%B1%8B",        /* 名古屋 */
  "%E7%A6%8F%E5%B2%A1",                 /* 福岡 */
  "%E6%9C%AD%E5%B9%8C",                 /* 札幌 */
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("NAVITIME_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[navitime-api] gated (no NAVITIME_API_KEY)\n");
    return 0;                       /* honest empty */
  }
  char keyhdr[256];
  snprintf(keyhdr, sizeof keyhdr, "X-RapidAPI-Key: %s", key);
  const char *hdrs[] = {
    keyhdr,
    "X-RapidAPI-Host: navitime-transport.p.rapidapi.com",
    NULL,
  };

  cJSON *features = cJSON_CreateArray();
  for (size_t i = 0; i < sizeof METROS / sizeof METROS[0]; i++) {
    char url[256];
    snprintf(url, sizeof url,
      "https://navitime-transport.p.rapidapi.com/transport_node"
      "?word=%s&datum=wgs84&coord_unit=degree", METROS[i]);
    cJSON *data = feed_get_json_h(ctx->http, url, hdrs, 12000);
    cJSON *items = data ? cJSON_GetObjectItem(data, "items") : NULL;
    if (cJSON_IsArray(items)) {
      cJSON *it;
      cJSON_ArrayForEach(it, items) {
        cJSON *c = cJSON_GetObjectItem(it, "coord");
        cJSON *lonv = c ? cJSON_GetObjectItem(c, "lon") : NULL;
        cJSON *latv = c ? cJSON_GetObjectItem(c, "lat") : NULL;
        if (!c || !lonv || cJSON_IsNull(lonv) || !latv || cJSON_IsNull(latv))
          continue;
        double lon = cJSON_IsNumber(lonv) ? lonv->valuedouble
                   : (cJSON_IsString(lonv) ? strtod(lonv->valuestring, 0) : 0);
        double lat = cJSON_IsNumber(latv) ? latv->valuedouble
                   : (cJSON_IsString(latv) ? strtod(latv->valuestring, 0) : 0);

        cJSON *f = gj_point_feature(lon, lat);

        cJSON *p = cJSON_CreateObject();         /* EXACT JS key order */
        jo_put_str_or_null(p, "node_id", it, "id");
        jo_put_str_or_null(p, "name", it, "name");
        jo_put_str_or_null(p, "ruby", it, "ruby");
        cJSON *types = cJSON_GetObjectItem(it, "types");
        cJSON_AddItemToObject(p, "types",
          types ? cJSON_Duplicate(types, 1) : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "source", "navitime_api");
        cJSON_AddItemToObject(f, "properties", p);
        cJSON_AddItemToArray(features, f);
      }
    }
    if (data) cJSON_Delete(data);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[navitime-api] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def navitime_api_def = {
  .id = "navitime-api", .collector = "transport",
  .name = "Navitime API", .name_ja = "ナビタイム API",
   .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(navitime_api_def)
