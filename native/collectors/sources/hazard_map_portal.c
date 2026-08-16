/* collectors/safety/sources/hazard_map_portal.c — port of
 * server/src/collectors/hazardMapPortal.js (fetchOverpass single area.jp).
 * HAZARD_ZONES offline fallback intentionally not ported (rule 8). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>
#include <string.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  char hb[32];
  snprintf(hb, sizeof hb, "HAZ_LIVE_%05d", i + 1);
  cJSON_AddStringToObject(p, "hazard_id", hb);

  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char nb[40];
    snprintf(nb, sizeof nb, "Hazard %lld", oid);
    cJSON_AddStringToObject(p, "name", nb);
  }

  const char *nat = ov_tag(el, "natural");
  const char *haz = ov_tag(el, "hazard");
  if (nat && strcmp(nat, "volcano") == 0)
    cJSON_AddStringToObject(p, "hazard", "volcano");
  else
    cJSON_AddStringToObject(p, "hazard", haz ? haz : "unknown");

  const char *st = ov_tag(el, "addr:state");
  cJSON_AddItemToObject(p, "prefecture",
                        st ? cJSON_CreateString(st) : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "hazard_map_live");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"natural\"=\"volcano\"](area.jp);"
    "way[\"natural\"=\"volcano\"](area.jp);"
    "node[\"hazard\"=\"landslide\"](area.jp);"
    "way[\"hazard\"=\"landslide\"](area.jp);",
    /* see marine_traffic.c: 60s per endpoint could never finish a nationwide
     * area.jp query on the one reachable Overpass mirror. */
    180, 150000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def hazard_map_portal_def = {
  .id = "hazard-map-portal", .collector = "safety",
  .name = "MLIT Hazard Map Portal",
  .name_ja = "国土交通省 ハザードマップポータル",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(hazard_map_portal_def)
