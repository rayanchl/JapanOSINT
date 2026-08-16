/* collectors/culture/sources/onsen_map.c — port of server/src/collectors/onsenMap.js
 * fetchOverpass (single area.jp query). SEED_ONSEN offline fallback
 * intentionally not ported (JS does `if (!live) features = []`). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char oid[64];
  snprintf(oid, sizeof oid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "onsen_id", oid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Onsen %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
  const char *name_ja = ov_tag(el, "name");
  if (name_ja) cJSON_AddStringToObject(p, "name_ja", name_ja);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  const char *bath_type = ov_tag(el, "bath:type");
  cJSON_AddStringToObject(p, "bath_type", bath_type ? bath_type : "public_bath");
  const char *operator_ = ov_tag(el, "operator");
  if (operator_) cJSON_AddStringToObject(p, "operator", operator_);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  const char *website = ov_tag(el, "website");
  if (website) cJSON_AddStringToObject(p, "website", website);
  else cJSON_AddItemToObject(p, "website", cJSON_CreateNull());
  const char *wikidata = ov_tag(el, "wikidata");
  if (wikidata) cJSON_AddStringToObject(p, "wikidata", wikidata);
  else cJSON_AddItemToObject(p, "wikidata", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"public_bath\"][\"bath:type\"=\"onsen\"](area.jp);"
    "node[\"natural\"=\"hot_spring\"](area.jp);"
    "way[\"natural\"=\"hot_spring\"](area.jp);"
    "node[\"amenity\"=\"public_bath\"][\"name\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def onsen_map_def = {
  .id = "onsen-map", .collector = "culture", .name = "Onsen (Hot Springs)",
  .name_ja = "温泉", 
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(onsen_map_def)
