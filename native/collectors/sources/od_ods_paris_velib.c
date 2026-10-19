/* Paris Open Data - Velib bike-share station availability (real time).
 * Endpoint: https://opendata.paris.fr/api/explore/v2.1/catalog/datasets/
 *           velib-disponibilite-en-temps-reel/records?limit=100&offset=0&order_by=stationcode
 * Row tier (results[]), 1,518 stations. Emits, per station, every scalar the
 * API returned: stationcode, name, capacity, numdocksavailable,
 * numbikesavailable, ebike, mechanical, is_installed/is_renting/is_returning,
 * nom_arrondissement_communes and the upstream duedate, which is used as
 * published_at (the freshness stamp of the reading).
 * R2: coordinates come only from the upstream coordonnees_geo{lon,lat}.
 * Keyless. Licence: ODbL / Licence Ouverte; Paris open-data terms permit
 * reuse with attribution. */
#include "od_shared.inc"

#define SID "ods-paris-velib"
static const char *URL =
  "https://opendata.paris.fr/api/explore/v2.1/catalog/datasets/"
  "velib-disponibilite-en-temps-reel/records?limit=100&offset=0&order_by=stationcode";
/* PAGED 2026-10-12. This made ONE request:
 * 1,519 Velib stations (total_count, measured 2026-10-12), 100 stored.
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
#define ODS_PAGE_MAX 30  /* exhaustive-ok: page ceiling handed to od_walk_rows_max(); a stop at it files a collector-truncation-notice */
static const char *const TITLE_KEYS[] = { "name", "stationcode", NULL };
static const char *const SUM_KEYS[] = { "nom_arrondissement_communes", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  od_rowspec sp = {0};
  sp.record_type = "bike-share-station";
  sp.tags_json = "[\"opendata\",\"mobility\",\"fr\"]";
  sp.link = URL;
  sp.lang = "fr";
  sp.title_keys = TITLE_KEYS;
  sp.summary_keys = SUM_KEYS;
  sp.id_key = "stationcode";
  sp.date_key = "duedate";
  return od_rc(SID, od_walk_rows_max(ctx, sink, SID, URL, "results", &sp,
                                   ODS_PAGE_MAX));
}

static const source_def od_ods_paris_velib_def = {
  .id = SID, .collector = "government",
  .name = "Paris Open Data - Velib station availability (real time)",
  .update_interval_sec = 900, .run = run,
  .category = "government", .type = "api",
  .url = "https://opendata.paris.fr/api/explore/v2.1/catalog/datasets/velib-disponibilite-en-temps-reel/records?limit=100&offset=0&order_by=stationcode",
  .description = "Live Velib bike-share telemetry for Paris stations: capacity, docks and bikes available, per-station coordinates and reading timestamp",
  .license = "ODbL / Licence Ouverte (Paris open-data terms)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_paris_velib_def)
