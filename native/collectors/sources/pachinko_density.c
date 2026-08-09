/* collectors/crime/sources/pachinko_density.c — port of
 * server/src/collectors/pachinkoDensity.js. fetchOverpass (single area.jp
 * query). SEED_DENSITY offline fallback intentionally not ported (JS does
 * `if (!live) features = []`). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char pid[64];
  snprintf(pid, sizeof pid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "parlor_id", pid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Pachinko %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
  const char *name_ja = ov_tag(el, "name");
  if (name_ja) cJSON_AddStringToObject(p, "name_ja", name_ja);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  const char *operator_ = ov_tag(el, "operator");
  if (operator_) cJSON_AddStringToObject(p, "operator", operator_);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  const char *machines = ov_tag(el, "capacity");
  if (machines) cJSON_AddStringToObject(p, "machines", machines);
  else cJSON_AddItemToObject(p, "machines", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"leisure\"=\"adult_gaming_centre\"](area.jp);"
    "way[\"leisure\"=\"adult_gaming_centre\"](area.jp);"
    "node[\"shop\"=\"games\"][\"name\"~\"パチンコ\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def pachinko_density_def = {
  .id = "pachinko-density", .collector = "crime",
  .name = "Pachinko Parlor Density", .name_ja = "パチンコ店密度",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(pachinko_density_def)
