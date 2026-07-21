/* collectors/environment/sources/p2pquake_jma.c
 * Port of server/src/collectors/p2pquakeJma.js.
 * P2P Quake community JMA mirror (codes=551, limit=50) → FeatureCollection.
 * Keyless. Items with no hypocenter / null lat|lon are dropped (=== JS). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://api.p2pquake.net/v2/history?codes=551&limit=50"
#define TIMEOUT_MS 8000

static void add_or_null(cJSON *p, const char *k, cJSON *v) {
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

/* coalesce a ?? b ?? null */
static void coalesce2(cJSON *p, const char *k, cJSON *a, cJSON *b) {
  if (a && !cJSON_IsNull(a)) cJSON_AddItemToObject(p, k, cJSON_Duplicate(a, 1));
  else if (b && !cJSON_IsNull(b)) cJSON_AddItemToObject(p, k, cJSON_Duplicate(b, 1));
  else cJSON_AddItemToObject(p, k, cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "User-Agent: JapanOSINT", NULL };
  cJSON *data = feed_get_json_h(ctx->http, API_URL, hdrs, TIMEOUT_MS);

  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(data)) {
    cJSON *item;
    cJSON_ArrayForEach(item, data) {
      cJSON *eq = cJSON_GetObjectItem(item, "earthquake");
      cJSON *h = eq ? cJSON_GetObjectItem(eq, "hypocenter") : NULL;
      if (!h || cJSON_IsNull(h)) continue;
      cJSON *latv = cJSON_GetObjectItem(h, "latitude");
      cJSON *lonv = cJSON_GetObjectItem(h, "longitude");
      if (!latv || cJSON_IsNull(latv) || !lonv || cJSON_IsNull(lonv)) continue;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_Duplicate(lonv, 1));
      cJSON_AddItemToArray(co, cJSON_Duplicate(latv, 1));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
      add_or_null(p, "earthquake_id", cJSON_GetObjectItem(item, "id"));
      add_or_null(p, "magnitude",     cJSON_GetObjectItem(h, "magnitude"));
      add_or_null(p, "depth_km",      cJSON_GetObjectItem(h, "depth"));
      add_or_null(p, "max_intensity", eq ? cJSON_GetObjectItem(eq, "maxScale") : NULL);
      coalesce2(p, "timestamp",
        eq ? cJSON_GetObjectItem(eq, "time") : NULL,
        cJSON_GetObjectItem(item, "time"));
      add_or_null(p, "place", cJSON_GetObjectItem(h, "name"));
      cJSON *dt = eq ? cJSON_GetObjectItem(eq, "domesticTsunami") : NULL;
      int tw = (dt && cJSON_IsString(dt) && dt->valuestring[0] &&
                strcmp(dt->valuestring, "None") != 0);
      cJSON_AddItemToObject(p, "tsunami_warning", cJSON_CreateBool(tw));
      cJSON_AddStringToObject(p, "source", "p2pquake");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
  }
  if (data) cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[p2pquake-jma] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def p2pquake_jma_def = {
  .id = "p2pquake-jma", .collector = "environment",
  .name = "P2P Quake JMA mirror", .name_ja = "P2P地震情報 (JMA)",
   .update_interval_sec = 60, .run = run,
};
REGISTER_SOURCE(p2pquake_jma_def)
