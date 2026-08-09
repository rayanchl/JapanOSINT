/* Statistics Denmark StatBank data API (JSON-stat).
 * Endpoint: https://api.statbank.dk/v1/data/FOLK1A/JSONSTAT
 *           ?Tid=*&valuePresentation=Value
 * Keyless GET returning JSON-stat 1.x under a "dataset" root: value[] is a
 * flat observation array aligned against dimension.Tid.category.index, which
 * is what the reader in od_shared.c joins to recover period -> value.
 * Emits one row per NON-NULL observation: the decoded period (and any other
 * dimension label the table carries) plus the numeric value. Nulls are gaps
 * and are skipped, never emitted as 0. Labels come back in Danish.
 * The table id is swappable (FOLK1A population, PRIS111 CPI, AULK01
 * unemployment). Keyless.
 * Licence: Statistics Denmark data is free to use with source attribution. */
#include "od_shared.c"

#define SID "stats-dk-statbank"
static const char *URL =
  "https://api.statbank.dk/v1/data/FOLK1A/JSONSTAT?Tid=*&valuePresentation=Value";

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_jsonstat_collect(ctx, sink, URL, "statistic-observation",
                                        "[\"statistics\",\"dk\"]", URL, 300));
}

static const source_def od_stats_dk_statbank_def = {
  .id = SID, .collector = "statistics",
  .name = "Statistics Denmark StatBank data API",
  .update_interval_sec = 86400, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://api.statbank.dk/v1/data/FOLK1A/JSONSTAT?Tid=*&valuePresentation=Value",
  .description = "Danish official statistics observations (quarterly population in FOLK1A) as period/value pairs from the keyless StatBank API",
  .license = "Statistics Denmark - free reuse with source attribution",
  .free_tier = 1,
};
REGISTER_SOURCE(od_stats_dk_statbank_def)
