/* SNCF Open Data - French passenger rail stations.
 * Endpoint: https://data.sncf.com/api/explore/v2.1/catalog/datasets/
 *           gares-de-voyageurs/records?limit=100
 * Row tier (results[]), 2,782 stations. Emits, per station, every scalar the
 * API returned: nom, libellecourt, segment_drg, codeinsee (commune),
 * codes_uic (joins to European rail reference data), niveau_de_service, id.
 * R2: coordinates come only from the upstream position_geographique{lon,lat}.
 * Keyless. Licence: Licence Ouverte / ODbL depending on the dataset; SNCF
 * open-data terms permit reuse with attribution. */
#include "od_shared.c"

#define SID "ods-sncf-stations"
static const char *URL =
  "https://data.sncf.com/api/explore/v2.1/catalog/datasets/"
  "gares-de-voyageurs/records?limit=100";
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
  return od_rc(SID, od_fetch_rows(ctx, sink, URL, "results", &sp));
}

static const source_def od_ods_sncf_stations_def = {
  .id = SID, .collector = "government",
  .name = "SNCF Open Data - passenger stations",
  .update_interval_sec = 604800, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://data.sncf.com/api/explore/v2.1/catalog/datasets/gares-de-voyageurs/records?limit=100",
  .description = "French passenger rail stations with UIC codes, INSEE commune codes and upstream coordinates",
  .license = "Licence Ouverte / ODbL (SNCF open-data terms)",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ods_sncf_stations_def)
