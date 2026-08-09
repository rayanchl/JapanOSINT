/* Rennes Metropole open data (OpenDataSoft Explore v2.1) - catalogue tier.
 * Endpoint: https://data.rennesmetropole.fr/api/explore/v2.1/catalog/datasets?limit=20
 * Emits, per dataset: dataset_id, title, description, publisher, licence,
 * records_count and modified - all from metas.default. Keyless.
 * Licence: Licence Ouverte / ODbL depending on the dataset; the per-dataset
 * licence field is emitted verbatim. */
#include "od_shared.c"

#define SID "ods-fr-rennes"
static const char *URL =
  "https://data.rennesmetropole.fr/api/explore/v2.1/catalog/datasets?limit=20";

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ods_catalog_collect(ctx, sink, URL,
    "https://data.rennesmetropole.fr/explore/dataset/",
    "[\"opendata\",\"ods\",\"fr\"]"));
}

static const source_def od_ods_fr_rennes_def = {
  .id = SID, .collector = "government",
  .name = "Rennes Metropole open data (OpenDataSoft)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.rennesmetropole.fr/api/explore/v2.1/catalog/datasets?limit=20",
  .description = "Rennes Metropole open-data catalogue (681 datasets): budgets, subsidies, mobility and live STAR transit feeds",
  .license = "Licence Ouverte / ODbL (per dataset)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_fr_rennes_def)
