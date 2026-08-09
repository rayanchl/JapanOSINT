/* South Australia open-data catalogue.
 * Endpoint: https://data.sa.gov.au/data/api/3/action/package_search?rows=20
 * (the CKAN base includes the /data path segment, as with NSW).
 * Emits, per dataset: title, organisation, notes, contact_point (departmental
 * email), data_state, licence and metadata timestamps. Keyless.
 * Licence: CC-BY 4.0 for most datasets. */
#include "od_shared.c"

#define SID "ckan-au-sa"
static const char *URL =
  "https://data.sa.gov.au/data/api/3/action/package_search?rows=20";
static const char *const EXTRA[] = { "notes", "contact_point", "data_state",
                                     "update_frequency", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ckan_collect(ctx, sink, URL,
                                    "https://data.sa.gov.au/data", EXTRA,
                                    "[\"opendata\",\"ckan\",\"au\"]"));
}

static const source_def od_ckan_au_sa_def = {
  .id = SID, .collector = "government",
  .name = "South Australia data.sa.gov.au CKAN",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.sa.gov.au/data/api/3/action/package_search?rows=20",
  .description = "South Australian government open-data catalogue (1,915 datasets) with departmental contact points",
  .license = "CC-BY 4.0 for most datasets",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ckan_au_sa_def)
