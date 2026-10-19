/* OpenDataSoft public catalogue - GeoNames cities with population > 1000.
 * Endpoint: https://public.opendatasoft.com/api/explore/v2.1/catalog/datasets/
 *           geonames-all-cities-with-a-population-1000/records?limit=100&offset=0&order_by=geoname_id
 * Row tier (results[]), 170,363 places. Emits, per record, every scalar field
 * the API returned (name, ascii_name, country_code, cou_name_en, population,
 * timezone, feature_code, admin codes, elevation, dem, modification_date).
 * R2: has_geo/lat/lon come only from the upstream coordinates{lon,lat} object
 * - no centroid or fallback point is ever substituted. Keyless.
 * Licence: GeoNames source data CC-BY 4.0; the ODS public hosting requires no
 * auth. */
#include "od_shared.inc"

#define SID "ods-public-geonames"
static const char *URL =
  "https://public.opendatasoft.com/api/explore/v2.1/catalog/datasets/"
  "geonames-all-cities-with-a-population-1000/records?limit=100&offset=0&order_by=geoname_id";
/* PAGED 2026-10-12. This made ONE request:
 * 171,176 places (total_count, measured 2026-10-12) and TWENTY were stored.
 * The ceiling here is the UPSTREAM's, not ours: ODS refuses offset+limit past
 * 10,000 ("Invalid value for sum of offset + limit API parameter"), so 100
 * pages of 100 is every record this API will serve and the remaining 161,176
 * are a disclosed shortfall. The complete set is one request to
 * .../exports/json, measured at 111 MB for all 171,176 rows -- that needs a
 * streaming parse this collector does not have, so it is named in the notice
 * as the remedy rather than silently attempted.
 */
/* ORDER_BY IS NOT COSMETIC ON AN OFFSET WALK (measured 2026-10-12). ODS
 * returns records in no guaranteed order, and this dataset updates live, so
 * paging by `offset` without a stable sort re-serves some records on a later
 * page and SKIPS others entirely. The skips are invisible; the re-serves show
 * up as a UID-COLLISION on the run line, which is how this was found: Velib
 * reported 1,519 emitted / 1,508 stored, and a `where=stationcode="13022"`
 * query proved the dataset holds exactly ONE record for the code that had
 * supposedly appeared twice. A collision that is really a skip is the worst
 * shape in this file -- the visible half is harmless and the invisible half is
 * lost data. `order_by` on the row's own id makes the walk a stable sequence.
 */
#define ODS_PAGE_MAX 100  /* exhaustive-ok: page ceiling handed to od_walk_rows_max(); a stop at it files a collector-truncation-notice */
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
  return od_rc(SID, od_walk_rows_max(ctx, sink, SID, URL, "results", &sp,
                                   ODS_PAGE_MAX));
}

static const source_def od_ods_geonames_def = {
  .id = SID, .collector = "government",
  .name = "OpenDataSoft public catalogue - GeoNames cities >1000",
  .update_interval_sec = 604800, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://public.opendatasoft.com/api/explore/v2.1/catalog/datasets/geonames-all-cities-with-a-population-1000/records?limit=100&offset=0&order_by=geoname_id",
  .description = "Keyless global gazetteer rows (place name, country, population, timezone, feature code) with upstream coordinates",
  .license = "GeoNames data CC-BY 4.0 (hosted on public.opendatasoft.com)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_geonames_def)
