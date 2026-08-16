/* Vélib' Métropole Paris — GBFS stations and status.
 * Endpoint: https://velib-metropole-opendata.smovengo.cloud/opendata/Velib_Metropole/station_information.json
 *           https://velib-metropole-opendata.smovengo.cloud/opendata/Velib_Metropole/station_status.json
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: Smovengo / Vélib' Métropole open data, Licence Ouverte (Etalab) v2.0.
 * Parse notes: station_id is a NUMBER here (213688169), not a string — trn_idstr normalises both forms so the station_status join still matches.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-velib-paris", .op = "Velib Metropole",
    .info_url = "https://velib-metropole-opendata.smovengo.cloud/opendata/Velib_Metropole/station_information.json",
    .status_url = "https://velib-metropole-opendata.smovengo.cloud/opendata/Velib_Metropole/station_status.json",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-velib-paris] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_velib_paris_def = {
  .id = "gbfs-velib-paris", .collector = "transport",
  .name = "Vélib' Métropole Paris — GBFS stations and status",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://velib-metropole-opendata.smovengo.cloud/opendata/Velib_Metropole/station_information.json",
  .description = "Every Vélib' dock across the Paris metropolitan area with live availability.",
  .license = "Smovengo / Vélib' Métropole open data, Licence Ouverte (Etalab) v2.0.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_velib_paris_def)
