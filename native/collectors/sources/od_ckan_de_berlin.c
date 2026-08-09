/* Berlin Datenregister (municipal open-data register).
 * Endpoint: https://datenregister.berlin.de/api/3/action/package_search?rows=20
 * Distinct host from the already-covered ckan.govdata.de federal portal.
 * Emits, per dataset: title, organisation, author, notes, licence, and the
 * portal-specific berlin_type (datensatz vs dokument) / berlin_source fields.
 * Keyless. Licence: Datenlizenz Deutschland / CC-BY, per dataset. */
#include "od_shared.c"

#define SID "ckan-de-berlin"
static const char *URL =
  "https://datenregister.berlin.de/api/3/action/package_search?rows=20";
static const char *const EXTRA[] = { "notes", "berlin_type", "berlin_source",
                                     "geographical_granularity",
                                     "temporal_granularity", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ckan_collect(ctx, sink, URL,
                                    "https://datenregister.berlin.de", EXTRA,
                                    "[\"opendata\",\"ckan\",\"de\"]"));
}

static const source_def od_ckan_de_berlin_def = {
  .id = SID, .collector = "government",
  .name = "Berlin Datenregister CKAN",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://datenregister.berlin.de/api/3/action/package_search?rows=20",
  .description = "Berlin municipal open-data register (2,619 datasets) with publishing authority and berlin_type/berlin_source provenance",
  .license = "Datenlizenz Deutschland / CC-BY (per dataset)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ckan_de_berlin_def)
