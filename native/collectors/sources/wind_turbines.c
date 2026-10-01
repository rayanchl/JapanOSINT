/* collectors/industry/sources/wind_turbines.c — port of
 * server/src/collectors/windTurbines.js. fetchOverpass (single area.jp
 * query, tryOverpass). SEED_WIND offline fallback intentionally not ported
 * (JS does `if (!live) features = []`). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char tid[64];
  snprintf(tid, sizeof tid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "turbine_id", tid);
  const char *name = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", name ? name : "Wind Turbine");
  /* JS: parseFloat(tags['generator:output:electricity'] || '0') / 1e6
   * — parseFloat tolerates trailing units ("2400000 W"); always a number. */
  const char *out = ov_tag(el, "generator:output:electricity");
  double mw = (out ? atof(out) : 0.0) / 1000000.0;
  cJSON_AddItemToObject(p, "capacity_mw", cJSON_CreateNumber(mw));
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"power\"=\"generator\"][\"generator:source\"=\"wind\"](area.jp);"
    "way[\"power\"=\"generator\"][\"generator:source\"=\"wind\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def wind_turbines_def = {
  .id = "wind-turbines", .collector = "industry", .name = "Wind Turbines",
  .name_ja = "風力発電所", 
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(wind_turbines_def)
