/* collectors/culture/sources/vending_machines.c — port of
 * server/src/collectors/vendingMachines.js. fetchOverpass (single query,
 * tryLive — bbox-sampled, body verbatim). SEED_ZONES offline fallback
 * intentionally not ported (JS does `if (!live) features = []`). */
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
  char vid[64];
  snprintf(vid, sizeof vid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "vm_id", vid);
  const char *vending = ov_tag(el, "vending");
  cJSON_AddStringToObject(p, "vending", vending ? vending : "drinks");
  const char *operator_ = ov_tag(el, "operator");
  if (operator_) cJSON_AddStringToObject(p, "operator", operator_);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  const char *brand = ov_tag(el, "brand");
  if (brand) cJSON_AddStringToObject(p, "brand", brand);
  else cJSON_AddItemToObject(p, "brand", cJSON_CreateNull());
  /* el.tags['payment:coins']==='yes' ? 'coin'
   *   : (el.tags['payment:cards']==='yes' ? 'card' : 'coin') */
  const char *coins = ov_tag(el, "payment:coins");
  const char *cards = ov_tag(el, "payment:cards");
  const char *payment = (coins && strcmp(coins, "yes") == 0)
                          ? "coin"
                          : ((cards && strcmp(cards, "yes") == 0) ? "card"
                                                                  : "coin");
  cJSON_AddStringToObject(p, "payment", payment);
  const char *indoor = ov_tag(el, "indoor");
  cJSON_AddBoolToObject(p, "indoor", indoor && strcmp(indoor, "yes") == 0);
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"vending_machine\"](35.6,139.6,35.8,139.85);"
    "node[\"amenity\"=\"vending_machine\"](34.6,135.4,34.75,135.6);"
    "node[\"amenity\"=\"vending_machine\"](35.1,136.8,35.25,137.0);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def vending_machines_def = {
  .id = "vending-machines", .collector = "culture",
  .name = "Vending Machines", .name_ja = "自動販売機",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(vending_machines_def)
