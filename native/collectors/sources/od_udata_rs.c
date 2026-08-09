/* Serbia data.gov.rs open-data catalogue (udata).
 * Endpoint: https://data.gov.rs/api/1/datasets/?page_size=20
 * The udata API lives at /api/1/; the CKAN path /api/3/action/package_search
 * returns the HTML homepage with HTTP 404 and must not be used.
 * Emits, per dataset: title, acronym, description, organisation,
 * created_at/last_modified, resource count and the upstream page URL.
 * Includes the mandatory retail price-list publications by Serbian companies,
 * which makes this a commercial-entity pivot. Keyless.
 * Licence: public portal, no auth, no stated reuse restriction. */
#include "od_shared.c"

#define SID "ckan-rs-datagovrs"
static const char *URL = "https://data.gov.rs/api/1/datasets/?page_size=20";

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_udata_collect(ctx, sink, URL,
                                     "[\"opendata\",\"udata\",\"rs\"]"));
}

static const source_def od_udata_rs_def = {
  .id = SID, .collector = "government",
  .name = "Serbia data.gov.rs open-data catalogue",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.gov.rs/api/1/datasets/?page_size=20",
  .description = "Serbian national open-data catalogue, including mandatory retail price-list publications by Serbian companies",
  .license = "Public portal, no stated reuse restriction",
  .free_tier = 1,
};
REGISTER_SOURCE(od_udata_rs_def)
