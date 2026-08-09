/* Kanton Basel-Stadt open data (OpenDataSoft Explore v2.1) - catalogue tier.
 * Endpoint: https://data.bs.ch/api/explore/v2.1/catalog/datasets?limit=20
 * Cantonal Swiss data that is not on the already-covered federal
 * opendata.swiss. Dataset ids on this instance are numeric strings (100085).
 * Emits, per dataset: dataset_id, title, description, publisher, licence,
 * records_count, theme and modified - all from metas.default. Keyless.
 * Licence: CC-BY / Open-Government-Data Basel-Stadt terms. */
#include "od_shared.c"

#define SID "ods-ch-basel"
static const char *URL =
  "https://data.bs.ch/api/explore/v2.1/catalog/datasets?limit=20";

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ods_catalog_collect(ctx, sink, URL,
    "https://data.bs.ch/explore/dataset/",
    "[\"opendata\",\"ods\",\"ch\"]"));
}

static const source_def od_ods_ch_basel_def = {
  .id = SID, .collector = "government",
  .name = "Kanton Basel-Stadt open data (OpenDataSoft)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.bs.ch/api/explore/v2.1/catalog/datasets?limit=20",
  .description = "Canton Basel-Stadt open-data catalogue (352 datasets) with row counts and last-modified stamps",
  .license = "CC-BY / OGD Basel-Stadt terms",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_ch_basel_def)
