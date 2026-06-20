/* collectors/tourism/sources/national_parks.c — port of
 * server/src/collectors/nationalParks.js (fetchOverpass single area.jp).
 * SEED_PARKS offline fallback intentionally not ported (rule 8). */
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
  char pb[32];
  snprintf(pb, sizeof pb, "PARK_LIVE_%05d", i + 1);
  cJSON_AddStringToObject(p, "park_id", pb);

  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char nb[32];
    snprintf(nb, sizeof nb, "Park %lld", oid);
    cJSON_AddStringToObject(p, "name", nb);
  }

  const char *bd = ov_tag(el, "boundary");
  cJSON_AddStringToObject(p, "kind",
                          (bd && strcmp(bd, "national_park") == 0)
                              ? "national"
                              : "protected_area");

  const char *pc = ov_tag(el, "protect_class");
  cJSON_AddItemToObject(p, "protect_class",
                        pc ? cJSON_CreateString(pc) : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "national_parks_live");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "relation[\"boundary\"=\"national_park\"](area.jp);"
    "relation[\"boundary\"=\"protected_area\"]"
    "[\"protect_class\"=\"2\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def national_parks_def = {
  .id = "national-parks", .collector = "tourism",
  .name = "National Parks", .name_ja = "国立公園",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(national_parks_def)
