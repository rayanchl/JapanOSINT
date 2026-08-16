/* Safecast realtime radiation device network.
 * Endpoint: https://tt.safecast.org/devices                            (keyless)
 * (NOT api.safecast.org/measurements.json, whose newest record was stale.)
 * Emits one row per device reporting a tube count: cpm (UNIT: counts per
 * minute) tagged with the tube field it came from, capture time, coordinates.
 *
 * UNIT TRAP: lnd_7318u / lnd_7128ec / lnd_712u / lnd_7318c are CPM (counts per
 * minute), NOT uSv/h. They are emitted as cpm and the tube name is kept; no
 * conversion factor is invented.
 * STALE-DATE TRAP: many entries carry when_captured "2012-00-00T00:00:00Z", an
 * invalid date. A parseable year >= 2015 and within a year of the newest
 * capture seen is required before a row is emitted.
 * GEO (R2): loc_lat/loc_lon from the device itself; missing -> no row (a
 * radiation reading with no location is not useful and must not be invented).
 * Licence: Safecast data is CC0. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SRC "safecast-realtime-devices"

static const char *TUBES[] = { "lnd_7318u", "lnd_7318c", "lnd_7128ec",
                               "lnd_712u", "lnd_78017w", NULL };

/* "2026-08-01T21:02:47Z" -> 2026; the sentinel "2012-00-00T00:00:00Z" has a
 * month of 00 and is rejected here as unparseable. */
static int captured_year(const char *s) {
  if (!s || strlen(s) < 10) return 0;
  int y = 0, mo = 0, d = 0;
  if (sscanf(s, "%4d-%2d-%2d", &y, &mo, &d) != 3) return 0;
  if (mo < 1 || mo > 12 || d < 1 || d > 31) return 0;
  return y;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, "https://tt.safecast.org/devices", 45000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  if (!cJSON_IsArray(doc)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] unexpected shape\n"); return -1; }

  time_t now = time(NULL);
  struct tm tmv; gmtime_r(&now, &tmv);
  int min_year = tmv.tm_year + 1900 - 1;      /* >= current-1, per parse notes */

  int n = 0;
  cJSON *d;
  cJSON_ArrayForEach(d, doc) {
    const char *urn = jo_sv(d, "device_urn");
    const char *when = jo_sv(d, "when_captured");
    int y = captured_year(when);
    if (!urn || y < min_year || y < 2015) continue;      /* stale/invalid date */

    cJSON *la = cJSON_GetObjectItem(d, "loc_lat");
    cJSON *lo = cJSON_GetObjectItem(d, "loc_lon");
    if (!cJSON_IsNumber(la) || !cJSON_IsNumber(lo)) continue;   /* no geo (R2) */
    double lat = la->valuedouble, lon = lo->valuedouble;
    if ((lat == 0 && lon == 0) || lat < -90 || lat > 90 || lon < -180 || lon > 180)
      continue;

    /* first tube field that carries a count */
    const char *tube = NULL; double cpm = 0;
    for (int i = 0; TUBES[i]; i++) {
      cJSON *v = cJSON_GetObjectItem(d, TUBES[i]);
      if (cJSON_IsNumber(v)) { tube = TUBES[i]; cpm = v->valuedouble; break; }
    }
    if (!tube) continue;                    /* no measurement -> no row (R1) */

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "device_urn", urn);
    cJSON_AddStringToObject(p, "tube_field", tube);
    cJSON_AddNumberToObject(p, "cpm", cpm);
    cJSON_AddStringToObject(p, "unit", "cpm (counts per minute)");
    cJSON_AddStringToObject(p, "unit_note",
      "tube counts are CPM, not uSv/h; no conversion applied");
    cJSON_AddStringToObject(p, "when_captured", when);
    const char *dc = jo_sv(d, "device_class");
    if (dc) cJSON_AddStringToObject(p, "device_class", dc);
    const char *up = jo_sv(d, "service_uploaded");
    if (up) cJSON_AddStringToObject(p, "service_uploaded", up);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[224];
    snprintf(title, sizeof title, "Safecast %s: %.0f CPM (%s)", urn, cpm, tube);

    intel_item row = {0};
    row.remote_key      = urn;
    row.title           = title;
    row.summary         = title;
    row.published_at    = when;
    row.lang            = "en";
    row.link            = "https://tt.safecast.org/devices";
    row.record_type     = "radiation-sensor";
    row.has_geo         = 1;
    row.lat = lat; row.lon = lon;
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"radiation\",\"safecast\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_safecast_devices_def = {
  .id = SRC, .collector = "environment",
  .name = "Safecast realtime radiation device network",
  .update_interval_sec = 900, .run = run,
  .category = "environment", .type = "api",
  .url = "https://tt.safecast.org/devices",
  .description = "Live gamma radiation counts (CPM) from the global Safecast bGeigie/Pointcast device network, with device coordinates.",
  .license = "Safecast data is CC0. Keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_safecast_devices_def)
