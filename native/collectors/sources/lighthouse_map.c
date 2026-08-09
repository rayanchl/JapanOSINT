/* collectors/transport/sources/lighthouse_map.c — port of
 * server/src/collectors/lighthouseMap.js (fetchOverpass single area.jp).
 * SEED_LIGHTHOUSES offline fallback intentionally not ported (rule 8).
 * JS `.slice(0,500)` cap is count-only / correctness-neutral; omitted. */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char lb[52];
  snprintf(lb, sizeof lb, "LH_OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "lighthouse_id", lb);

  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:ja");
  cJSON_AddStringToObject(p, "name", nm ? nm : "Lighthouse");

  const char *h = ov_tag(el, "height");
  double hv = h ? strtod(h, 0) : 0.0;
  cJSON_AddItemToObject(p, "height_m",
                        hv != 0.0 ? cJSON_CreateNumber(hv)
                                  : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"man_made\"=\"lighthouse\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def lighthouse_map_def = {
  .id = "lighthouse-map", .collector = "transport",
  .name = "Lighthouses", .name_ja = "灯台マップ",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(lighthouse_map_def)
