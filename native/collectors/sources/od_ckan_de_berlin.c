/* Berlin Datenregister (municipal open-data register).
 * Endpoint: https://datenregister.berlin.de/api/3/action/package_search?rows=200&start=0
 * Distinct host from the already-covered ckan.govdata.de federal portal.
 * Emits, per dataset: title, organisation, author, notes, licence, and the
 * portal-specific berlin_type (datensatz vs dokument) / berlin_source fields.
 * Keyless. Licence: Datenlizenz Deutschland / CC-BY, per dataset. */
#include "od_shared.inc"

#define SID "ckan-de-berlin"
static const char *URL =
  "https://datenregister.berlin.de/api/3/action/package_search?rows=200&start=0&sort=id%20asc";  /* exhaustive-ok: walked by pw_walk/pw_walk_max below, which advances the offset or page this URL carries */
/* 2,627 datasets (result.count, measured 2026-10-12) at 200 a page
 * = 14; 30 is that with headroom. */
#define CKAN_PAGE_MAX 30  /* exhaustive-ok: page ceiling handed to pw_walk_max(); a stop at it files a collector-truncation-notice */
static const char *const EXTRA[] = { "notes", "berlin_type", "berlin_source",
                                     "geographical_granularity",
                                     "temporal_granularity", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ckan_walk(ctx, sink, SID, URL,
                                    "https://datenregister.berlin.de", EXTRA,
                                    "[\"opendata\",\"ckan\",\"de\"]",
                                    CKAN_PAGE_MAX));
}

static const source_def od_ckan_de_berlin_def = {
  .id = SID, .collector = "government",
  .name = "Berlin Datenregister CKAN",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://datenregister.berlin.de/api/3/action/package_search?rows=200&start=0&sort=id%20asc",  /* exhaustive-ok: metadata field, not a fetch; the row is walked by pw_walk below, which advances the offset this URL carries */
  .description = "Berlin municipal open-data register (2,619 datasets) with publishing authority and berlin_type/berlin_source provenance",
  .license = "Datenlizenz Deutschland / CC-BY (per dataset)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ckan_de_berlin_def)
