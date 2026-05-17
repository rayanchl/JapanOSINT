/* collectors/food/sources/fish_markets.c — port of
 * server/src/collectors/fishMarkets.js (fetchOverpass single area.jp).
 * SEED_MARKETS offline fallback intentionally not ported (rule 8). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>

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
  char mb[48];
  snprintf(mb, sizeof mb, "OSM_%lld", oid);
  cJSON_AddStringToObject(p, "market_id", mb);

  const char *en = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  if (en) {
    cJSON_AddStringToObject(p, "name", en);
  } else if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char nb[40];
    snprintf(nb, sizeof nb, "Market %d", i + 1);
    cJSON_AddStringToObject(p, "name", nb);
  }
  cJSON_AddItemToObject(p, "name_ja",
                        nm ? cJSON_CreateString(nm) : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"marketplace\"]"
    "[\"name\"~\"魚|水産|卸売|市場\"](area.jp);"
    "way[\"amenity\"=\"marketplace\"]"
    "[\"name\"~\"魚|水産|卸売|市場\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def fish_markets_def = {
  .id = "fish-markets", .collector = "food",
  .name = "Fish Markets", .name_ja = "魚市場・水産卸売",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(fish_markets_def)
