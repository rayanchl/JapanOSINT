/* Luxembourg data.public.lu open-data catalogue (udata, NOT CKAN).
 * Endpoint: https://data.public.lu/api/1/datasets/?page_size=20
 * Envelope is {"data":[...],"next_page":...} - there is no result.results[]
 * wrapper, and organization/owner are objects (od_udata_collect handles both).
 * Emits, per dataset: title, acronym, description, publishing organisation,
 * created_at/last_modified, resource count and the upstream dataset page URL.
 * Keyless.
 * Licence: portal metadata is open, most datasets CC-BY / CC0; no stated
 * restriction on API reuse. */
#include "od_shared.c"

#define SID "ckan-lu-datapublic"
static const char *URL = "https://data.public.lu/api/1/datasets/?page_size=20";

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_udata_collect(ctx, sink, URL,
                                     "[\"opendata\",\"udata\",\"lu\"]"));
}

static const source_def od_udata_lu_def = {
  .id = SID, .collector = "government",
  .name = "Luxembourg data.public.lu open-data catalogue",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.public.lu/api/1/datasets/?page_size=20",
  .description = "Luxembourg national open-data portal: new and updated dataset titles, publishing organisation, description and resources",
  .license = "Portal metadata open; datasets mostly CC-BY / CC0",
  .free_tier = 1,
};
REGISTER_SOURCE(od_udata_lu_def)
