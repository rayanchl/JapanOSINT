/* Statistics Netherlands (CBS) OData v3 - consumer price index table 83131NED.
 * Endpoint:
 *   https://opendata.cbs.nl/ODataFeed/odata/83131NED/TypedDataSet?$format=json&$top=1000&$skip=0
 * OData: the rows are in value[]; paging is $top/$skip and the default order
 * is oldest-first. Column names are Dutch with a numeric suffix (CPI_1,
 * JaarmutatieCPI_5, Wegingscoefficient_7) and are emitted exactly as named.
 * Nulls are omitted rather than emitted as 0. The table id is swappable
 * across ~5,000 CBS tables. Keyless.
 * Licence: CBS open data, CC-BY 4.0.
 *
 * TWO CHANGES, both measured 2026-10-12.
 *
 * 1. The HOST PATH. This read `/ODataApi/`, which answers `$skip` with
 *    HTTP 500 and the explanation in its own body: "The 'Skip' query option is
 *    not supported on the ODataApi. This functionality is available only on the
 *    ODataFeed endpoint." `/ODataFeed/` takes $skip and is otherwise the same
 *    service, so every CBS row that needs to page needs this path. It defaults
 *    to Atom XML, hence `$format=json` — without it the response is HTTP 200
 *    and unparseable, which looks like a dead source rather than a wrong
 *    Accept.
 *
 * 2. `$top=20` was a bound on a finite table: 20 rows of the 159,120 the table
 *    holds (`/TypedDataSet/$count`), every day, forever — the seventh discard
 *    shape (tools/audit_url_limits.py;
 *    docs/measured-url-limits-2026-10-11.tsv). With `$skip=0` in the URL,
 *    lib/pagewalk.c advances an offset the author wrote; 200 pages of 1,000
 *    covers 200,000 against the measured 159,120, and `$inlinecount` is
 *    IGNORED by this service, so the completeness evidence is the short final
 *    page rather than a declared total.
 *
 * NOTE: `$top` is a PAGE SIZE here and `$skip` the offset, which is the right
 * pairing. Do not "simplify" it to a page number — see the page-size-on-
 * page-number trap in CLAUDE.md. */
#include "od_shared.inc"

#define SID "stats-nl-cbs-cpi"
static const char *URL =
  "https://opendata.cbs.nl/ODataFeed/odata/83131NED/TypedDataSet"
  "?$format=json&$top=1000&$skip=0";  /* exhaustive-ok: walked by lib/pagewalk.c, which advances the offset/page this URL carries */

/* 159,120 rows at 1,000 a page = 160; 200 is that with headroom. */
#define NL_CBS_PAGE_MAX 200  /* exhaustive-ok: page ceiling handed to od_walk_rows_max(); a stop at it files a collector-truncation-notice */
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
  return od_rc(SID, od_walk_rows_max(ctx, sink, SID, URL, "value", &sp,
                                    NL_CBS_PAGE_MAX));
}

static const source_def od_stats_nl_cbs_def = {
  .id = SID, .collector = "statistics",
  .name = "Statistics Netherlands (CBS) OData - consumer price index",
  .update_interval_sec = 86400, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://opendata.cbs.nl/ODataFeed/odata/83131NED/TypedDataSet?$format=json&$top=1000&$skip=0",  /* exhaustive-ok: documentation of the row, never fetched; the walk is pw_walk_max() above */
  .description = "Dutch CPI observations (index, annual change, weighting coefficient) per period and spending category from the CBS OData API",
  .license = "CC-BY 4.0 (CBS open data)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_stats_nl_cbs_def)
