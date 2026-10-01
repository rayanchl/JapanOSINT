/* US DOT border crossing / entry data (Socrata row tier).
 * Endpoint: https://data.transportation.gov/resource/keg4-3bc2.json
 *           ?$limit=100&$order=date%20DESC
 * Bare JSON array; value/latitude/longitude arrive as STRINGS and the
 * ":@computed_region_*" columns are ignored. Emits, per row: port_name,
 * state, port_code, border, date, measure (conveyance type) and value.
 * R2: `point` is a proper upstream GeoJSON Point and is the coordinate
 * source; nothing is derived when it is absent. Keyless.
 * Licence: US federal government work - public domain. */
#include "od_shared.inc"

#define SID "socrata-usdot-border-crossing"
/* `$offset=0` seeds lib/pagewalk.c's offset walk — pw_walk only ever advances a
 * parameter the URL already carries. `,:id` makes the sort TOTAL: this table is
 * one row per (port, month, measure), so a bare `date DESC` ties thousands of
 * rows together and an offset window across that tie is undefined. Measured
 * 2026-09-19: 275,901 rows upstream, of which the single-page collector kept
 * 100. */
static const char *URL =
  "https://data.transportation.gov/resource/keg4-3bc2.json"
  "?$limit=100&$offset=0&$order=date%20DESC,:id&$select=*,:id";
static const char *const TITLE_KEYS[] = { "port_name", "port_code", NULL };
static const char *const SUM_KEYS[] = { "measure", "border", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "border-crossing-count";
  sp.tags_json = "[\"opendata\",\"border\",\"us\"]";
  sp.link = URL;
  sp.title_keys = TITLE_KEYS;
  sp.summary_keys = SUM_KEYS;
  sp.id_key = ":id";   /* rule 4b, measured: port_code recurs per measure/date (100 emitted, 84 stored); :id is Socrata's per-row identity, via $select=*,:id */
  sp.date_key = "date";
  sp.title_prefix = "Border crossings:";
  return od_wrc(SID, od_walk_rows(ctx, sink, SID, URL, NULL, &sp));
}

static const source_def od_socrata_usdot_border_def = {
  .id = SID, .collector = "statistics",
  .name = "US DOT border crossing / entry data (Socrata)",
  .update_interval_sec = 86400, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://data.transportation.gov/resource/keg4-3bc2.json?$limit=100&$order=date%20DESC",
  .description = "Monthly inbound crossing counts at US land ports of entry by conveyance type, with upstream port coordinates",
  .license = "US federal government work - public domain",
  .free_tier = 1,
};
REGISTER_SOURCE(od_socrata_usdot_border_def)
