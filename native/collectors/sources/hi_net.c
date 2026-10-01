/* collectors/environment/sources/hi_net.c — port of
 * server/src/collectors/hiNet.js (fetchOverpass single area.jp).
 * SEED_HINET offline fallback intentionally not ported (rule 8).
 * `updated_at` mirrors JS `new Date().toISOString()` (UTC, ms). */
#include "lib/geojson.h"
#include "lib/jocore.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  char sb[32];
  snprintf(sb, sizeof sb, "HINET_LIVE_%04d", i + 1);
  cJSON_AddStringToObject(p, "station_id", sb);

  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;

  const char *ref = ov_tag(el, "ref");
  const char *nm = ov_tag(el, "name");
  if (ref) {
    cJSON_AddStringToObject(p, "code", ref);
  } else if (nm) {
    cJSON_AddStringToObject(p, "code", nm);
  } else {
    char cb[32];
    snprintf(cb, sizeof cb, "H%lld", oid);
    cJSON_AddStringToObject(p, "code", cb);
  }

  const char *en = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else if (en) {
    cJSON_AddStringToObject(p, "name", en);
  } else {
    char nb[32];
    snprintf(nb, sizeof nb, "Hi-net %lld", oid);
    cJSON_AddStringToObject(p, "name", nb);
  }

  cJSON_AddItemToObject(p, "depth_m", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "network", "Hi-net");

  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "NIED");

  const char *st = ov_tag(el, "addr:state");
  cJSON_AddItemToObject(p, "prefecture",
                        st ? cJSON_CreateString(st) : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "country", "JP");
  char ts[40];
  jo_iso_now(ts, sizeof ts);
  cJSON_AddStringToObject(p, "updated_at", ts);
  cJSON_AddStringToObject(p, "source", "hinet_live");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"man_made\"=\"monitoring_station\"]"
    "[\"monitoring:seismic_activity\"=\"yes\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def hi_net_def = {
  .id = "hi-net", .collector = "environment",
  .name = "NIED Hi-net Stations", .name_ja = "NIED Hi-net 観測点",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(hi_net_def)
