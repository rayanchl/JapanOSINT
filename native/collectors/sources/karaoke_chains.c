/* collectors/culture/sources/karaoke_chains.c — port of
 * server/src/collectors/karaokeChains.js (fetchOverpass single area.jp).
 * SEED_KARAOKE offline fallback intentionally not ported (rule 8). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  char kb[48];
  snprintf(kb, sizeof kb, "OSM_%lld", oid);
  cJSON_AddStringToObject(p, "karaoke_id", kb);

  const char *en = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  if (en) {
    cJSON_AddStringToObject(p, "name", en);
  } else if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char nb[32];
    snprintf(nb, sizeof nb, "Karaoke %d", i + 1);
    cJSON_AddStringToObject(p, "name", nb);
  }
  cJSON_AddItemToObject(p, "name_ja",
                        nm ? cJSON_CreateString(nm) : cJSON_CreateNull());

  const char *brand = ov_tag(el, "brand");
  cJSON_AddItemToObject(p, "brand",
                        brand ? cJSON_CreateString(brand)
                              : cJSON_CreateNull());

  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator",
                        op ? cJSON_CreateString(op) : cJSON_CreateNull());

  const char *oh = ov_tag(el, "opening_hours");
  cJSON_AddItemToObject(p, "opening_hours",
                        oh ? cJSON_CreateString(oh) : cJSON_CreateNull());

  const char *rooms = ov_tag(el, "rooms");
  cJSON_AddItemToObject(p, "rooms",
                        rooms ? cJSON_CreateString(rooms)
                              : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"karaoke_box\"](area.jp);"
    "way[\"amenity\"=\"karaoke_box\"](area.jp);"
    "node[\"shop\"=\"karaoke\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def karaoke_chains_def = {
  .id = "karaoke-chains", .collector = "culture",
  .name = "Karaoke Chains", .name_ja = "カラオケチェーン",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(karaoke_chains_def)
