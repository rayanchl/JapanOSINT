/* Queensland (Australia) open-data catalogue.
 * Endpoint: https://www.data.qld.gov.au/api/3/action/package_search?rows=20
 * Emits, per dataset: title, organisation, notes, licence, author_email and
 * the non-standard but populated data_last_updated, de_identified_data and
 * data_driven_application fields. Keyless.
 * Licence: Queensland Government open data, mostly CC-BY 4.0. */
#include "od_shared.c"

#define SID "ckan-au-qld"
static const char *URL =
  "https://www.data.qld.gov.au/api/3/action/package_search?rows=20";
static const char *const EXTRA[] = { "notes", "data_last_updated",
                                     "de_identified_data",
                                     "data_driven_application",
                                     "update_frequency", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ckan_collect(ctx, sink, URL,
                                    "https://www.data.qld.gov.au", EXTRA,
                                    "[\"opendata\",\"ckan\",\"au\"]"));
}

static const source_def od_ckan_au_qld_def = {
  .id = SID, .collector = "government",
  .name = "Queensland (Australia) data.qld.gov.au CKAN",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://www.data.qld.gov.au/api/3/action/package_search?rows=20",
  .description = "Queensland state open-data catalogue (188,836 packages) with departmental contact emails and per-dataset last-updated stamps",
  .license = "Queensland Government open data, mostly CC-BY 4.0",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ckan_au_qld_def)
