/* Kanton Thurgau open data (OpenDataSoft Explore v2.1) - catalogue tier.
 * Endpoint: https://data.tg.ch/api/explore/v2.1/catalog/datasets?limit=20
 * Includes republished MeteoSwiss station observation series.
 * Emits, per dataset: dataset_id, title, description, publisher, licence,
 * records_count, theme and modified - all from metas.default. Keyless.
 * Licence: CC-BY; Kanton Thurgau OGD terms. */
#include "od_shared.c"

#define SID "ods-ch-thurgau"
static const char *URL =
  "https://data.tg.ch/api/explore/v2.1/catalog/datasets?limit=20";

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ods_catalog_collect(ctx, sink, URL,
    "https://data.tg.ch/explore/dataset/",
    "[\"opendata\",\"ods\",\"ch\"]"));
}

static const source_def od_ods_ch_thurgau_def = {
  .id = SID, .collector = "government",
  .name = "Kanton Thurgau open data (OpenDataSoft)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.tg.ch/api/explore/v2.1/catalog/datasets?limit=20",
  .description = "Canton Thurgau open-data catalogue (447 datasets) including republished MeteoSwiss station series",
  .license = "CC-BY; Kanton Thurgau OGD terms",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_ch_thurgau_def)
