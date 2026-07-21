/* collectors/culture/sources/themed_cafes.c — port of
 * server/src/collectors/themedCafes.js. fetchOverpass (single area.jp query,
 * tryLive). SEED_CAFES offline fallback intentionally not ported (JS does
 * `if (!live) features = []`). */
#include "../../source.h"
#include "../../lib/overpass.h"
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
  char cid[64];
  snprintf(cid, sizeof cid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "cafe_id", cid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Themed Cafe %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
  const char *name_ja = ov_tag(el, "name");
  if (name_ja) cJSON_AddStringToObject(p, "name_ja", name_ja);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  const char *theme = ov_tag(el, "cafe:type");
  if (!theme) theme = ov_tag(el, "animal");
  cJSON_AddStringToObject(p, "theme", theme ? theme : "unknown");
  const char *operator_ = ov_tag(el, "operator");
  if (operator_) cJSON_AddStringToObject(p, "operator", operator_);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"cafe\"][\"cafe:type\"](area.jp);"
    "node[\"amenity\"=\"cafe\"][\"animal\"](area.jp);"
    "node[\"amenity\"=\"cafe\"][\"name\"~\"メイド|cat cafe|owl cafe|maid\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def themed_cafes_def = {
  .id = "themed-cafes", .collector = "culture", .name = "Themed Cafes",
  .name_ja = "コンセプトカフェ", 
  .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(themed_cafes_def)
