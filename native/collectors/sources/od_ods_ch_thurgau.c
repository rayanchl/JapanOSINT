/* Kanton Thurgau open data (OpenDataSoft Explore v2.1) - catalogue tier.
 * Endpoint: https://data.tg.ch/api/explore/v2.1/catalog/datasets?limit=100&offset=0
 * Includes republished MeteoSwiss station observation series.
 * Emits, per dataset: dataset_id, title, description, publisher, licence,
 * records_count, theme and modified - all from metas.default. Keyless.
 * Licence: CC-BY; Kanton Thurgau OGD terms. */
#include "od_shared.inc"

#define SID "ods-ch-thurgau"
static const char *URL =
  "https://data.tg.ch/api/explore/v2.1/catalog/datasets?limit=100&offset=0&order_by=dataset_id";  /* exhaustive-ok: walked by pw_walk/pw_walk_max below, which advances the offset or page this URL carries */

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ods_catalog_walk(ctx, sink, SID, URL,
    "https://data.tg.ch/explore/dataset/",
    "[\"opendata\",\"ods\",\"ch\"]"));
}

static const source_def od_ods_ch_thurgau_def = {
  .id = SID, .collector = "government",
  .name = "Kanton Thurgau open data (OpenDataSoft)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.tg.ch/api/explore/v2.1/catalog/datasets?limit=100&offset=0&order_by=dataset_id",  /* exhaustive-ok: metadata field, not a fetch; the row is walked by pw_walk below, which advances the offset this URL carries */
  .description = "Canton Thurgau open-data catalogue (447 datasets) including republished MeteoSwiss station series",
  .license = "CC-BY; Kanton Thurgau OGD terms",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_ch_thurgau_def)
