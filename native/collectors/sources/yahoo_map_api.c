/* collectors/geospatial/sources/yahoo_map_api.c
 * Port of server/src/collectors/yahooMapApi.js.
 * Yahoo! Japan Local Search API over metro centroids → FeatureCollection.
 * Gated on YAHOO_APP_ID. No seed. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const struct { double lat, lon; } SCAN[] = {
  { 35.6812, 139.7671 },  /* Tokyo */
  { 34.7024, 135.4959 },  /* Osaka */
  { 35.1709, 136.8815 },  /* Nagoya */
  { 33.5897, 130.4207 },  /* Fukuoka */
  { 43.0687, 141.3508 },  /* Sapporo */
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *appId = getenv("YAHOO_APP_ID");
  if (!appId || !*appId) {
    fprintf(stderr, "[yahoo-map-api] gated (no YAHOO_APP_ID)\n");
    return 0;
  }

  cJSON *features = cJSON_CreateArray();
  for (size_t si = 0; si < sizeof(SCAN) / sizeof(SCAN[0]); si++) {
    char url[512];
    snprintf(url, sizeof url,
      "https://map.yahooapis.jp/search/local/V1/localSearch"
      "?appid=%s&output=json&results=50&sort=dist&lat=%.4f&lon=%.4f&dist=10",
      appId, SCAN[si].lat, SCAN[si].lon);
    cJSON *data = feed_get_json(ctx->http, url, 12000);
    cJSON *feat = data ? cJSON_GetObjectItem(data, "Feature") : NULL;
    if (cJSON_IsArray(feat)) {
      cJSON *f;
      cJSON_ArrayForEach(f, feat) {
        cJSON *geom = cJSON_GetObjectItem(f, "Geometry");
        cJSON *coordv = geom ? cJSON_GetObjectItem(geom, "Coordinates") : NULL;
        if (!(coordv && cJSON_IsString(coordv))) continue;
        /* "lon,lat".split(',').map(Number) */
        double lon, lat;
        char *e;
        lon = strtod(coordv->valuestring, &e);
        if (*e != ',') continue;
        lat = strtod(e + 1, NULL);
        if (lon != lon || lat != lat) continue;   /* NaN guard */

        cJSON *of = cJSON_CreateObject();
        cJSON_AddStringToObject(of, "type", "Feature");
        cJSON *g = cJSON_CreateObject();
        cJSON_AddStringToObject(g, "type", "Point");
        cJSON *co = cJSON_CreateArray();
        cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
        cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
        cJSON_AddItemToObject(g, "coordinates", co);
        cJSON_AddItemToObject(of, "geometry", g);

        cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
        cJSON *idv = cJSON_GetObjectItem(f, "Id");
        cJSON_AddItemToObject(p, "poi_id",
          (idv && cJSON_IsString(idv) && idv->valuestring[0])
            ? cJSON_CreateString(idv->valuestring) : cJSON_CreateNull());
        cJSON *nmv = cJSON_GetObjectItem(f, "Name");
        cJSON_AddItemToObject(p, "name",
          (nmv && cJSON_IsString(nmv) && nmv->valuestring[0])
            ? cJSON_CreateString(nmv->valuestring) : cJSON_CreateNull());
        cJSON *prop = cJSON_GetObjectItem(f, "Property");
        cJSON *genre = prop ? cJSON_GetObjectItem(prop, "Genre") : NULL;
        cJSON *g0 = (genre && cJSON_IsArray(genre))
                      ? cJSON_GetArrayItem(genre, 0) : NULL;
        cJSON *gnm = g0 ? cJSON_GetObjectItem(g0, "Name") : NULL;
        cJSON_AddItemToObject(p, "category",
          (gnm && cJSON_IsString(gnm) && gnm->valuestring[0])
            ? cJSON_CreateString(gnm->valuestring) : cJSON_CreateNull());
        cJSON *adr = prop ? cJSON_GetObjectItem(prop, "Address") : NULL;
        cJSON_AddItemToObject(p, "address",
          (adr && cJSON_IsString(adr) && adr->valuestring[0])
            ? cJSON_CreateString(adr->valuestring) : cJSON_CreateNull());
        cJSON *tel = prop ? cJSON_GetObjectItem(prop, "Tel1") : NULL;
        cJSON_AddItemToObject(p, "tel",
          (tel && cJSON_IsString(tel) && tel->valuestring[0])
            ? cJSON_CreateString(tel->valuestring) : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "source", "yahoo_api");
        cJSON_AddItemToObject(of, "properties", p);
        cJSON_AddItemToArray(features, of);
      }
    }
    if (data) cJSON_Delete(data);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[yahoo-map-api] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def yahoo_map_api_def = {
  .id = "yahoo-map-api", .collector = "geospatial",
  .name = "Yahoo! Japan Map API",
  .name_ja = "Yahoo!\xE5\x9C\xB0\xE5\x9B\xB3 API",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(yahoo_map_api_def)
