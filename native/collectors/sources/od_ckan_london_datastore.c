/* London Datastore (Greater London Authority).
 * Endpoint: https://data.london.gov.uk/api/action/package_search?rows=20
 * NON-STANDARD ENVELOPE: the path has no /3/ and the array is result.result[],
 * not result.results[] (od_ckan_collect falls back to it); owner_org here is a
 * human-readable organisation name rather than a UUID, so it is used as the
 * organisation when no organization object is present.
 * Emits, per dataset: title, owning organisation, notes, maintainer and
 * maintainer_email, licence, metadata timestamps - all upstream fields.
 * Keyless. Licence: UK Open Government Licence v3 for most datasets. */
#include "od_shared.c"

#define SID "ckan-london-datastore"
static const char *URL =
  "https://data.london.gov.uk/api/action/package_search?rows=20";
static const char *const EXTRA[] = { "notes", "owner_org", "update_frequency",
                                     "london_smallest_geography", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ckan_collect(ctx, sink, URL,
                                    "https://data.london.gov.uk", EXTRA,
                                    "[\"opendata\",\"ckan\",\"uk\"]"));
}

static const source_def od_ckan_london_datastore_def = {
  .id = SID, .collector = "government",
  .name = "London Datastore (GLA)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.london.gov.uk/api/action/package_search?rows=20",
  .description = "Greater London Authority datastore: 1,301 London datasets (rough sleeping, crime, housing, transport) with named maintainer teams and contact emails",
  .license = "UK Open Government Licence v3 (most datasets)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ckan_london_datastore_def)
