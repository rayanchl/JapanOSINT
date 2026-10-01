/* collectors/telecom/sources/submarine_cables.c — port of
 * server/src/collectors/submarineCables.js. fetchOverpass (single area.jp
 * query, tryLive). SEED_CABLES offline fallback intentionally not ported (JS
 * does `if (!live) features = []`). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char cid[64];
  snprintf(cid, sizeof cid, "CBL_LIVE_%05d", i + 1);
  cJSON_AddStringToObject(p, "cable_id", cid);
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  char nbuf[64];
  if (!name) {
    snprintf(nbuf, sizeof nbuf, "Cable landing %lld",
             id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
    name = nbuf;
  }
  cJSON_AddStringToObject(p, "name", name);
  const char *operator_ = ov_tag(el, "operator");
  if (operator_) cJSON_AddStringToObject(p, "operator", operator_);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  const char *region = ov_tag(el, "addr:state");
  if (region) cJSON_AddStringToObject(p, "region", region);
  else cJSON_AddItemToObject(p, "region", cJSON_CreateNull());
  const char *endpoints = ov_tag(el, "communication:submarine_cable");
  cJSON_AddStringToObject(p, "endpoints", endpoints ? endpoints : "multi");
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "submarine_cables_live");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"telecom\"=\"connection_point\"](area.jp);"
    "way[\"submarine\"=\"yes\"](area.jp);"
    "node[\"communication:submarine_cable\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def submarine_cables_def = {
  .id = "submarine-cables", .collector = "telecom",
  .name = "Submarine Cable Landings", .name_ja = "海底ケーブル陸揚局",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(submarine_cables_def)
