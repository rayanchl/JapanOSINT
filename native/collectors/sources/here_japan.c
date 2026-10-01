/* collectors/geospatial/sources/here_japan.c
 * Port of server/src/collectors/hereJapan.js.
 * HERE Browse API over metro centroids → FeatureCollection.
 * Gated on HERE_API_KEY. No seed. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
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
  const char *key = getenv("HERE_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[here-japan] gated (no HERE_API_KEY)\n");
    return 0;
  }

  cJSON *features = cJSON_CreateArray();
  for (size_t si = 0; si < sizeof(SCAN) / sizeof(SCAN[0]); si++) {
    char url[512];
    snprintf(url, sizeof url,
      "https://browse.search.hereapi.com/v1/browse"
      "?at=%.4f,%.4f&limit=100&apiKey=%s",
      SCAN[si].lat, SCAN[si].lon, key);
    cJSON *data = feed_get_json(ctx->http, url, 12000);
    cJSON *items = data ? cJSON_GetObjectItem(data, "items") : NULL;
    if (cJSON_IsArray(items)) {
      cJSON *it;
      cJSON_ArrayForEach(it, items) {
        cJSON *pos = cJSON_GetObjectItem(it, "position");
        double lon, lat;
        if (!(pos &&
              jo_num_of(cJSON_GetObjectItem(pos, "lng"), &lon) &&
              jo_num_of(cJSON_GetObjectItem(pos, "lat"), &lat)))
          continue;

        cJSON *of = gj_point_feature(lon, lat);

        cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
        cJSON *idv = cJSON_GetObjectItem(it, "id");
        cJSON_AddItemToObject(p, "poi_id",
          (idv && cJSON_IsString(idv) && idv->valuestring[0])
            ? cJSON_CreateString(idv->valuestring) : cJSON_CreateNull());
        cJSON *tv = cJSON_GetObjectItem(it, "title");
        cJSON_AddItemToObject(p, "name",
          (tv && cJSON_IsString(tv) && tv->valuestring[0])
            ? cJSON_CreateString(tv->valuestring) : cJSON_CreateNull());
        cJSON *cats = cJSON_GetObjectItem(it, "categories");
        cJSON *c0 = (cats && cJSON_IsArray(cats))
                      ? cJSON_GetArrayItem(cats, 0) : NULL;  /* exhaustive-ok: display pick; categories_all carries every category */
        if (cats && cJSON_IsArray(cats) && cJSON_GetArraySize(cats) > 1)
          cJSON_AddItemToObject(p, "categories_all", cJSON_Duplicate(cats, 1));
        cJSON *cnm = c0 ? cJSON_GetObjectItem(c0, "name") : NULL;
        cJSON_AddItemToObject(p, "category",
          (cnm && cJSON_IsString(cnm) && cnm->valuestring[0])
            ? cJSON_CreateString(cnm->valuestring) : cJSON_CreateNull());
        cJSON *addr = cJSON_GetObjectItem(it, "address");
        cJSON *lbl = addr ? cJSON_GetObjectItem(addr, "label") : NULL;
        cJSON_AddItemToObject(p, "address",
          (lbl && cJSON_IsString(lbl) && lbl->valuestring[0])
            ? cJSON_CreateString(lbl->valuestring) : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "source", "here_api");
        cJSON_AddItemToObject(of, "properties", p);
        cJSON_AddItemToArray(features, of);
      }
    }
    if (data) cJSON_Delete(data);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[here-japan] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def here_japan_def = {
  .id = "here-japan", .collector = "geospatial",
  .name = "HERE Maps Japan",
  .name_ja = "HERE Maps \xE6\x97\xA5\xE6\x9C\xAC",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(here_japan_def)
