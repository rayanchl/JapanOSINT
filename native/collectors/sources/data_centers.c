/* collectors/telecom/sources/data_centers.c — port of
 * server/src/collectors/dataCenters.js (fetchOverpass single area.jp).
 * SEED_DC offline fallback intentionally not ported (rule 8). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char db[48];
  snprintf(db, sizeof db, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "dc_id", db);

  const char *name = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "name", name ? name : "Data Center");

  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "unknown");

  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"telecom\"=\"data_center\"](area.jp);"
    "way[\"telecom\"=\"data_center\"](area.jp);"
    "node[\"building\"=\"data_center\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def data_centers_def = {
  .id = "data-centers", .collector = "telecom",
  .name = "Data Centers", .name_ja = "データセンター",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(data_centers_def)
