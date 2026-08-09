/* New South Wales (Australia) open-data catalogue.
 * Endpoint: https://data.nsw.gov.au/data/api/3/action/package_search?rows=20
 * (the CKAN base includes the /data path segment; omitting it 404s).
 * Emits, per dataset: title, organisation, notes, license_title,
 * metadata_modified, resource count - parsed from the CKAN response. Keyless.
 * Licence: CC-BY 4.0 for most; the per-dataset license_title is emitted
 * verbatim rather than assumed. */
#include "od_shared.c"

#define SID "ckan-au-nsw"
static const char *URL =
  "https://data.nsw.gov.au/data/api/3/action/package_search?rows=20";
static const char *const EXTRA[] = { "notes", "update_freq", "contact_point",
                                     "data_portal", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ckan_collect(ctx, sink, URL,
                                    "https://data.nsw.gov.au/data", EXTRA,
                                    "[\"opendata\",\"ckan\",\"au\"]"));
}

static const source_def od_ckan_au_nsw_def = {
  .id = SID, .collector = "government",
  .name = "NSW (Australia) data.nsw.gov.au CKAN",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.nsw.gov.au/data/api/3/action/package_search?rows=20",
  .description = "New South Wales state open-data catalogue (16,997 datasets), the largest Australian sub-national portal",
  .license = "Per-dataset; mostly CC-BY 4.0 (license_title emitted verbatim)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ckan_au_nsw_def)
