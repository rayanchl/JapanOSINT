/* open.africa (Code for Africa) pan-African open-data catalogue.
 * Endpoint: https://open.africa/api/3/action/package_search?rows=20
 * (africaopendata.org 301-redirects here; the canonical host is used direct.)
 * Emits, per dataset: title, organisation, notes, license_title/license_url,
 * metadata_modified, resource count and the catalogue total - all parsed from
 * the CKAN response. Keyless.
 * Licence: Code for Africa portal, datasets predominantly CC-BY; the
 * per-dataset licence fields are emitted verbatim. */
#include "od_shared.c"

#define SID "ckan-open-africa"
static const char *URL =
  "https://open.africa/api/3/action/package_search?rows=20";
static const char *const EXTRA[] = { "notes", "tags", "groups", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ckan_collect(ctx, sink, URL, "https://open.africa",
                                    EXTRA,
                                    "[\"opendata\",\"ckan\",\"africa\"]"));
}

static const source_def od_ckan_open_africa_def = {
  .id = SID, .collector = "government",
  .name = "open.africa (africaopendata.org) CKAN",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://open.africa/api/3/action/package_search?rows=20",
  .description = "Pan-African open-data catalogue (6,981 datasets) from Code for Africa: national statistics, budgets, health and infrastructure",
  .license = "Code for Africa portal; datasets predominantly CC-BY",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ckan_open_africa_def)
