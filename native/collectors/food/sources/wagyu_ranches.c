/* collectors/food/sources/wagyu_ranches.c — port of
 * server/src/collectors/wagyuRanches.js. fetchOverpass (single area.jp
 * query, tryLive). SEED_RANCHES offline fallback intentionally not ported
 * (JS does `if (!live) features = []`). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>
#include <string.h>

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
  char rid[64];
  snprintf(rid, sizeof rid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "ranch_id", rid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Ranch %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
  const char *name_ja = ov_tag(el, "name");
  if (name_ja) cJSON_AddStringToObject(p, "name_ja", name_ja);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  const char *operator_ = ov_tag(el, "operator");
  if (operator_) cJSON_AddStringToObject(p, "operator", operator_);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  const char *livestock = ov_tag(el, "livestock");
  cJSON_AddStringToObject(p, "livestock", livestock ? livestock : "cattle");
  const char *shop = ov_tag(el, "shop");
  cJSON_AddStringToObject(p, "kind",
                          shop && strcmp(shop, "butcher") == 0 ? "retailer"
                                                               : "ranch");
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"landuse\"=\"farmyard\"][\"name\"~\"牧場|和牛|牛|ranch|cattle\"](area.jp);"
    "way[\"landuse\"=\"farmyard\"][\"name\"~\"牧場|和牛|牛\"](area.jp);"
    "node[\"shop\"=\"butcher\"][\"name\"~\"松阪|神戸|近江|飛騨|米沢|前沢|仙台|宮崎|鹿児島|和牛\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def wagyu_ranches_def = {
  .id = "wagyu-ranches", .collector = "food", .name = "Wagyu Ranches",
  .name_ja = "和牛産地", 
  .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(wagyu_ranches_def)
