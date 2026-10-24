/* Toulouse Metropole open data (OpenDataSoft Explore v2.1) - catalogue tier.
 * Endpoint: https://data.toulouse-metropole.fr/api/explore/v2.1/catalog/datasets?limit=100&offset=0
 * The subsidy registers on this portal carry SIRET company identifiers, which
 * join to the existing French company-registry sources. (In the row tier
 * ndeg_siret comes back as a JSON NUMBER wider than 32 bits; that tier is not
 * emitted here, only the catalogue.)
 * Emits, per dataset: dataset_id, title, description, publisher, licence,
 * records_count and modified - all from metas.default. Keyless.
 * Licence: Licence Ouverte (Etalab). */
#include "od_shared.inc"

#define SID "ods-fr-toulouse"
static const char *URL =
  "https://data.toulouse-metropole.fr/api/explore/v2.1/catalog/datasets?limit=100&offset=0&order_by=dataset_id";  /* exhaustive-ok: walked by pw_walk/pw_walk_max below, which advances the offset or page this URL carries */

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_ods_catalog_walk(ctx, sink, SID, URL,
    "https://data.toulouse-metropole.fr/explore/dataset/",
    "[\"opendata\",\"ods\",\"fr\"]"));
}

static const source_def od_ods_fr_toulouse_def = {
  .id = SID, .collector = "government",
  .name = "Toulouse Metropole open data (OpenDataSoft)",
  .update_interval_sec = 86400, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.toulouse-metropole.fr/api/explore/v2.1/catalog/datasets?limit=100&offset=0&order_by=dataset_id",  /* exhaustive-ok: metadata field, not a fetch; the row is walked by pw_walk below, which advances the offset this URL carries */
  .description = "Toulouse Metropole open-data catalogue (842 datasets) including subsidy registers with SIRET identifiers",
  .license = "Licence Ouverte (Etalab)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_fr_toulouse_def)
