/* Verified-live fi_transport sources (3), part 1.
 * Every endpoint in this file returned 2xx and parsed to at
 * least one record at generation time; see
 * docs/verified-sources-manifest.tsv for the recorded proof.
 * Scaffolded once by collectors/gen_verified_sources.py; HAND-MAINTAINED
 * since — this file, not the manifest, is the current copy. The
 * generator refuses to overwrite it without --force. */
#include "_verified_macros.inc"

VJSON(fi_digitraffic_rail_operators, "fi-digitraffic-rail-operators", "Digitraffic Finnish rail operator registry", "Digitraffic Finnish rail operator registry",
  "fi_transport", "transport",
  "https://rata.digitraffic.fi/api/v1/metadata/operators",
  "",
  "fi", "[\"fi\",\"transport\",\"batch16\",\"high-penetrancy\"]", 3600,
  "Every licensed rail operator in Finland: id, operatorName, operatorShortCode, operatorUICCode and the train-number ranges allocated to it by trainCategory (Cargo, Locomotive, Shunting, On-track machines). Maps any observed train number to the company running it.");

/* fi-digitraffic-road-maintenance-tracking: the bare URL answers the API's
 * default window (routes that ENDED in the last 24 h) as ONE FeatureCollection —
 * 114,204 features, 6.4 MB gzip, over the engine's 64 MB decoded-body ceiling.
 * The fetch was refused whole on every run ("response exceeded 67108864 byte
 * ceiling") and nothing was stored. The API has no paging, but it takes
 * endFrom (inclusive) / endBefore (exclusive), so the same 24 h is read as
 * eight contiguous 3 h windows (about 14k features each). Tracking ids are
 * unique across the day (114,204 distinct), so windows cannot collide. A
 * window that fails is disclosed as a truncation notice, not skipped. */
#include "lib/jocore.h"
#define DT_MAINT_ID "fi-digitraffic-road-maintenance-tracking"
#define DT_MAINT_URL "https://tie.digitraffic.fi/api/maintenance/v1/tracking/routes?domain=state-roads"
#define DT_MAINT_WIN_S 10800
#define DT_MAINT_WINDOWS 8
static int run_fi_digitraffic_road_maintenance_tracking(const source_ctx *c, intel_sink *s) {
  vgeo_cat_ctx vc = { s, "transport", DT_MAINT_ID };
  intel_sink vs = { &vc, vgeo_cat_emit };
  time_t end = time(NULL);
  int ok = 0, failed = 0;
  long total = 0;
  for (int w = 0; w < DT_MAINT_WINDOWS; w++) {
    time_t from = end - (time_t)(DT_MAINT_WINDOWS - w) * DT_MAINT_WIN_S;
    time_t to = from + DT_MAINT_WIN_S;
    struct tm t1, t2;
    char a[32], b[32], url[256];
    if (!gmtime_r(&from, &t1) || !gmtime_r(&to, &t2)) { failed++; continue; }
    strftime(a, sizeof a, "%Y-%m-%dT%H:%M:%SZ", &t1);
    strftime(b, sizeof b, "%Y-%m-%dT%H:%M:%SZ", &t2);
    snprintf(url, sizeof url, "%s&endFrom=%s&endBefore=%s", DT_MAINT_URL, a, b);
    int n = geojson_emit_paged(&vs, DT_MAINT_ID, c->http, url, 90000);
    if (n < 0) {
      fprintf(stderr, "[%s] window %s..%s fetch failed\n", DT_MAINT_ID, a, b);
      failed++;
      continue;
    }
    ok++;
    total += n;
  }
  if (failed)
    jo_truncation_notice_ex(s, DT_MAINT_ID, DT_MAINT_URL, total, -1,
                            "one or more 3-hour endFrom/endBefore windows of the 24-hour day failed to fetch",
                            "re-run; each window is an independent request", NULL);
  fprintf(stderr, "[%s] emitted %ld across %d of %d windows\n", DT_MAINT_ID,
          total, ok, DT_MAINT_WINDOWS);
  if (!ok) return -1;
  return 0;
}
static const source_def fi_digitraffic_road_maintenance_tracking = {
  .id = DT_MAINT_ID, .collector = "fi_transport",
  .name = "Digitraffic road maintenance vehicle tracking",
  .name_ja = "Digitraffic road maintenance vehicle tracking",
  .update_interval_sec = 3600, .run = run_fi_digitraffic_road_maintenance_tracking,
  .category = "transport", .type = "api", .url = DT_MAINT_URL,
  .description = "GeoJSON LineString traces of actual maintenance vehicle movements: tracking id and previousId (chainable), sendingTime, startTime/endTime, created, tasks[] performed (MECHANICAL_CUT, SALTING, PLOUGHING...), direction, domain and source agency. Read as eight 3-hour windows covering the last 24 hours.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(fi_digitraffic_road_maintenance_tracking);

VJSON(fi_digitraffic_road_tms_station_data, "fi-digitraffic-road-tms-station-data", "Digitraffic Finnish TMS station live sensor data", "Digitraffic Finnish TMS station live sensor data",
  "fi_transport", "transport",
  "https://tie.digitraffic.fi/api/tms/v1/stations/23001/data",
  "sensorvalues",
  "fi", "[\"fi\",\"transport\",\"batch16\",\"high-penetrancy\"]", 3600,
  "Per-sensor readings from one traffic measurement station: sensor id, name (e.g. OHITUKSET_60MIN_KIINTEA_SUUNTA1 = vehicles/hour by direction), shortName, unit, value, measuredTime and the aggregation window. The detail hop under the station list already in the tree.");
