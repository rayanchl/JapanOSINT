/* Statistics Sweden PxWeb API v2 table data (JSON-stat 2.0).
 * Endpoint: https://api.scb.se/OV0104/v2beta/api/v2/tables/TAB5444/data
 *           ?lang=en&outputFormat=json-stat2
 * PxWeb v1 needs a POST body; v2beta serves the full table over a plain GET,
 * which is why this uses the v2beta path. Emits one row per NON-NULL
 * observation with every dimension decoded to its label (population per month
 * by sex, month and region in TAB5444). Table ids are discoverable at
 * /v2beta/api/v2/tables?lang=en. Keyless.
 * Licence: SCB official statistics are free to reuse with attribution. */
#include "od_shared.c"

#define SID "stats-se-scb-table"
static const char *URL =
  "https://api.scb.se/OV0104/v2beta/api/v2/tables/TAB5444/data"
  "?lang=en&outputFormat=json-stat2";

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_jsonstat_collect(ctx, sink, URL, "statistic-observation",
                                        "[\"statistics\",\"se\"]", URL, 300));
}

static const source_def od_stats_se_scb_def = {
  .id = SID, .collector = "statistics",
  .name = "Statistics Sweden PxWeb API v2 table data",
  .update_interval_sec = 86400, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://api.scb.se/OV0104/v2beta/api/v2/tables/TAB5444/data?lang=en&outputFormat=json-stat2",
  .description = "Swedish monthly population by sex, month and region from the SCB PxWeb v2 GET API, one row per observation",
  .license = "SCB official statistics - free reuse with attribution",
  .free_tier = 1,
};
REGISTER_SOURCE(od_stats_se_scb_def)
