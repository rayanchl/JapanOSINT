/* collectors/infrastructure/sources/nuclear_facilities.c — port of
 * server/src/collectors/nuclearFacilities.js (fetchOverpass single
 * area.jp). NUCLEAR_FACILITIES offline fallback not ported (rule 8).
 * `updated_at` mirrors JS `new Date().toISOString()` (UTC, ms). */
#include "../../source.h"
#include "../../lib/overpass.h"
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
  (void)i; (void)ud;
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
  char fb[32];
  snprintf(fb, sizeof fb, "NUC_LIVE_%04d", i + 1);
  cJSON_AddStringToObject(p, "facility_id", fb);

  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char nb[40];
    snprintf(nb, sizeof nb, "Nuclear facility %lld", oid);
    cJSON_AddStringToObject(p, "name", nb);
  }

  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "unknown");
  cJSON_AddStringToObject(p, "facility_type", "npp");

  const char *stt = ov_tag(el, "plant:status");
  cJSON_AddStringToObject(p, "status", stt ? stt : "unknown");

  cJSON_AddStringToObject(p, "country", "JP");
  char ts[40];
  iso_now(ts, sizeof ts);
  cJSON_AddStringToObject(p, "updated_at", ts);
  cJSON_AddStringToObject(p, "source", "nuclear_facilities");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"power\"=\"plant\"][\"plant:source\"=\"nuclear\"](area.jp);"
    "way[\"power\"=\"plant\"][\"plant:source\"=\"nuclear\"](area.jp);"
    "node[\"industrial\"=\"nuclear\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def nuclear_facilities_def = {
  .id = "nuclear-facilities", .collector = "infrastructure",
  .name = "Nuclear Facilities", .name_ja = "原子力施設",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(nuclear_facilities_def)
