/* collectors/food/sources/sake_breweries.c — port of
 * server/src/collectors/sakeBreweries.js. fetchOverpass (single area.jp
 * query, tryLive). SEED_BREWERIES offline fallback intentionally not ported
 * (JS does `if (!live) features = []`). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char bid[64];
  snprintf(bid, sizeof bid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "brewery_id", bid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Brewery %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
  const char *name_ja = ov_tag(el, "name");
  if (name_ja) cJSON_AddStringToObject(p, "name_ja", name_ja);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  const char *founded = ov_tag(el, "start_date");
  if (founded) cJSON_AddStringToObject(p, "founded", founded);
  else cJSON_AddItemToObject(p, "founded", cJSON_CreateNull());
  const char *website = ov_tag(el, "website");
  if (website) cJSON_AddStringToObject(p, "website", website);
  else cJSON_AddItemToObject(p, "website", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"craft\"=\"brewery\"][\"produces\"=\"sake\"](area.jp);"
    "way[\"craft\"=\"brewery\"][\"produces\"=\"sake\"](area.jp);"
    "node[\"craft\"=\"brewery\"][\"name\"~\"酒造\"](area.jp);"
    "way[\"craft\"=\"brewery\"][\"name\"~\"酒造\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def sake_breweries_def = {
  .id = "sake-breweries", .collector = "food", .name = "Sake Breweries",
  .name_ja = "日本酒蔵元", 
  .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(sake_breweries_def)
