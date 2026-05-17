/* collectors/environment/sources/jma_typhoon_json.c
 * Port of server/src/collectors/jmaTyphoonJson.js. JMA active typhoon index
 * → one Feature per TC with lat/lon. SEED branch dropped (rule 8). */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>

#define INDEX_URL "https://www.jma.go.jp/bosai/typhoon/data/targetTc.json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *data = feed_get_json(ctx->http, INDEX_URL, 8000);

  cJSON *features = cJSON_CreateArray();
  /* list = Array.isArray(data) ? data : [] */
  if (data && cJSON_IsArray(data)) {
    cJSON *tc;
    cJSON_ArrayForEach(tc, data) {
      cJSON *lat = cJSON_GetObjectItem(tc, "lat");
      cJSON *lon = cJSON_GetObjectItem(tc, "lon");
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

      cJSON *p = cJSON_CreateObject();           /* EXACT JS key order */
      cJSON *nm = cJSON_GetObjectItem(tc, "name");
      cJSON_AddItemToObject(p, "name",
        (nm && !cJSON_IsNull(nm)) ? cJSON_Duplicate(nm, 1) : cJSON_CreateNull());
      cJSON *id = cJSON_GetObjectItem(tc, "id");
      cJSON_AddItemToObject(p, "id",
        (id && !cJSON_IsNull(id)) ? cJSON_Duplicate(id, 1) : cJSON_CreateNull());
      cJSON_AddStringToObject(p, "source", "jma_typhoon");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jma-typhoon-json] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def jma_typhoon_json_def = {
  .id = "jma-typhoon-json", .collector = "environment",
  .name = "JMA Typhoon Track JSON", .name_ja = "気象庁 台風経路",
   .update_interval_sec = 900, .run = run,
};
REGISTER_SOURCE(jma_typhoon_json_def)
