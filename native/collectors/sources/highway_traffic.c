/* collectors/transport/sources/highway_traffic.c — port of
 * server/src/collectors/highwayTraffic.js (fetchOverpass single area.jp).
 * HIGHWAY_NODES offline fallback intentionally not ported (rule 8).
 * `updated_at` mirrors JS `new Date().toISOString()` (UTC, ms). */
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
  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;

  /* STABLE uid. "node_id" is not one of lib/geojson.c's NATIVE_ID_KEYS, so
   * feature_uid() fell through to sha1({geometry,properties}) — and the
   * properties carry `updated_at`, regenerated every run. On a 600s interval
   * that re-inserted every motorway junction in Japan as a NEW row every ten
   * minutes: a live double-run measured 7,298 rows becoming 14,641.
   * `station_id` IS a NATIVE_ID_KEY, and the OSM element id is stable across
   * runs, so the uid is now "highway-traffic|OSM_<id>" and re-runs update in
   * place. `updated_at` stays as payload; it no longer feeds the uid. */
  char sid_buf[40];
  snprintf(sid_buf, sizeof sid_buf, "OSM_%lld", oid);
  cJSON_AddStringToObject(p, "station_id", sid_buf);

  char nb[32];
  snprintf(nb, sizeof nb, "HWY_LIVE_%04d", i + 1);
  cJSON_AddStringToObject(p, "node_id", nb);
  const char *nm = ov_tag(el, "name");
  const char *ref = ov_tag(el, "ref");
  if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else if (ref) {
    cJSON_AddStringToObject(p, "name", ref);
  } else {
    char jb[32];
    snprintf(jb, sizeof jb, "Junction %lld", oid);
    cJSON_AddStringToObject(p, "name", jb);
  }

  const char *hw = ov_tag(el, "highway");
  cJSON_AddStringToObject(p, "highway", hw ? hw : "motorway_junction");

  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "unknown");
  cJSON_AddStringToObject(p, "node_type", "JCT");

  cJSON_AddItemToObject(p, "ref",
                        ref ? cJSON_CreateString(ref) : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "country", "JP");
  char ts[40];
  jo_iso_now(ts, sizeof ts);
  cJSON_AddStringToObject(p, "updated_at", ts);
  cJSON_AddStringToObject(p, "source", "highway_traffic");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"highway\"=\"motorway_junction\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def highway_traffic_def = {
  .id = "highway-traffic", .collector = "transport",
  .name = "Expressway IC/JCT/SA/PA", .name_ja = "高速道路 IC/JCT/SA/PA",
   .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(highway_traffic_def)
