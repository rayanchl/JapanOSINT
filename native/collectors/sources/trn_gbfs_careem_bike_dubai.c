/* Careem BIKE Dubai — GBFS v3 stations.
 * Endpoint: https://careem.publicbikesystem.net/customer/gbfs/v3.0/station_information
 *           https://careem.publicbikesystem.net/customer/gbfs/v3.0/station_status
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: Careem / PBSC open GBFS feed; listed in MobilityData systems.csv.
 * Parse notes: GBFS v3.0 localised name array; vehicle_docks_capacity enumerates scooter types alongside bikes.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-careem-bike-dubai", .op = "Careem BIKE",
    .info_url = "https://careem.publicbikesystem.net/customer/gbfs/v3.0/station_information",
    .status_url = "https://careem.publicbikesystem.net/customer/gbfs/v3.0/station_status",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-careem-bike-dubai] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_careem_bike_dubai_def = {
  .id = "gbfs-careem-bike-dubai", .collector = "transport",
  .name = "Careem BIKE Dubai — GBFS v3 stations",
  .update_interval_sec = 900, .run = run,
  .category = "transport", .type = "api",
  .url = "https://careem.publicbikesystem.net/customer/gbfs/v3.0/station_information",
  .description = "Careem BIKE docks across Dubai and Abu Dhabi — the only open, keyless micromobility feed found for the Gulf.",
  .license = "Careem / PBSC open GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_careem_bike_dubai_def)
