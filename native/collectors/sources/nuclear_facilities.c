/* collectors/infrastructure/sources/nuclear_facilities.c — port of
 * server/src/collectors/nuclearFacilities.js (fetchOverpass single
 * area.jp). NUCLEAR_FACILITIES offline fallback not ported (rule 8).
 * `updated_at` mirrors JS `new Date().toISOString()` (UTC, ms). */
#include "lib/geojson.h"
#include "lib/jocore.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;

  /* STABLE uid — "facility_id" is not a NATIVE_ID_KEY (lib/geojson.c:9-12), so
   * feature_uid() hashed {geometry,properties}, and `updated_at` below moves
   * every run. Measured live: 21 facilities became 42 on a second run.
   * `station_id` is a NATIVE_ID_KEY; the OSM id is stable, so re-runs now
   * update in place. Index-derived facility_id kept as display payload. */
  char sid_buf[40];
  snprintf(sid_buf, sizeof sid_buf, "OSM_%lld", oid);
  cJSON_AddStringToObject(p, "station_id", sid_buf);

  char fb[32];
  snprintf(fb, sizeof fb, "NUC_LIVE_%04d", i + 1);
  cJSON_AddStringToObject(p, "facility_id", fb);
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
  jo_iso_now(ts, sizeof ts);
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
