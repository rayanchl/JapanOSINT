/* collectors/environment/sources/k_net.c — port of
 * server/src/collectors/kNet.js (fetchOverpass single area.jp).
 * SEED_KNET offline fallback intentionally not ported (rule 8).
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
  char sb[32];
  snprintf(sb, sizeof sb, "KNET_LIVE_%04d", i + 1);
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
    snprintf(cb, sizeof cb, "K%lld", oid);
    cJSON_AddStringToObject(p, "code", cb);
  }

  const char *en = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else if (en) {
    cJSON_AddStringToObject(p, "name", en);
  } else {
    char nb[32];
    snprintf(nb, sizeof nb, "K-NET %lld", oid);
    cJSON_AddStringToObject(p, "name", nb);
  }

  cJSON_AddStringToObject(p, "network", "K-NET");

  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "NIED");

  const char *st = ov_tag(el, "addr:state");
  cJSON_AddItemToObject(p, "prefecture",
                        st ? cJSON_CreateString(st) : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "country", "JP");
  char ts[40];
  iso_now(ts, sizeof ts);
  cJSON_AddStringToObject(p, "updated_at", ts);
  cJSON_AddStringToObject(p, "source", "knet_live");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"man_made\"=\"monitoring_station\"]"
    "[\"monitoring:strong_motion\"=\"yes\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def k_net_def = {
  .id = "k-net", .collector = "environment",
  .name = "NIED K-NET Stations", .name_ja = "NIED K-NET 観測点",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(k_net_def)
