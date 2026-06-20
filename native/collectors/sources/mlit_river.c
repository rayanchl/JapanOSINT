/* collectors/infrastructure/sources/mlit_river.c — port of
 * server/src/collectors/mlitRiver.js. The JS tryMlitJson path iterates an
 * EMPTY API_URLS list (no verified public JSON source) and always returns
 * null, so the faithful reachable live path is tryOsmGauges: an OSM Overpass
 * water-monitoring proxy (single area.jp query, timeoutMs 60000 /
 * queryTimeout 120). The curated RIVER_STATIONS seed / _meta envelope is
 * intentionally not ported (JS does `features = []` when nothing live). */
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char sid[64];
  snprintf(sid, sizeof sid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "station_id", sid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "station_name", name ? name : "River gauge");
  const char *op = ov_tag(el, "operator");
  if (op) cJSON_AddStringToObject(p, "operator", op);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  const char *ref = ov_tag(el, "ref");
  if (ref) cJSON_AddStringToObject(p, "ref", ref);
  else cJSON_AddItemToObject(p, "ref", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"man_made\"=\"monitoring_station\"][\"monitoring:water_level\"=\"yes\"](area.jp);"
    "node[\"man_made\"=\"water_well\"](area.jp);"
    "node[\"waterway\"=\"water_point\"](area.jp);",
    120, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def mlit_river_def = {
  .id = "mlit-river", .collector = "infrastructure",
  .name = "MLIT River Water Levels", .name_ja = "国交省 河川水位情報",
   .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(mlit_river_def)
