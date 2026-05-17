/* collectors/infrastructure/sources/electrical_grid.c — port of
 * server/src/collectors/electricalGrid.js (fetchOverpass single area.jp).
 * POWER_FACILITIES offline fallback intentionally not ported (rule 8). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>
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

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  char pb[48];
  snprintf(pb, sizeof pb, "OSM_%lld", oid);
  cJSON_AddStringToObject(p, "power_id", pb);

  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char nb[40];
    snprintf(nb, sizeof nb, "Power facility %d", i + 1);
    cJSON_AddStringToObject(p, "name", nb);
  }

  const char *pw = ov_tag(el, "power");
  cJSON_AddStringToObject(p, "type",
                          (pw && strcmp(pw, "plant") == 0) ? "plant"
                                                           : "substation");

  const char *fuel = ov_tag(el, "plant:source");
  if (!fuel) fuel = ov_tag(el, "generator:source");
  cJSON_AddStringToObject(p, "fuel", fuel ? fuel : "unknown");

  const char *cap = ov_tag(el, "plant:output:electricity");
  double capv = cap ? strtod(cap, 0) : 0.0;
  cJSON_AddItemToObject(p, "capacity_mw",
                        capv != 0.0 ? cJSON_CreateNumber(capv)
                                    : cJSON_CreateNull());

  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "unknown");

  const char *volt = ov_tag(el, "voltage");
  cJSON_AddItemToObject(p, "voltage",
                        volt ? cJSON_CreateString(volt) : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"power\"=\"plant\"](area.jp);"
    "way[\"power\"=\"plant\"](area.jp);"
    "node[\"power\"=\"substation\"]"
    "[\"substation\"!=\"minor_distribution\"](area.jp);"
    "way[\"power\"=\"substation\"]"
    "[\"substation\"!=\"minor_distribution\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def electrical_grid_def = {
  .id = "electrical-grid", .collector = "infrastructure",
  .name = "Electrical Grid", .name_ja = "電力網",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(electrical_grid_def)
