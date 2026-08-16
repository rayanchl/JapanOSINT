/* collectors/government/sources/embassies.c
 * Port of server/src/collectors/embassies.js (fetchOverpass — single area.jp
 * query). SEED_EMBASSIES offline fallback intentionally not ported (JS does
 * `if (!live) features = []` anyway). REFERENCE source.c for the OVERPASS
 * (single, non-tiled) family. */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char eid[64];
  snprintf(eid, sizeof eid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "embassy_id", eid);
  const char *name = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", name ? name : "Embassy");
  const char *country = ov_tag(el, "country");
  if (!country) country = ov_tag(el, "target:country");
  cJSON_AddStringToObject(p, "country", country ? country : "unknown");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"diplomatic\"=\"embassy\"](area.jp);"
    "way[\"diplomatic\"=\"embassy\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def embassies_def = {
  .id = "embassies", .collector = "government", .name = "Embassies",
  .name_ja = "大使館", 
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(embassies_def)
