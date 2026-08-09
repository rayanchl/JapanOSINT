/* BikeMi Milan — GBFS stations and status.
 * Endpoint: https://gbfs.urbansharing.com/bikemi.com/station_information.json
 *           https://gbfs.urbansharing.com/bikemi.com/station_status.json
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: Comune di Milano / Urban Sharing open GBFS feed; listed in MobilityData systems.csv.
 * Parse notes: Same Urban Sharing shape as Bergen: string names, MultiPolygon station_area.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-bikemi-milan", .op = "BikeMi",
    .info_url = "https://gbfs.urbansharing.com/bikemi.com/station_information.json",
    .status_url = "https://gbfs.urbansharing.com/bikemi.com/station_status.json",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-bikemi-milan] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_bikemi_milan_def = {
  .id = "gbfs-bikemi-milan", .collector = "transport",
  .name = "BikeMi Milan — GBFS stations and status",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://gbfs.urbansharing.com/bikemi.com/station_information.json",
  .description = "BikeMi docks in Milan with live counts from the joined station_status document.",
  .license = "Comune di Milano / Urban Sharing open GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_bikemi_milan_def)
