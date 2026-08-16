/* collectors/defense/sources/coast_guard_stations.c — port of
 * server/src/collectors/coastGuardStations.js (fetchOverpass single area.jp).
 * SEED_STATIONS offline fallback intentionally not ported (rule 8). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>
#include <string.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char sb[48];
  snprintf(sb, sizeof sb, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "station_id", sb);

  const char *en = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  const char *name = en ? en : nm;
  cJSON_AddStringToObject(p, "name", name ? name : "JCG Station");
  cJSON_AddItemToObject(p, "name_ja",
                        nm ? cJSON_CreateString(nm) : cJSON_CreateNull());

  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator",
                        op ? cJSON_CreateString(op) : cJSON_CreateNull());

  const char *am = ov_tag(el, "amenity");
  cJSON_AddStringToObject(p, "kind",
                          (am && strcmp(am, "coast_guard") == 0)
                              ? "coast_guard"
                              : "station");

  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"operator\"~\"海上保安\"](area.jp);"
    "way[\"operator\"~\"海上保安\"](area.jp);"
    "node[\"amenity\"=\"coast_guard\"](area.jp);"
    "way[\"amenity\"=\"coast_guard\"](area.jp);"
    "node[\"office\"=\"government\"][\"government\"=\"coast_guard\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def coast_guard_stations_def = {
  .id = "coast-guard-stations", .collector = "defense",
  .name = "Coast Guard Stations", .name_ja = "海上保安部・署",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(coast_guard_stations_def)
