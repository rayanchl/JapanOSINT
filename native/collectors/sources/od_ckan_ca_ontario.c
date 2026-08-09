/* Ontario (Canada) provincial open-data catalogue.
 * Endpoint: https://data.ontario.ca/api/3/action/package_search?rows=20
 * PARSE TRAP: title, notes and access_instructions are OBJECTS
 * {"en":...,"fr":...}, not strings - cJSON_GetStringValue returns NULL on
 * them, so od_sml() descends into the language member (see od_shared.c).
 * Emits, per dataset: title, organisation, notes, access_level,
 * current_as_of, asset_type, exemption and metadata timestamps. Keyless.
 * Licence: Open Government Licence - Ontario. */
#include "od_shared.c"

#define SID "ckan-ca-ontario"
static const char *URL =
  "https://data.ontario.ca/api/3/action/package_search?rows=20";
static const char *const EXTRA[] = { "access_level", "current_as_of",
                                     "asset_type", "exemption",
                                     "update_frequency", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ckan_collect(ctx, sink, URL, "https://data.ontario.ca",
                                    EXTRA,
                                    "[\"opendata\",\"ckan\",\"ca\"]"));
}

static const source_def od_ckan_ca_ontario_def = {
  .id = SID, .collector = "government",
  .name = "Ontario (Canada) data.ontario.ca CKAN",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.ontario.ca/api/3/action/package_search?rows=20",
  .description = "Ontario provincial open-data catalogue (2,954 assets) with bilingual titles, access level and currency dates",
  .license = "Open Government Licence - Ontario",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ckan_ca_ontario_def)
