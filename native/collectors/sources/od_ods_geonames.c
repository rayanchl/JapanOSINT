/* OpenDataSoft public catalogue - GeoNames cities with population > 1000.
 * Endpoint: https://public.opendatasoft.com/api/explore/v2.1/catalog/datasets/
 *           geonames-all-cities-with-a-population-1000/records?limit=20
 * Row tier (results[]), 170,363 places. Emits, per record, every scalar field
 * the API returned (name, ascii_name, country_code, cou_name_en, population,
 * timezone, feature_code, admin codes, elevation, dem, modification_date).
 * R2: has_geo/lat/lon come only from the upstream coordinates{lon,lat} object
 * - no centroid or fallback point is ever substituted. Keyless.
 * Licence: GeoNames source data CC-BY 4.0; the ODS public hosting requires no
 * auth. */
#include "od_shared.c"

#define SID "ods-public-geonames"
static const char *URL =
  "https://public.opendatasoft.com/api/explore/v2.1/catalog/datasets/"
  "geonames-all-cities-with-a-population-1000/records?limit=20";
static const char *const TITLE_KEYS[] = { "name", "ascii_name", NULL };
static const char *const SUM_KEYS[]   = { "cou_name_en", "country_code", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "gazetteer-place";
  sp.tags_json = "[\"opendata\",\"gazetteer\",\"geonames\"]";
  sp.link = URL;
  sp.title_keys = TITLE_KEYS;
  sp.summary_keys = SUM_KEYS;
  sp.id_key = "geoname_id";
  sp.date_key = "modification_date";
  return od_rc(SID, od_fetch_rows(ctx, sink, URL, "results", &sp));
}

static const source_def od_ods_geonames_def = {
  .id = SID, .collector = "government",
  .name = "OpenDataSoft public catalogue - GeoNames cities >1000",
  .update_interval_sec = 604800, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://public.opendatasoft.com/api/explore/v2.1/catalog/datasets/geonames-all-cities-with-a-population-1000/records?limit=20",
  .description = "Keyless global gazetteer rows (place name, country, population, timezone, feature code) with upstream coordinates",
  .license = "GeoNames data CC-BY 4.0 (hosted on public.opendatasoft.com)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_geonames_def)
