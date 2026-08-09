/* collectors/infrastructure/sources/gas_network.c — port of
 * server/src/collectors/gasNetwork.js (fetchOverpass single area.jp).
 * GAS_FACILITIES offline fallback intentionally not ported (rule 8).
 * `updated_at` mirrors JS `new Date().toISOString()` (UTC, ms precision). */
#include "../../lib/geojson.h"
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  char fb[32];
  snprintf(fb, sizeof fb, "GAS_LIVE_%04d", i + 1);
  cJSON_AddStringToObject(p, "facility_id", fb);

  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char nb[40];
    snprintf(nb, sizeof nb, "Gas facility %lld", oid);
    cJSON_AddStringToObject(p, "name", nb);
  }

  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "unknown");

  const char *mm = ov_tag(el, "man_made");
  const char *ind = ov_tag(el, "industrial");
  cJSON_AddStringToObject(p, "facility_type", mm ? mm : (ind ? ind : "gas"));

  cJSON_AddStringToObject(p, "country", "JP");
  char ts[40];
  jo_iso_now(ts, sizeof ts);
  cJSON_AddStringToObject(p, "updated_at", ts);
  cJSON_AddStringToObject(p, "source", "gas_network");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"man_made\"=\"storage_tank\"][\"content\"=\"gas\"](area.jp);"
    "way[\"man_made\"=\"gasometer\"](area.jp);"
    "node[\"industrial\"=\"gas\"](area.jp);"
    "way[\"industrial\"=\"gas\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def gas_network_def = {
  .id = "gas-network", .collector = "infrastructure",
  .name = "Gas Network", .name_ja = "ガス網",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(gas_network_def)
