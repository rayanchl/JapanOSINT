/* collectors/geospatial/sources/gsi_geocode.c
 * Port of server/src/collectors/gsiGeocode.js (FeatureCollection) +
 * inlined utils/gsiAddressSearch.js (single GET, top hit).
 * Probe q=東京駅 → 1 Feature on hit; seed branch dropped (rule 8) so a
 * miss/failure → 0 rows. Props order: title, source. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>

/* q=東京駅 percent-encoded (encodeURIComponent of the 3 UTF-8 chars). */
#define SEARCH_URL \
  "https://msearch.gsi.go.jp/address-search/AddressSearch" \
  "?q=%E6%9D%B1%E4%BA%AC%E9%A7%85"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *data = feed_get_json(ctx->http, SEARCH_URL, 8000);
  cJSON *features = cJSON_CreateArray();

  /* gsiAddressSearch: need a non-empty array; first.geometry.coordinates
   * with >=2 finite numbers; else null → no live hit. */
  if (data && cJSON_IsArray(data) && cJSON_GetArraySize(data) > 0) {
    cJSON *first = cJSON_GetArrayItem(data, 0);
    cJSON *geom  = first ? cJSON_GetObjectItem(first, "geometry") : NULL;
    cJSON *coords= geom ? cJSON_GetObjectItem(geom, "coordinates") : NULL;
    if (cJSON_IsArray(coords) && cJSON_GetArraySize(coords) >= 2) {
      cJSON *c0 = cJSON_GetArrayItem(coords, 0);
      cJSON *c1 = cJSON_GetArrayItem(coords, 1);
      if (c0 && cJSON_IsNumber(c0) && c1 && cJSON_IsNumber(c1)) {
        double lon = c0->valuedouble, lat = c1->valuedouble;

        cJSON *props0 = cJSON_GetObjectItem(first, "properties");
        cJSON *titlev = props0 ? cJSON_GetObjectItem(props0, "title") : NULL;

        cJSON *f = gj_point_feature(lon, lat);

        cJSON *p = cJSON_CreateObject();      /* EXACT JS key order */
        /* title: first?.properties?.title ?? null */
        cJSON_AddItemToObject(p, "title",
          (titlev && !cJSON_IsNull(titlev)) ? cJSON_Duplicate(titlev, 1)
                                            : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "source", "gsi_geocode");
        cJSON_AddItemToObject(f, "properties", p);
        cJSON_AddItemToArray(features, f);
      }
    }
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[gsi-geocode] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def gsi_geocode_def = {
  .id = "gsi-geocode", .collector = "geospatial",
  .name = "GSI Address Search", .name_ja = "国土地理院 住所検索",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(gsi_geocode_def)
