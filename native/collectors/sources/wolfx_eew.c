/* collectors/environment/sources/wolfx_eew.c
 * Port of server/src/collectors/wolfxEew.js. api.wolfx.jp/jma_eew.json is a
 * single EEW object → at most one Feature. Keyless. SEED branch (d==null)
 * dropped → 0 features when upstream down. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://api.wolfx.jp/jma_eew.json"

/* x ?? null (any JSON type) */
static void coalesce_null(cJSON *p, const char *k, cJSON *v) {
  cJSON_AddItemToObject(p, k,
    (v && !cJSON_IsNull(v)) ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

/* JS Number(x): number → itself, numeric string → parsed, else NaN. */
static double jnum(cJSON *v, int *ok) {
  if (v && cJSON_IsNumber(v)) { *ok = 1; return v->valuedouble; }
  if (v && cJSON_IsString(v) && v->valuestring[0]) {
    char *e; double d = strtod(v->valuestring, &e);
    if (e != v->valuestring) { *ok = 1; return d; }
  }
  *ok = 0;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *d = feed_get_json(ctx->http, API_URL, 6000);

  cJSON *features = cJSON_CreateArray();
  cJSON *latv = d ? cJSON_GetObjectItem(d, "Latitude") : NULL;
  cJSON *lonv = d ? cJSON_GetObjectItem(d, "Longitude") : NULL;
  if (latv && !cJSON_IsNull(latv) && lonv && !cJSON_IsNull(lonv)) {
    int ok1, ok2;
    double lon = jnum(lonv, &ok1);
    double lat = jnum(latv, &ok2);

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *p = cJSON_CreateObject();               /* EXACT JS key order */
    coalesce_null(p, "event_id", cJSON_GetObjectItem(d, "EventID"));
    coalesce_null(p, "serial", cJSON_GetObjectItem(d, "Serial"));
    cJSON *mg = cJSON_GetObjectItem(d, "Magunitude");
    if (!mg || cJSON_IsNull(mg)) mg = cJSON_GetObjectItem(d, "Magnitude");
    coalesce_null(p, "magnitude", mg);
    coalesce_null(p, "depth_km", cJSON_GetObjectItem(d, "Depth"));
    coalesce_null(p, "max_intensity", cJSON_GetObjectItem(d, "MaxIntensity"));
    coalesce_null(p, "hypocenter", cJSON_GetObjectItem(d, "Hypocenter"));
    coalesce_null(p, "announced_time", cJSON_GetObjectItem(d, "AnnouncedTime"));
    coalesce_null(p, "origin_time", cJSON_GetObjectItem(d, "OriginTime"));
    coalesce_null(p, "is_final", cJSON_GetObjectItem(d, "isFinal"));
    coalesce_null(p, "is_cancel", cJSON_GetObjectItem(d, "isCancel"));
    coalesce_null(p, "is_warn", cJSON_GetObjectItem(d, "isWarn"));
    cJSON_AddStringToObject(p, "source", "wolfx_jma_eew");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  if (d) cJSON_Delete(d);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[wolfx-eew] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def wolfx_eew_def = {
  .id = "wolfx-eew", .collector = "environment",
  .name = "Wolfx JMA Earthquake Early Warning", .name_ja = "Wolfx 緊急地震速報",
   .update_interval_sec = 10, .run = run,
};
REGISTER_SOURCE(wolfx_eew_def)
