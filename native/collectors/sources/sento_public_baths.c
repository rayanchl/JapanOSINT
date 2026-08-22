/* collectors/culture/sources/sento_public_baths.c — port of
 * server/src/collectors/sentoPublicBaths.js. fetchOverpass (single area.jp
 * query, tryLive). SEED_SENTO offline fallback intentionally not ported (JS
 * does `if (!live) features = []`). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>
#include <string.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char sid[64];
  snprintf(sid, sizeof sid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "sento_id", sid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  /* no-fabrication (house rule 1): OSM carried no name tag for this element.
   * The old code wrote "Sento %d" + the loop index, which is both an invented
   * label and an UNSTABLE one — it feeds geojson's content-hash uid, so the
   * same object was re-keyed whenever Overpass changed element order. An
   * absent name is serialized as null; pick_text() skips nulls, so the row
   * persists with a NULL title rather than a made-up one. */
  if (name) cJSON_AddStringToObject(p, "name", name);
  else cJSON_AddItemToObject(p, "name", cJSON_CreateNull());
  const char *name_ja = ov_tag(el, "name");
  if (name_ja) cJSON_AddStringToObject(p, "name_ja", name_ja);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  const char *oh = ov_tag(el, "opening_hours");
  if (oh) cJSON_AddStringToObject(p, "opening_hours", oh);
  else cJSON_AddItemToObject(p, "opening_hours", cJSON_CreateNull());
  const char *fee = ov_tag(el, "fee");
  if (fee) cJSON_AddStringToObject(p, "fee", fee);
  else cJSON_AddItemToObject(p, "fee", cJSON_CreateNull());
  const char *sauna = ov_tag(el, "sauna");
  cJSON_AddBoolToObject(p, "sauna", sauna && strcmp(sauna, "yes") == 0);
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"public_bath\"][\"bath:type\"!=\"onsen\"](area.jp);"
    "way[\"amenity\"=\"public_bath\"][\"bath:type\"!=\"onsen\"](area.jp);"
    "node[\"amenity\"=\"public_bath\"][\"name\"~\"湯$\"](area.jp);"
    "way[\"amenity\"=\"public_bath\"][\"name\"~\"湯$\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def sento_public_baths_def = {
  .id = "sento-public-baths", .collector = "culture",
  .name = "Sento Public Baths", .name_ja = "銭湯",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(sento_public_baths_def)
