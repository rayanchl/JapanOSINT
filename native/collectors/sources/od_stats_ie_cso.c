/* CSO Ireland PxStat cube read (JSON-stat 2.0).
 * Endpoint: https://ws.cso.ie/public/api.restful/
 *           PxStat.Data.Cube_API.ReadDataset/CPM01/JSON-stat/2.0/en
 * JSON-stat 2.0 at the root (class=dataset); the dimension order is in the
 * "id" array and each observation is decoded against it by the reader in
 * od_shared.c. value[] is SPARSE: nulls are common and are skipped rather
 * than emitted as 0. The matrix code (CPM01 = Consumer Price Index) is
 * swappable across the whole CSO catalogue. Keyless.
 * Licence: CSO data is CC-BY 4.0. */
#include "od_shared.c"

#define SID "stats-ie-cso-cube"
static const char *URL =
  "https://ws.cso.ie/public/api.restful/PxStat.Data.Cube_API.ReadDataset/"
  "CPM01/JSON-stat/2.0/en";

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_jsonstat_collect(ctx, sink, URL, "statistic-observation",
                                        "[\"statistics\",\"ie\"]", URL, 300));
}

static const source_def od_stats_ie_cso_def = {
  .id = SID, .collector = "statistics",
  .name = "CSO Ireland PxStat cube read (JSON-stat 2.0)",
  .update_interval_sec = 86400, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://ws.cso.ie/public/api.restful/PxStat.Data.Cube_API.ReadDataset/CPM01/JSON-stat/2.0/en",
  .description = "Irish Consumer Price Index observations from the CSO PxStat cube API, one row per statistic/period with the decoded dimension labels",
  .license = "CC-BY 4.0 (CSO Ireland)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_stats_ie_cso_def)
