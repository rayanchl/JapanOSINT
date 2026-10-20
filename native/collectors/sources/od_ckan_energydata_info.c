/* energydata.info (World Bank / ESMAP) energy open-data catalogue.
 * Endpoint: https://energydata.info/api/3/action/package_search?rows=200&start=0
 * Emits, per dataset, only fields parsed out of the CKAN response: title,
 * publishing organisation, notes, license_title/license_url, country_code[]
 * (non-standard but genuinely populated here), metadata_modified, resource
 * count, and the catalogue total. Keyless (canonical CKAN action API).
 * Licence: World Bank open data; per-dataset licences are exposed in
 * license_title / license_url (mostly CC-BY-4.0) and emitted verbatim. */
#include "od_shared.inc"

#define SID "ckan-energydata-info"
static const char *URL =
  "https://energydata.info/api/3/action/package_search?rows=200&start=0&sort=id%20asc";
/* 1,191 datasets (result.count, measured 2026-10-12) at 200 a page
 * = 6; 20 is that with headroom. */
#define CKAN_PAGE_MAX 20  /* exhaustive-ok: page ceiling handed to pw_walk_max(); a stop at it files a collector-truncation-notice */
static const char *const EXTRA[] = { "country_code", "notes", "tags", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ckan_walk(ctx, sink, SID, URL,
                                    "https://energydata.info",
                                    EXTRA,
                                    "[\"opendata\",\"ckan\",\"energy\"]",
                                    CKAN_PAGE_MAX));
}

static const source_def od_ckan_energydata_info_def = {
  .id = SID, .collector = "government",
  .name = "energydata.info (World Bank/ESMAP) CKAN",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://energydata.info/api/3/action/package_search?rows=200&start=0&sort=id%20asc",
  .description = "World Bank/ESMAP energy open-data catalogue: dataset title, organisation, licence and ISO3 country tags for 1,191 energy-sector datasets",
  .license = "World Bank open data; per-dataset licences in license_title/license_url (mostly CC-BY-4.0)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ckan_energydata_info_def)
