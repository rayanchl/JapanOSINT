/* collectors/telecom/sources/satellite_ground_stations.c
 * Port of server/src/collectors/satelliteGroundStations.js.
 * Faithful live path = tryLive() fetchOverpass (man_made=satellite_dish +
 * building=observatory). GEONET CSV merge and the SEED_* arrays are not
 * ported (curated/seed; rule 7). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>
#include <string.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
  char gsid[32];
  snprintf(gsid, sizeof gsid, "GS_LIVE_%05d", i + 1);
  cJSON_AddStringToObject(p, "gs_id", gsid);

  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  if (name) {
    cJSON_AddStringToObject(p, "name", name);
  } else {
    cJSON *id = cJSON_GetObjectItem(el, "id");
    char nb[64];
    snprintf(nb, sizeof nb, "Ground station %lld",
             id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
    cJSON_AddStringToObject(p, "name", nb);
  }

  const char *op = ov_tag(el, "operator");
  if (op) cJSON_AddStringToObject(p, "operator", op);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());

  const char *building = ov_tag(el, "building");
  int obs = building && !strcmp(building, "observatory");
  cJSON_AddStringToObject(p, "kind", obs ? "observatory" : "satellite_dish");
  cJSON_AddStringToObject(p, "category", obs ? "vlbi" : "satcom");

  const char *bands = ov_tag(el, "satellite:bands");
  if (bands) cJSON_AddStringToObject(p, "bands", bands);
  else cJSON_AddItemToObject(p, "bands", cJSON_CreateNull());

  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "satellite_gs_live");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"man_made\"=\"satellite_dish\"](area.jp);"
    "way[\"man_made\"=\"satellite_dish\"](area.jp);"
    "node[\"building\"=\"observatory\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def satellite_ground_stations_def = {
  .id = "satellite-ground-stations", .collector = "telecom",
  .name = "Satellite Ground Stations", .name_ja = "衛星地上局",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(satellite_ground_stations_def)
