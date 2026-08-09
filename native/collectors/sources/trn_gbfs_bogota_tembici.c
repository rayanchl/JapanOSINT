/* Tembici Bogotá — GBFS v3 stations.
 * Endpoint: https://bogota.publicbikesystem.net/customer/gbfs/v3.0/station_information
 *           https://bogota.publicbikesystem.net/customer/gbfs/v3.0/station_status
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: Tembici / PBSC open GBFS feed; listed in MobilityData systems.csv.
 * Parse notes: GBFS v3.0 localised name array.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-bogota-tembici", .op = "Tembici Bogota",
    .info_url = "https://bogota.publicbikesystem.net/customer/gbfs/v3.0/station_information",
    .status_url = "https://bogota.publicbikesystem.net/customer/gbfs/v3.0/station_status",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-bogota-tembici] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_bogota_tembici_def = {
  .id = "gbfs-bogota-tembici", .collector = "transport",
  .name = "Tembici Bogotá — GBFS v3 stations",
  .update_interval_sec = 900, .run = run,
  .category = "transport", .type = "api",
  .url = "https://bogota.publicbikesystem.net/customer/gbfs/v3.0/station_information",
  .description = "Tembici docks in Bogotá, including adapted-cycle (Handbike) and cargo-bike dock types.",
  .license = "Tembici / PBSC open GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_bogota_tembici_def)
