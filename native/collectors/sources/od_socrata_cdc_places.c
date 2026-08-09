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
#include "od_shared.c"

#define SID "socrata-cdc-places"
static const char *URL = "https://chronicdata.cdc.gov/resource/eav7-hnsx.json?$limit=100";
static const char *const TITLE_KEYS[] = { "measure", "locationname", NULL };
static const char *const SUM_KEYS[] = { "locationname", "statedesc", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "health-prevalence-estimate";
  sp.tags_json = "[\"opendata\",\"health\",\"us\"]";
  sp.link = URL;
  sp.title_keys = TITLE_KEYS;
  sp.summary_keys = SUM_KEYS;
  sp.id_key = "locationid";
  return od_rc(SID, od_fetch_rows(ctx, sink, URL, NULL, &sp));
}

static const source_def od_socrata_cdc_places_def = {
  .id = SID, .collector = "statistics",
  .name = "CDC PLACES local health measures (Socrata)",
  .update_interval_sec = 604800, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://chronicdata.cdc.gov/resource/eav7-hnsx.json?$limit=100",
  .description = "CDC PLACES small-area health estimates: place/county prevalence with confidence intervals and population",
  .license = "US federal government work - public domain",
  .free_tier = 1,
};
REGISTER_SOURCE(od_socrata_cdc_places_def)
