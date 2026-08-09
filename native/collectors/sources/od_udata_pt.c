/* Portugal dados.gov.pt open-data catalogue (udata, run by AMA).
 * Endpoint: https://dados.gov.pt/api/1/datasets/?page_size=20
 * Same udata envelope as data.public.lu: {"data":[...],"next_page":...}.
 * Emits, per dataset: title, description, publishing organisation,
 * created_at/last_modified, resource count and the upstream page URL.
 * Keyless. Licence: open portal, no auth; per-dataset licences (mostly CC-BY)
 * are emitted verbatim when present. */
#include "od_shared.c"

#define SID "ckan-pt-dadosgov"
static const char *URL = "https://dados.gov.pt/api/1/datasets/?page_size=20";

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_udata_collect(ctx, sink, URL,
                                     "[\"opendata\",\"udata\",\"pt\"]"));
}

static const source_def od_udata_pt_def = {
  .id = SID, .collector = "government",
  .name = "Portugal dados.gov.pt open-data catalogue",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://dados.gov.pt/api/1/datasets/?page_size=20",
  .description = "Portuguese national open-data portal (AMA): new and updated datasets from Portuguese public bodies",
  .license = "Open portal; per-dataset licences mostly CC-BY",
  .free_tier = 1,
};
REGISTER_SOURCE(od_udata_pt_def)
