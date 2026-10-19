/* SNCF Open Data - French passenger rail stations.
 * Endpoint: https://data.sncf.com/api/explore/v2.1/catalog/datasets/
 *           gares-de-voyageurs/records?limit=100&offset=0&order_by=id
 * Row tier (results[]), 2,782 stations. Emits, per station, every scalar the
 * API returned: nom, libellecourt, segment_drg, codeinsee (commune),
 * codes_uic (joins to European rail reference data), niveau_de_service, id.
 * R2: coordinates come only from the upstream position_geographique{lon,lat}.
 * Keyless. Licence: Licence Ouverte / ODbL depending on the dataset; SNCF
 * open-data terms permit reuse with attribution. */
#include "od_shared.inc"

#define SID "ods-sncf-stations"
static const char *URL =
  "https://data.sncf.com/api/explore/v2.1/catalog/datasets/"
  "gares-de-voyageurs/records?limit=100&offset=0&order_by=id";
/* PAGED 2026-10-12. This made ONE request:
 * 2,792 passenger stations (total_count, measured 2026-10-12), 100 stored.
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
#define ODS_PAGE_MAX 40  /* exhaustive-ok: page ceiling handed to od_walk_rows_max(); a stop at it files a collector-truncation-notice */
static const char *const TITLE_KEYS[] = { "nom", "libellecourt", NULL };
static const char *const SUM_KEYS[] = { "segment_drg", "codeinsee", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "rail-station";
  sp.tags_json = "[\"opendata\",\"rail\",\"fr\"]";
  sp.link = URL;
  sp.lang = "fr";
  sp.title_keys = TITLE_KEYS;
  sp.summary_keys = SUM_KEYS;
  sp.id_key = "id";
  return od_rc(SID, od_walk_rows_max(ctx, sink, SID, URL, "results", &sp,
                                   ODS_PAGE_MAX));
}

static const source_def od_ods_sncf_stations_def = {
  .id = SID, .collector = "government",
  .name = "SNCF Open Data - passenger stations",
  .update_interval_sec = 604800, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.sncf.com/api/explore/v2.1/catalog/datasets/gares-de-voyageurs/records?limit=100&offset=0&order_by=id",
  .description = "French passenger rail stations with UIC codes, INSEE commune codes and upstream coordinates",
  .license = "Licence Ouverte / ODbL (SNCF open-data terms)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_sncf_stations_def)
