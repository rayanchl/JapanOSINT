/* collectors/transport/sources/highway_traffic.c — port of
 * server/src/collectors/highwayTraffic.js (fetchOverpass single area.jp).
 * HIGHWAY_NODES offline fallback intentionally not ported (rule 8).
 * `updated_at` mirrors JS `new Date().toISOString()` (UTC, ms). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

static void iso_now(char *o, size_t n) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  struct tm tm;
  gmtime_r(&tv.tv_sec, &tm);
  char base[32];
  strftime(base, sizeof base, "%Y-%m-%dT%H:%M:%S", &tm);
  snprintf(o, n, "%s.%03dZ", base, (int)(tv.tv_usec / 1000));
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();
  char nb[32];
  snprintf(nb, sizeof nb, "HWY_LIVE_%04d", i + 1);
  cJSON_AddStringToObject(p, "node_id", nb);

  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
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
  iso_now(ts, sizeof ts);
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
