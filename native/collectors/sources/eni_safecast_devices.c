/* Safecast realtime radiation device network.
 * Endpoint: https://tt.safecast.org/devices                            (keyless)
 * (NOT api.safecast.org/measurements.json, whose newest record was stale.)
 * Emits one row per device report carrying a tube count: cpm (UNIT: counts per
 * minute) tagged with the tube field it came from, capture time, coordinates,
 * and the whole report as fetched under `report`.
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
#include "lib/keyqual.h"
#include "_timefmt.inc"
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

/* The entry is a usable reading: a urn, a valid recent capture date, a real
 * coordinate and at least one tube count. Both passes ask the same question,
 * so the key census in pass 1 covers exactly the rows pass 2 emits. */
static int usable(const cJSON *d, int min_year, double *lat, double *lon,
                  const char **tube, double *cpm) {
  const char *urn = jo_sv(d, "device_urn");
  int y = captured_year(jo_sv(d, "when_captured"));
  if (!urn || y < min_year || y < 2015) return 0;      /* stale/invalid date */
  cJSON *la = cJSON_GetObjectItem(d, "loc_lat");
  cJSON *lo = cJSON_GetObjectItem(d, "loc_lon");
  if (!cJSON_IsNumber(la) || !cJSON_IsNumber(lo)) return 0;   /* no geo (R2) */
  *lat = la->valuedouble; *lon = lo->valuedouble;
  if ((*lat == 0 && *lon == 0) || *lat < -90 || *lat > 90 || *lon < -180 || *lon > 180)
    return 0;
  /* first tube field that carries a count */
  *tube = NULL; *cpm = 0;
  for (int i = 0; TUBES[i]; i++) {
    cJSON *v = cJSON_GetObjectItem(d, TUBES[i]);
    if (cJSON_IsNumber(v)) { *tube = TUBES[i]; *cpm = v->valuedouble; break; }
  }
  return *tube != NULL;                    /* no measurement -> no row (R1) */
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, "https://tt.safecast.org/devices", 45000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  if (!cJSON_IsArray(doc)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] unexpected shape\n"); return -1; }

  /* The staleness threshold IS the current year: without it the
   * "2012-00-00T00:00:00Z" sentinel rows this collector exists to reject
   * would every one of them pass the filter. */
  struct tm tmv;
  if (!jo_tm_utc(time(NULL), &tmv)) {
    cJSON_Delete(doc);
    fprintf(stderr, "[" SRC "] cannot render the current date\n");
    return -1;
  }
  int min_year = tmv.tm_year + 1900 - 1;      /* >= current-1, per parse notes */

  int n = 0, folded = 0;
  /* IDENTITY. /devices lists one entry per device REPORT, and a busy device
   * appears several times with different when_captured readings (live
   * 2026-10-06: 2,085 entries, 11 urns listed more than once, one of them 7
   * times), in no chronological order. Pass 1 counts every urn among the
   * usable readings; pass 2 qualifies EVERY reading of a repeated urn by its
   * capture time ("note:dev:…|2026-10-06T03:23:59Z"), falling back to a hash
   * of the entry. It used to be first-come-plain: the plain urn went to
   * whichever report the list put first — a different one on most 15-minute
   * polls — so the row under it was overwritten by another reading while the
   * reading it held re-appeared under a qualified uid. See lib/keyqual.h. */
  keyqual kq = {0};
  cJSON *d;
  cJSON_ArrayForEach(d, doc) {
    double lat, lon, cpm; const char *tube;
    if (usable(d, min_year, &lat, &lon, &tube, &cpm))
      keyqual_add(&kq, jo_sv(d, "device_urn"), jo_sv(d, "when_captured"));
  }
  keyqual_seal(&kq);

  cJSON_ArrayForEach(d, doc) {
    double lat, lon, cpm; const char *tube;
    if (!usable(d, min_year, &lat, &lon, &tube, &cpm)) continue;
    const char *urn = jo_sv(d, "device_urn");
    const char *when = jo_sv(d, "when_captured");

    char keybuf[512];
    const char *rk = urn;
    if (keyqual_count(&kq, urn) > 1) {
      char *raw = cJSON_PrintUnformatted(d);
      rk = keyqual_uid(&kq, urn, when, raw ? raw : "", keybuf, sizeof keybuf);
      free(raw);
      if (!keyqual_claim(&kq, urn, rk)) { folded++; continue; }
    }

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
    /* The whole report as fetched: every tube (not just the first that
     * carries a count), the particulate counters, temperature, humidity,
     * pressure, battery, location name and device metadata. */
    cJSON_AddItemToObject(p, "report", cJSON_Duplicate(d, 1));
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[224];
    snprintf(title, sizeof title, "Safecast %s: %.0f CPM (%s)", urn, cpm, tube);

    intel_item row = {0};
    row.remote_key      = rk;
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
  keyqual_free(&kq);
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d (%d byte-identical repeats folded)\n",
          n, folded);
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
