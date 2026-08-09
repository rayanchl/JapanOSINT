/* energydata.info (World Bank / ESMAP) energy open-data catalogue.
 * Endpoint: https://energydata.info/api/3/action/package_search?rows=20
 * Emits, per dataset, only fields parsed out of the CKAN response: title,
 * publishing organisation, notes, license_title/license_url, country_code[]
 * (non-standard but genuinely populated here), metadata_modified, resource
 * count, and the catalogue total. Keyless (canonical CKAN action API).
 * Licence: World Bank open data; per-dataset licences are exposed in
 * license_title / license_url (mostly CC-BY-4.0) and emitted verbatim. */
#include "od_shared.c"

#define SID "ckan-energydata-info"
static const char *URL =
  "https://energydata.info/api/3/action/package_search?rows=20";
static const char *const EXTRA[] = { "country_code", "notes", "tags", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ckan_collect(ctx, sink, URL, "https://energydata.info",
                                    EXTRA,
                                    "[\"opendata\",\"ckan\",\"energy\"]"));
}

static const source_def od_ckan_energydata_info_def = {
  .id = SID, .collector = "government",
  .name = "energydata.info (World Bank/ESMAP) CKAN",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://energydata.info/api/3/action/package_search?rows=20",
  .description = "World Bank/ESMAP energy open-data catalogue: dataset title, organisation, licence and ISO3 country tags for 1,191 energy-sector datasets",
  .license = "World Bank open data; per-dataset licences in license_title/license_url (mostly CC-BY-4.0)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ckan_energydata_info_def)
