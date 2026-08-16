/* BIXI Montréal — GBFS stations and live status.
 * Endpoint: https://gbfs.velobixi.com/gbfs/2-2/en/station_information.json
 *           https://gbfs.velobixi.com/gbfs/2-2/en/station_status.json
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: BIXI Montréal open data — public GBFS feed listed in MobilityData systems.csv.
 * Parse notes: Two-call join on station_id is required to get the live counts; is_charging marks the e-bike charging docks.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-bixi-montreal", .op = "BIXI Montreal",
    .info_url = "https://gbfs.velobixi.com/gbfs/2-2/en/station_information.json",
    .status_url = "https://gbfs.velobixi.com/gbfs/2-2/en/station_status.json",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-bixi-montreal] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_bixi_montreal_def = {
  .id = "gbfs-bixi-montreal", .collector = "transport",
  .name = "BIXI Montréal — GBFS stations and live status",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://gbfs.velobixi.com/gbfs/2-2/en/station_information.json",
  .description = "BIXI docks in Montréal with per-dock bikes/e-bikes/docks available from the joined station_status document.",
  .license = "BIXI Montréal open data — public GBFS feed listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_bixi_montreal_def)
