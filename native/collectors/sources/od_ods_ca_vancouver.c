/* City of Vancouver open data (OpenDataSoft Explore v2.1) - catalogue tier.
 * Endpoint: https://opendata.vancouver.ca/api/explore/v2.1/catalog/datasets?limit=20
 * Emits, per dataset: dataset_id, title, description, publisher, licence,
 * records_count, theme/keyword and the modified stamp - every value read from
 * metas.default in the response. The per-dataset row tier lives at
 * /catalog/datasets/<dataset_id>/records and carries real geo
 * (geo_point_2d / geom), which is why has_geo is left unset here: the
 * catalogue tier returns no coordinates and none are invented (R2). Keyless.
 * Licence: Open Government Licence - Vancouver. */
#include "od_shared.c"

#define SID "ods-ca-vancouver"
static const char *URL =
  "https://opendata.vancouver.ca/api/explore/v2.1/catalog/datasets?limit=20";

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ods_catalog_collect(ctx, sink, URL,
    "https://opendata.vancouver.ca/explore/dataset/",
    "[\"opendata\",\"ods\",\"ca\"]"));
}

static const source_def od_ods_ca_vancouver_def = {
  .id = SID, .collector = "government",
  .name = "City of Vancouver open data (OpenDataSoft)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://opendata.vancouver.ca/api/explore/v2.1/catalog/datasets?limit=20",
  .description = "City of Vancouver open-data catalogue (197 datasets) with per-dataset row counts, licence and last-modified stamps",
  .license = "Open Government Licence - Vancouver",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_ca_vancouver_def)
