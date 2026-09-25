/* CDC PLACES local health measures (Socrata row tier).
 * Endpoint: https://chronicdata.cdc.gov/resource/eav7-hnsx.json?$limit=100
 * New host: the existing Socrata source covers data.cdc.gov, not
 * chronicdata.cdc.gov. Bare JSON array; every numeric arrives as a STRING and
 * is emitted exactly as returned. Emits, per estimate: year, stateabbr,
 * statedesc, locationname, measure, data_value, data_value_unit,
 * data_value_type, low/high_confidence_limit, totalpopulation, locationid.
 * R2: geolocation is a GeoJSON Point when the publisher supplies one and is
 * the only source of coordinates; rows without it get no location. The table
 * is very large, so the request is bounded with $limit (narrow further with
 * $where=stateabbr=... when pivoting). Keyless.
 * Licence: US federal government work - public domain. */
#include "od_shared.inc"

#define SID "socrata-cdc-places"
/* `$offset=0` seeds lib/pagewalk.c's offset walk — pw_walk only ever advances a
 * parameter the URL already carries, and never invents one. `$order=:id` makes
 * the sequence total, so an offset window has a defined row order to step
 * through. Measured 2026-09-19: 2,150,438 rows upstream, of which the
 * single-page collector kept 100. */
static const char *URL =
  "https://chronicdata.cdc.gov/resource/eav7-hnsx.json"
  "?$limit=100&$offset=0&$order=:id";
static const char *const TITLE_KEYS[] = { "measure", "locationname", NULL };
static const char *const SUM_KEYS[] = { "locationname", "statedesc", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "health-prevalence-estimate";
  sp.tags_json = "[\"opendata\",\"health\",\"us\"]";
  sp.link = URL;
  sp.title_keys = TITLE_KEYS;
  sp.summary_keys = SUM_KEYS;
  /* NOT `locationid` alone. PLACES publishes one row per location × measure ×
   * year × value-type, so a 2,000-record walk carried just 32 distinct
   * locationids and 1,968 records collapsed onto a uid already written
   * (measured 2026-09-20, after paging was fixed — the collapse only became
   * visible once the row stopped reading a single page). */
  sp.id_key = "locationid";
  sp.id_key2 = "measure";
  sp.id_key3 = "year";
  sp.id_key4 = "data_value_type";
  return od_wrc(SID, od_walk_rows(ctx, sink, SID, URL, NULL, &sp));
}

static const source_def od_socrata_cdc_places_def = {
  .id = SID, .collector = "statistics",
  .name = "CDC PLACES local health measures (Socrata)",
  .update_interval_sec = 604800, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://chronicdata.cdc.gov/resource/eav7-hnsx.json?$limit=100&$order=:id",
  .description = "CDC PLACES small-area health estimates: place/county prevalence with confidence intervals and population",
  .license = "US federal government work - public domain",
  .free_tier = 1,
};
REGISTER_SOURCE(od_socrata_cdc_places_def)
