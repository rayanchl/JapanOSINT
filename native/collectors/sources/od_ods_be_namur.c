/* Ville de Namur open data (OpenDataSoft Explore v2.1) - catalogue tier.
 * Endpoint: https://data.namur.be/api/explore/v2.1/catalog/datasets?limit=100&offset=0
 * Includes republished Statbel income-by-statistical-sector data.
 * Emits, per dataset: dataset_id, title, description, publisher, licence,
 * records_count and modified - all from metas.default. Keyless.
 * Licence: Ville de Namur / Statbel open data, reuse with attribution. */
#include "od_shared.inc"

#define SID "ods-be-namur"
static const char *URL =
  "https://data.namur.be/api/explore/v2.1/catalog/datasets?limit=100&offset=0&order_by=dataset_id";  /* exhaustive-ok: walked by pw_walk/pw_walk_max below, which advances the offset or page this URL carries */

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ods_catalog_walk(ctx, sink, SID, URL,
    "https://data.namur.be/explore/dataset/",
    "[\"opendata\",\"ods\",\"be\"]"));
}

static const source_def od_ods_be_namur_def = {
  .id = SID, .collector = "government",
  .name = "Ville de Namur open data (OpenDataSoft)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.namur.be/api/explore/v2.1/catalog/datasets?limit=100&offset=0&order_by=dataset_id",  /* exhaustive-ok: metadata field, not a fetch; the row is walked by pw_walk below, which advances the offset this URL carries */
  .description = "City of Namur open-data catalogue (295 datasets) including republished Statbel income data",
  .license = "Ville de Namur / Statbel open data; reuse with attribution",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_be_namur_def)
