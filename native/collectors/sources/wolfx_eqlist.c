/* collectors/environment/sources/wolfx_eqlist.c
 * Port of server/src/collectors/wolfxEqlist.js. api.wolfx.jp/jma_eqlist.json
 * is an OBJECT keyed by report; each object value → a Feature. uid via
 * event_id (∈ NATIVE_ID_KEYS) = EventID ?? key. Non-object values (md5,
 * Title…) skipped, exactly like `typeof v==='object' && v`. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../core/intel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://api.wolfx.jp/jma_eqlist.json"

static void add_or_null(cJSON *p, const char *k, cJSON *v) {
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *data = feed_get_json(ctx->http, API_URL, 8000);
  if (!data || !cJSON_IsObject(data)) { if (data) cJSON_Delete(data); return -1; }

  cJSON *features = cJSON_CreateArray();
  cJSON *eq;
  cJSON_ArrayForEach(eq, data) {                  /* Object.entries order */
    if (!cJSON_IsObject(eq)) continue;            /* typeof v==='object' && v */
    const char *key = eq->string ? eq->string : "";

    cJSON *lat = cJSON_GetObjectItem(eq, "Latitude");
    cJSON *lon = cJSON_GetObjectItem(eq, "Longitude");
    double dlat = lat ? (cJSON_IsNumber(lat) ? lat->valuedouble
                        : (cJSON_IsString(lat) ? atof(lat->valuestring) : 0)) : 0;
    double dlon = lon ? (cJSON_IsNumber(lon) ? lon->valuedouble
                        : (cJSON_IsString(lon) ? atof(lon->valuestring) : 0)) : 0;
    int geo = (lat && lon && dlat == dlat && dlon == dlon &&
               (cJSON_IsNumber(lat) || (cJSON_IsString(lat) && lat->valuestring[0])) &&
               (cJSON_IsNumber(lon) || (cJSON_IsString(lon) && lon->valuestring[0])));

    cJSON *feat = cJSON_CreateObject();
    cJSON_AddStringToObject(feat, "type", "Feature");
    if (geo) {
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(dlon));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(dlat));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(feat, "geometry", g);
    } else {
      cJSON_AddItemToObject(feat, "geometry", cJSON_CreateNull());
    }
    cJSON *p = cJSON_CreateObject();             /* exact JS key order */
    cJSON *eid = cJSON_GetObjectItem(eq, "EventID");
    if (eid) cJSON_AddItemToObject(p, "event_id", cJSON_Duplicate(eid, 1));
    else cJSON_AddStringToObject(p, "event_id", key);
    cJSON *mg = cJSON_GetObjectItem(eq, "Magunitude");
    if (!mg) mg = cJSON_GetObjectItem(eq, "Magnitude");
    add_or_null(p, "magnitude", mg);
    add_or_null(p, "depth_km",      cJSON_GetObjectItem(eq, "Depth"));
    add_or_null(p, "max_intensity", cJSON_GetObjectItem(eq, "MaxIntensity"));
    add_or_null(p, "place",         cJSON_GetObjectItem(eq, "Hypocenter"));
    cJSON *tm = cJSON_GetObjectItem(eq, "time");
    if (!tm) tm = cJSON_GetObjectItem(eq, "Time");
    add_or_null(p, "time", tm);
    cJSON_AddStringToObject(p, "source", "wolfx_eqlist");
    cJSON_AddItemToObject(feat, "properties", p);
    cJSON_AddItemToArray(features, feat);
  }
  cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[wolfx-eqlist] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def wolfx_eqlist_def = {
  .id = "wolfx-eqlist", .collector = "environment",
  .name = "Wolfx JMA Earthquake List", .name_ja = "Wolfx 地震一覧",
   .update_interval_sec = 60, .run = run,
};
REGISTER_SOURCE(wolfx_eqlist_def)
