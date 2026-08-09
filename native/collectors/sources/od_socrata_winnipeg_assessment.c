/* City of Winnipeg assessment parcels (Socrata row tier).
 * Endpoint: https://data.winnipeg.ca/resource/d4mq-wa44.json?$limit=100
 * Bare JSON array; the table is large, so the request is always bounded with
 * $limit (paging is via $offset). Emits, per parcel, every scalar the API
 * returned: roll_number, full_address, street_number, street_name,
 * neighbourhood_area, market_region, total_living_area, property_use_code,
 * assessed_value. No coordinates are published on this dataset, so has_geo is
 * never set (R2). Keyless.
 * Licence: Open Government Licence - City of Winnipeg. */
#include "od_shared.c"

#define SID "socrata-winnipeg-assessment"
static const char *URL = "https://data.winnipeg.ca/resource/d4mq-wa44.json?$limit=100";
static const char *const TITLE_KEYS[] = { "full_address", "roll_number", NULL };
static const char *const SUM_KEYS[] = { "neighbourhood_area", "market_region",
                                        NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "property-assessment";
  sp.tags_json = "[\"opendata\",\"property\",\"ca\"]";
  sp.link = URL;
  sp.title_keys = TITLE_KEYS;
  sp.summary_keys = SUM_KEYS;
  sp.id_key = "roll_number";
  sp.title_prefix = "Winnipeg parcel:";
  return od_rc(SID, od_fetch_rows(ctx, sink, URL, NULL, &sp));
}

static const source_def od_socrata_winnipeg_assessment_def = {
  .id = SID, .collector = "government",
  .name = "City of Winnipeg assessment parcels (Socrata)",
  .update_interval_sec = 604800, .run = run,
  .category = "government", .type = "api",
  .url = "https://data.winnipeg.ca/resource/d4mq-wa44.json?$limit=100",
  .description = "Winnipeg property assessment roll: address, neighbourhood, property use code and assessed value",
  .license = "Open Government Licence - City of Winnipeg",
  .free_tier = 1,
};
REGISTER_SOURCE(od_socrata_winnipeg_assessment_def)
