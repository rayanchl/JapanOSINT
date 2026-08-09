/* collectors/infrastructure/sources/water_infra.c
 * Port of server/src/collectors/waterInfra.js — PRIMARY tryLive() Overpass
 * path only. The synthetic MLIT real-time dam levels and JWWA facility
 * arrays (always concatenated in JS) are curated seed data and are
 * intentionally not ported (correctness-neutral). */
#include "../../lib/geojson.h"
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  char fid[24];
  snprintf(fid, sizeof fid, "WTR_LIVE_%04d", i + 1);
  cJSON_AddStringToObject(p, "facility_id", fid);

  cJSON *idv = cJSON_GetObjectItem(el, "id");
  long long oid = idv && cJSON_IsNumber(idv) ? (long long)idv->valuedouble : 0;
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  if (name) {
    cJSON_AddStringToObject(p, "name", name);
  } else {
    char nm[48];
    snprintf(nm, sizeof nm, "Water facility %lld", oid);
    cJSON_AddStringToObject(p, "name", nm);
  }

  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "unknown");

  const char *mm = ov_tag(el, "man_made");
  if (!mm) mm = ov_tag(el, "landuse");
  cJSON_AddStringToObject(p, "facility_type", mm ? mm : "water");

  cJSON_AddStringToObject(p, "country", "JP");
  char now[40];
  jo_iso_now(now, sizeof now);
  cJSON_AddStringToObject(p, "updated_at", now);
  cJSON_AddStringToObject(p, "source", "water_infra");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"man_made\"=\"water_works\"](area.jp);"
    "way[\"man_made\"=\"water_works\"](area.jp);"
    "node[\"man_made\"=\"water_tower\"](area.jp);"
    "way[\"landuse\"=\"reservoir\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def water_infra_def = {
  .id = "water-infra", .collector = "infrastructure",
  .name = "Water Infrastructure", .name_ja = "水道インフラ",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(water_infra_def)
