/* collectors/culture/sources/sento_public_baths.c — port of
 * server/src/collectors/sentoPublicBaths.js. fetchOverpass (single area.jp
 * query, tryLive). SEED_SENTO offline fallback intentionally not ported (JS
 * does `if (!live) features = []`). */
#include "../../source.h"
#include "../../lib/overpass.h"
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
  char sid[64];
  snprintf(sid, sizeof sid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "sento_id", sid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Sento %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
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
