/* collectors/government/sources/city_halls.c — port of
 * server/src/collectors/cityHalls.js (fetchOverpass single area.jp).
 * SEED_CITY_HALLS offline fallback intentionally not ported (rule 8). */
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <string.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
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
  char hb[48];
  snprintf(hb, sizeof hb, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "hall_id", hb);

  const char *en = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  const char *name = en ? en : nm;
  cJSON_AddStringToObject(p, "name", name ? name : "City Hall");
  cJSON_AddItemToObject(p, "name_ja",
                        nm ? cJSON_CreateString(nm) : cJSON_CreateNull());

  const char *am = ov_tag(el, "amenity");
  const char *gov = ov_tag(el, "government");
  if (am && strcmp(am, "townhall") == 0)
    cJSON_AddStringToObject(p, "kind", "townhall");
  else
    cJSON_AddStringToObject(p, "kind", gov ? gov : "government_office");

  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator",
                        op ? cJSON_CreateString(op) : cJSON_CreateNull());

  const char *addr = ov_tag(el, "addr:full");
  if (!addr) addr = ov_tag(el, "addr:city");
  cJSON_AddItemToObject(p, "addr",
                        addr ? cJSON_CreateString(addr) : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"townhall\"](area.jp);"
    "way[\"amenity\"=\"townhall\"](area.jp);"
    "node[\"office\"=\"government\"]"
    "[\"government\"~\"municipal|prefecture|cabinet\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def city_halls_def = {
  .id = "city-halls", .collector = "government",
  .name = "City Halls", .name_ja = "市区町村役場",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(city_halls_def)
