/* South Australia open-data catalogue.
 * Endpoint: https://data.sa.gov.au/data/api/3/action/package_search?rows=200&start=0
 * (the CKAN base includes the /data path segment, as with NSW).
 * Emits, per dataset: title, organisation, notes, contact_point (departmental
 * email), data_state, licence and metadata timestamps. Keyless.
 * Licence: CC-BY 4.0 for most datasets. */
#include "od_shared.inc"

#define SID "ckan-au-sa"
static const char *URL =
  "https://data.sa.gov.au/data/api/3/action/package_search?rows=200&start=0&sort=id%20asc";  /* exhaustive-ok: walked by pw_walk/pw_walk_max below, which advances the offset or page this URL carries */
/* 1,929 datasets (result.count, measured 2026-10-12) at 200 a page
 * = 10; 30 is that with headroom. */
#define CKAN_PAGE_MAX 30  /* exhaustive-ok: page ceiling handed to pw_walk_max(); a stop at it files a collector-truncation-notice */
static const char *const EXTRA[] = { "notes", "contact_point", "data_state",
                                     "update_frequency", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ckan_walk(ctx, sink, SID, URL,
                                    "https://data.sa.gov.au/data", EXTRA,
                                    "[\"opendata\",\"ckan\",\"au\"]",
                                    CKAN_PAGE_MAX));
}

static const source_def od_ckan_au_sa_def = {
  .id = SID, .collector = "government",
  .name = "South Australia data.sa.gov.au CKAN",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.sa.gov.au/data/api/3/action/package_search?rows=200&start=0&sort=id%20asc",  /* exhaustive-ok: metadata field, not a fetch; the row is walked by pw_walk below, which advances the offset this URL carries */
  .description = "South Australian government open-data catalogue (1,915 datasets) with departmental contact points",
  .license = "CC-BY 4.0 for most datasets",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ckan_au_sa_def)
