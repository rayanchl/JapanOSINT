/* collectors/food/sources/wineries_craftbeer.c — port of
 * server/src/collectors/wineriesCraftbeer.js. fetchOverpass (single area.jp
 * query, tryLive). SEED_FACILITIES offline fallback intentionally not ported
 * (JS does `if (!live) features = []`). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char fid[64];
  snprintf(fid, sizeof fid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "facility_id", fid);
  const char *name = ov_tag(el, "name:en");
  if (!name) name = ov_tag(el, "name");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Facility %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
  const char *name_ja = ov_tag(el, "name");
  if (name_ja) cJSON_AddStringToObject(p, "name_ja", name_ja);
  else cJSON_AddItemToObject(p, "name_ja", cJSON_CreateNull());
  const char *category = ov_tag(el, "craft");
  if (category) cJSON_AddStringToObject(p, "category", category);
  else cJSON_AddItemToObject(p, "category", cJSON_CreateNull());
  const char *produces = ov_tag(el, "produces");
  if (produces) cJSON_AddStringToObject(p, "produces", produces);
  else cJSON_AddItemToObject(p, "produces", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"craft\"=\"winery\"](area.jp);"
    "way[\"craft\"=\"winery\"](area.jp);"
    "node[\"craft\"=\"brewery\"][\"produces\"!=\"sake\"](area.jp);"
    "way[\"craft\"=\"brewery\"][\"produces\"!=\"sake\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def wineries_craftbeer_def = {
  .id = "wineries-craftbeer", .collector = "food",
  .name = "Wineries & Craft Beer", .name_ja = "ワイナリー・地ビール・蒸溜所",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(wineries_craftbeer_def)
