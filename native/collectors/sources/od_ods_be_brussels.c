/* City of Brussels open data (OpenDataSoft Explore v2.1) - catalogue tier.
 * Endpoint: https://opendata.brussels.be/api/explore/v2.1/catalog/datasets?limit=20
 * Notable content: municipal public-procurement award registers, a direct
 * entity-pivot surface.
 * Emits, per dataset: dataset_id, title, description, dcat creator/issued,
 * licence, records_count and has_records - all read from the response. Some
 * catalogue entries are metadata-only (has_records=false); that flag is
 * emitted verbatim so a row fetch is not attempted against an empty dataset.
 * Keyless. Licence: Ville de Bruxelles open-data terms, reuse with
 * attribution. */
#include "od_shared.c"

#define SID "ods-be-brussels"
static const char *URL =
  "https://opendata.brussels.be/api/explore/v2.1/catalog/datasets?limit=20";

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ods_catalog_collect(ctx, sink, URL,
    "https://opendata.brussels.be/explore/dataset/",
    "[\"opendata\",\"ods\",\"be\"]"));
}

static const source_def od_ods_be_brussels_def = {
  .id = SID, .collector = "government",
  .name = "Brussels City open data (OpenDataSoft)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://opendata.brussels.be/api/explore/v2.1/catalog/datasets?limit=20",
  .description = "City of Brussels open-data catalogue (207 datasets) including municipal public-procurement award registers",
  .license = "Ville de Bruxelles open-data terms; reuse with attribution",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_be_brussels_def)
