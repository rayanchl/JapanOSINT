/* collectors/culture/sources/themed_cafes.c — port of
 * server/src/collectors/themedCafes.js. fetchOverpass (single area.jp query,
 * tryLive). SEED_CAFES offline fallback intentionally not ported (JS does
 * `if (!live) features = []`). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char cid[64];
  snprintf(cid, sizeof cid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "cafe_id", cid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  /* no-fabrication (house rule 1): OSM carried no name tag for this element.
   * The old code wrote "Themed Cafe %d" + the loop index, which is both an invented
   * label and an UNSTABLE one — it feeds geojson's content-hash uid, so the
   * same object was re-keyed whenever Overpass changed element order. An
   * absent name is serialized as null; pick_text() skips nulls, so the row
   * persists with a NULL title rather than a made-up one. */
  if (name) cJSON_AddStringToObject(p, "name", name);
  else cJSON_AddItemToObject(p, "name", cJSON_CreateNull());
  const char *name_ja = ov_tag(el, "name");
  if (name_ja) cJSON_AddStringToObject(p, "name_ja", name_ja);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  const char *theme = ov_tag(el, "cafe:type");
  if (!theme) theme = ov_tag(el, "animal");
  if (theme) cJSON_AddStringToObject(p, "theme", theme);
  else cJSON_AddItemToObject(p, "theme", cJSON_CreateNull());
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
    180, 200000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def themed_cafes_def = {
  .id = "themed-cafes", .collector = "culture", .name = "Themed Cafes",
  .name_ja = "コンセプトカフェ", 
  .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(themed_cafes_def)
