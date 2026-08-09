/* Paris Open Data - Velib bike-share station availability (real time).
 * Endpoint: https://opendata.paris.fr/api/explore/v2.1/catalog/datasets/
 *           velib-disponibilite-en-temps-reel/records?limit=100
 * Row tier (results[]), 1,518 stations. Emits, per station, every scalar the
 * API returned: stationcode, name, capacity, numdocksavailable,
 * numbikesavailable, ebike, mechanical, is_installed/is_renting/is_returning,
 * nom_arrondissement_communes and the upstream duedate, which is used as
 * published_at (the freshness stamp of the reading).
 * R2: coordinates come only from the upstream coordonnees_geo{lon,lat}.
 * Keyless. Licence: ODbL / Licence Ouverte; Paris open-data terms permit
 * reuse with attribution. */
#include "od_shared.c"

#define SID "ods-paris-velib"
static const char *URL =
  "https://opendata.paris.fr/api/explore/v2.1/catalog/datasets/"
  "velib-disponibilite-en-temps-reel/records?limit=100";
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
  return od_rc(SID, od_fetch_rows(ctx, sink, URL, "results", &sp));
}

static const source_def od_ods_paris_velib_def = {
  .id = SID, .collector = "government",
  .name = "Paris Open Data - Velib station availability (real time)",
  .update_interval_sec = 900, .run = run,
  .category = "government", .type = "api",
  .url = "https://opendata.paris.fr/api/explore/v2.1/catalog/datasets/velib-disponibilite-en-temps-reel/records?limit=100",
  .description = "Live Velib bike-share telemetry for Paris stations: capacity, docks and bikes available, per-station coordinates and reading timestamp",
  .license = "ODbL / Licence Ouverte (Paris open-data terms)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_paris_velib_def)
