/* Stadt Zurich open-data catalogue (city statistics office).
 * Endpoint: https://data.stadt-zuerich.ch/api/3/action/package_search?rows=200&start=0
 * Distinct from the already-covered national opendata.swiss.
 * Emits, per dataset: title, organisation, author (issuing office), notes,
 * licence, dataQuality and metadata_modified. Keyless.
 * Licence: CC-BY / City of Zurich open-data terms. */
#include "od_shared.inc"

#define SID "ckan-ch-zurich"
static const char *URL =
  "https://data.stadt-zuerich.ch/api/3/action/package_search?rows=200&start=0&sort=id%20asc";
/* 942 datasets (result.count, measured 2026-10-12) at 200 a page
 * = 5; 20 is that with headroom. */
#define CKAN_PAGE_MAX 20  /* exhaustive-ok: page ceiling handed to pw_walk_max(); a stop at it files a collector-truncation-notice */
static const char *const EXTRA[] = { "notes", "dataQuality", "sszBemerkungen",
                                     "dateFirstPublished", "updateInterval",
                                     NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ckan_walk(ctx, sink, SID, URL,
                                    "https://data.stadt-zuerich.ch", EXTRA,
                                    "[\"opendata\",\"ckan\",\"ch\"]",
                                    CKAN_PAGE_MAX));
}

static const source_def od_ckan_ch_zurich_def = {
  .id = SID, .collector = "government",
  .name = "Stadt Zurich open-data CKAN",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.stadt-zuerich.ch/api/3/action/package_search?rows=200&start=0&sort=id%20asc",
  .description = "City of Zurich open-data catalogue (937 datasets) from the city statistics office",
  .license = "CC-BY / City of Zurich open-data terms",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ckan_ch_zurich_def)
