/* collectors/infrastructure/sources/airport_infra.c — port of
 * server/src/collectors/airportInfra.js. Primary live path is
 * tryOSMAirportInfra() (fetchOverpass, single area.jp query). The curated
 * AIRPORT_FACILITIES seed / _meta envelope is intentionally not ported
 * (JS does `if (!live) features = []` and the seed-merge loop is empty). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char fid[64];
  snprintf(fid, sizeof fid, "OSM_AIR_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "facility_id", fid);
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Airport facility %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
  const char *icao = ov_tag(el, "icao");
  if (icao) cJSON_AddStringToObject(p, "icao", icao);
  else cJSON_AddItemToObject(p, "icao", cJSON_CreateNull());
  const char *iata = ov_tag(el, "iata");
  if (iata) cJSON_AddStringToObject(p, "iata", iata);
  else cJSON_AddItemToObject(p, "iata", cJSON_CreateNull());
  const char *mm = ov_tag(el, "man_made");
  const char *aw = ov_tag(el, "aeroway");
  const char *ftype = (mm && strcmp(mm, "tower") == 0) ? "control_tower"
                    : (aw && strcmp(aw, "navigationaid") == 0) ? "navaid"
                    : "aerodrome";
  cJSON_AddStringToObject(p, "facility_type", ftype);
  if (aw) cJSON_AddStringToObject(p, "aeroway", aw);
  else cJSON_AddItemToObject(p, "aeroway", cJSON_CreateNull());
  const char *ele = ov_tag(el, "ele");
  if (ele) cJSON_AddNumberToObject(p, "elevation_ft", strtod(ele, NULL));
  else cJSON_AddItemToObject(p, "elevation_ft", cJSON_CreateNull());
  const char *rl = ov_tag(el, "runway:length");
  if (rl) cJSON_AddNumberToObject(p, "runway_length_m", strtod(rl, NULL));
  else cJSON_AddItemToObject(p, "runway_length_m", cJSON_CreateNull());
  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "unknown");
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"aeroway\"=\"aerodrome\"](area.jp);"
    "way[\"aeroway\"=\"aerodrome\"](area.jp);"
    "node[\"aeroway\"=\"navigationaid\"](area.jp);"
    "node[\"man_made\"=\"tower\"][\"tower:type\"=\"aircraft_control\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def airport_infra_def = {
  .id = "airport-infra", .collector = "infrastructure",
  .name = "Airport Infrastructure", .name_ja = "空港インフラ",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(airport_infra_def)
