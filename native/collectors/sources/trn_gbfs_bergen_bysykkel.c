/* Bergen Bysykkel — GBFS stations and status.
 * Endpoint: https://gbfs.urbansharing.com/bergenbysykkel.no/station_information.json
 *           https://gbfs.urbansharing.com/bergenbysykkel.no/station_status.json
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: Bergen Bysykkel / Urban Sharing open GBFS feed; listed in MobilityData systems.csv.
 * Parse notes: Urban Sharing shape: plain string name (not localised), address + cross_street, MultiPolygon station_area.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-bergen-bysykkel", .op = "Bergen Bysykkel",
    .info_url = "https://gbfs.urbansharing.com/bergenbysykkel.no/station_information.json",
    .status_url = "https://gbfs.urbansharing.com/bergenbysykkel.no/station_status.json",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-bergen-bysykkel] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_bergen_bysykkel_def = {
  .id = "gbfs-bergen-bysykkel", .collector = "transport",
  .name = "Bergen Bysykkel — GBFS stations and status",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://gbfs.urbansharing.com/bergenbysykkel.no/station_information.json",
  .description = "Bergen city-bike docks with num_bikes_available and num_docks_available per dock.",
  .license = "Bergen Bysykkel / Urban Sharing open GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_bergen_bysykkel_def)
