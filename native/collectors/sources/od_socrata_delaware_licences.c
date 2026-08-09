/* Delaware active business licences (Socrata row tier).
 * Endpoint: https://data.delaware.gov/resource/5zy2-grhr.json?$limit=100
 * Bare JSON array. Emits, per licence, every scalar the API returned:
 * business_name, trade_name, category, current_license_valid_from,
 * current_license_valid_to, address, city, state, zip. A genuine corporate
 * pivot for a jurisdiction whose corporate registry is search-walled.
 * The endpoint also supports $q= and $where= for name pivots.
 * No coordinates are published, so has_geo is never set (R2). Keyless.
 * Licence: State of Delaware open data, public record. */
#include "od_shared.c"

#define SID "socrata-delaware-business-licences"
static const char *URL = "https://data.delaware.gov/resource/5zy2-grhr.json?$limit=100";
static const char *const TITLE_KEYS[] = { "business_name", "trade_name", NULL };
static const char *const SUM_KEYS[] = { "category", "city", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "business-licence";
  sp.tags_json = "[\"opendata\",\"company\",\"us\"]";
  sp.link = URL;
  sp.title_keys = TITLE_KEYS;
  sp.summary_keys = SUM_KEYS;
  sp.id_key = "business_name";
  sp.date_key = "current_license_valid_from";
  return od_rc(SID, od_fetch_rows(ctx, sink, URL, NULL, &sp));
}

static const source_def od_socrata_delaware_licences_def = {
  .id = SID, .collector = "government",
  .name = "Delaware active business licences (Socrata)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "api",
  .url = "https://data.delaware.gov/resource/5zy2-grhr.json?$limit=100",
  .description = "Delaware state business licence register: legal and trade names, licence category, validity window and address",
  .license = "State of Delaware open data, public record",
  .free_tier = 1,
};
REGISTER_SOURCE(od_socrata_delaware_licences_def)
