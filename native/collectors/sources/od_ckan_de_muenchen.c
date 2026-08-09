/* City of Munich open-data catalogue.
 * Endpoint: https://opendata.muenchen.de/api/3/action/package_search?rows=20
 * Emits, per dataset: title, organisation, author and author_email (the owning
 * city department), the DCAT-AP update frequency URI, notes and licence.
 * Keyless. Licence: CC-BY 4.0 / DL-DE-BY. */
#include "od_shared.c"

#define SID "ckan-de-muenchen"
static const char *URL =
  "https://opendata.muenchen.de/api/3/action/package_search?rows=20";
static const char *const EXTRA[] = { "notes", "frequency", "spatial_uri",
                                     "temporal_start", "temporal_end", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ckan_collect(ctx, sink, URL,
                                    "https://opendata.muenchen.de", EXTRA,
                                    "[\"opendata\",\"ckan\",\"de\"]"));
}

static const source_def od_ckan_de_muenchen_def = {
  .id = SID, .collector = "government",
  .name = "Munich opendata.muenchen.de CKAN",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://opendata.muenchen.de/api/3/action/package_search?rows=20",
  .description = "City of Munich open-data catalogue (336 datasets) with departmental owner emails and DCAT-AP update frequencies",
  .license = "CC-BY 4.0 / DL-DE-BY",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ckan_de_muenchen_def)
