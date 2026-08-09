/* Statistics Netherlands (CBS) OData v3 - consumer price index table 83131NED.
 * Endpoint: https://opendata.cbs.nl/ODataApi/odata/83131NED/TypedDataSet?$top=20
 * OData: the rows are in value[]; paging is $top/$skip and the default order
 * is oldest-first. Column names are Dutch with a numeric suffix (CPI_1,
 * JaarmutatieCPI_5, Wegingscoefficient_7) and are emitted exactly as named.
 * Nulls are omitted rather than emitted as 0. The table id is swappable
 * across ~5,000 CBS tables. Keyless.
 * Licence: CBS open data, CC-BY 4.0. */
#include "od_shared.c"

#define SID "stats-nl-cbs-cpi"
static const char *URL =
  "https://opendata.cbs.nl/ODataApi/odata/83131NED/TypedDataSet?$top=20";
static const char *const TITLE_KEYS[] = { "Perioden", NULL };
static const char *const SUM_KEYS[] = { "Bestedingscategorieen", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "statistic-observation";
  sp.tags_json = "[\"statistics\",\"nl\",\"inflation\"]";
  sp.link = URL;
  sp.lang = "nl";
  sp.title_keys = TITLE_KEYS;
  sp.summary_keys = SUM_KEYS;
  sp.id_key = "ID";
  sp.title_prefix = "CBS 83131NED consumer prices";
  return od_rc(SID, od_fetch_rows(ctx, sink, URL, "value", &sp));
}

static const source_def od_stats_nl_cbs_def = {
  .id = SID, .collector = "statistics",
  .name = "Statistics Netherlands (CBS) OData - consumer price index",
  .update_interval_sec = 86400, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://opendata.cbs.nl/ODataApi/odata/83131NED/TypedDataSet?$top=20",
  .description = "Dutch CPI observations (index, annual change, weighting coefficient) per period and spending category from the CBS OData API",
  .license = "CC-BY 4.0 (CBS open data)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_stats_nl_cbs_def)
