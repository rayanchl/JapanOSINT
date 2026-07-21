/* collectors/environment/sources/nerv_feed.c
 * Port of server/src/collectors/nervFeed.js. NERV multi-hazard alerts JSON
 * (array or {alerts:[]}). No natural id → sha1 hash-fallback uid. properties
 * {kind,headline,severity,issued_at,source} in exact JS order with the same
 * ?? fallbacks so the serialization (hence hash) matches Node. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../core/intel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://unii-api.nerv.app/v1/lib/alerts.json"

/* a.x ?? a.y : first present, non-null cJSON (string/number), else NULL. */
static cJSON *coalesce(cJSON *o, const char *a, const char *b) {
  cJSON *v = cJSON_GetObjectItem(o, a);
  if (v && !cJSON_IsNull(v)) return v;
  if (b) { v = cJSON_GetObjectItem(o, b); if (v && !cJSON_IsNull(v)) return v; }
  return NULL;
}
static double num(cJSON *v) {
  if (!v) return 0;
  if (cJSON_IsNumber(v)) return v->valuedouble;
  if (cJSON_IsString(v)) return atof(v->valuestring);
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *data = feed_get_json(ctx->http, API_URL, 8000);
  if (!data) return -1;
  cJSON *arr = cJSON_IsArray(data) ? data : cJSON_GetObjectItem(data, "alerts");

  cJSON *features = cJSON_CreateArray();
  cJSON *a;
  cJSON_ArrayForEach(a, arr) {
    if (!cJSON_IsObject(a)) continue;
    /* lat = latitude ?? lat ?? location.lat ; same for lon */
    cJSON *loc = cJSON_GetObjectItem(a, "location");
    cJSON *latv = coalesce(a, "latitude", "lat");
    if (!latv && loc) latv = cJSON_GetObjectItem(loc, "lat");
    cJSON *lonv = coalesce(a, "longitude", "lon");
    if (!lonv && loc) lonv = cJSON_GetObjectItem(loc, "lon");
    if (!latv || !lonv || cJSON_IsNull(latv) || cJSON_IsNull(lonv)) continue;

    cJSON *feat = cJSON_CreateObject();
    cJSON_AddStringToObject(feat, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(num(lonv)));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(num(latv)));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(feat, "geometry", g);

    cJSON *p = cJSON_CreateObject();
    cJSON *kind = coalesce(a, "kind", "type");
    cJSON_AddItemToObject(p, "kind", kind ? cJSON_Duplicate(kind,1) : cJSON_CreateNull());
    cJSON *hl = coalesce(a, "headline", "title");
    cJSON_AddItemToObject(p, "headline", hl ? cJSON_Duplicate(hl,1) : cJSON_CreateNull());
    cJSON *sv = cJSON_GetObjectItem(a, "severity");
    cJSON_AddItemToObject(p, "severity",
        (sv && !cJSON_IsNull(sv)) ? cJSON_Duplicate(sv,1) : cJSON_CreateNull());
    cJSON *iss = coalesce(a, "issued", "time");
    cJSON_AddItemToObject(p, "issued_at", iss ? cJSON_Duplicate(iss,1) : cJSON_CreateNull());
    cJSON_AddStringToObject(p, "source", "nerv");
    cJSON_AddItemToObject(feat, "properties", p);
    cJSON_AddItemToArray(features, feat);
  }
  cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[nerv-feed] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def nerv_feed_def = {
  .id = "nerv-feed", .collector = "environment",
  .name = "NERV Disaster Alerts", .name_ja = "特務機関NERV",
   .update_interval_sec = 60, .run = run,
};
REGISTER_SOURCE(nerv_feed_def)
