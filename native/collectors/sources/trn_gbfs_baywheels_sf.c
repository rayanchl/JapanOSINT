/* Bay Wheels San Francisco — GBFS stations and free bikes.
 * Endpoint: https://gbfs.lyft.com/gbfs/2.3/bay/en/station_information.json
 *           https://gbfs.lyft.com/gbfs/2.3/bay/en/station_status.json
 *           https://gbfs.lyft.com/gbfs/2.3/bay/en/free_bike_status.json
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Plus one row per free-floating vehicle with its live GPS position, type and
 * remaining range. Keyless.
 * Licence: Public GBFS feed (MobilityData systems.csv), published for reuse.
 * Parse notes: GBFS v2.3. free_bike_status carries bike_id/lat/lon/current_range_meters/vehicle_type_id.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-baywheels-sf", .op = "Bay Wheels",
    .info_url = "https://gbfs.lyft.com/gbfs/2.3/bay/en/station_information.json",
    .status_url = "https://gbfs.lyft.com/gbfs/2.3/bay/en/station_status.json",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  /* Free-floating vehicles are capped per run: these feeds run to
   * thousands of vehicles and refresh every few minutes. */
  int v = trn_gbfs_vehicles(ctx, sink, "gbfs-baywheels-sf", "Bay Wheels",
                            "https://gbfs.lyft.com/gbfs/2.3/bay/en/free_bike_status.json", NULL, 4000);
  if (v > 0) n += v;
  fprintf(stderr, "[gbfs-baywheels-sf] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_baywheels_sf_def = {
  .id = "gbfs-baywheels-sf", .collector = "transport",
  .name = "Bay Wheels San Francisco — GBFS stations and free bikes",
  .update_interval_sec = 900, .run = run,
  .category = "transport", .type = "api",
  .url = "https://gbfs.lyft.com/gbfs/2.3/bay/en/station_information.json",
  .description = "Bay Wheels docks across San Francisco, Oakland and San Jose plus the dockless e-bike fleet with live positions and remaining range.",
  .license = "Public GBFS feed (MobilityData systems.csv), published for reuse.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_baywheels_sf_def)
