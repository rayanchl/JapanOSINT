/* collectors/geospatial/sources/admin_boundaries.c — port of
 * server/src/collectors/adminBoundaries.js (fetchOverpass single area.jp).
 * JS: fetchOverpass(body, mapFn, 30000, {limit:2000,queryTimeout:90}).
 * The `limit` (out-clause cap) is not part of overpass_collect — count-only,
 * correctness-neutral for emitted rows (toolkit uses `out center;`). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>

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
  char idb[48];
  snprintf(idb, sizeof idb, "ADM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "id", idb);

  const char *en = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  const char *name = en ? en : nm;
  cJSON_AddItemToObject(p, "name",
                        name ? cJSON_CreateString(name) : cJSON_CreateNull());
  cJSON_AddItemToObject(p, "name_ja",
                        nm ? cJSON_CreateString(nm) : cJSON_CreateNull());

  const char *al = ov_tag(el, "admin_level");
  cJSON_AddItemToObject(p, "admin_level",
                        al ? cJSON_CreateNumber(strtod(al, 0))
                           : cJSON_CreateNull());

  const char *place = ov_tag(el, "place");
  cJSON_AddItemToObject(p, "place",
                        place ? cJSON_CreateString(place)
                              : cJSON_CreateNull());

  const char *iso = ov_tag(el, "ISO3166-2");
  cJSON_AddItemToObject(p, "iso_code",
                        iso ? cJSON_CreateString(iso) : cJSON_CreateNull());

  const char *wd = ov_tag(el, "wikidata");
  cJSON_AddItemToObject(p, "wikidata",
                        wd ? cJSON_CreateString(wd) : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "relation[\"boundary\"=\"administrative\"][\"admin_level\"=\"4\"](area.jp);"
    "relation[\"boundary\"=\"administrative\"][\"admin_level\"=\"7\"](area.jp);",
    90, 30000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def admin_boundaries_def = {
  .id = "admin-boundaries", .collector = "geospatial",
  .name = "OSM Administrative Boundaries", .name_ja = "OSM 行政境界",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(admin_boundaries_def)
