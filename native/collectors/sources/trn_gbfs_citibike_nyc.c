/* Citi Bike New York — GBFS stations.
 * Endpoint: https://gbfs.lyft.com/gbfs/2.3/bkn/en/station_information.json
 *           https://gbfs.lyft.com/gbfs/2.3/bkn/en/station_status.json
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: Lyft/NYC Citi Bike GBFS data licence — public GBFS feed listed in the MobilityData systems catalog, published for reuse.
 * Parse notes: Discovery gbfs.citibikenyc.com redirects the feed URLs to gbfs.lyft.com. GBFS v2.3 data.stations[]; free_bike_status is empty for this dock-only system.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-citibike-nyc", .op = "Citi Bike",
    .info_url = "https://gbfs.lyft.com/gbfs/2.3/bkn/en/station_information.json",
    .status_url = "https://gbfs.lyft.com/gbfs/2.3/bkn/en/station_status.json",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-citibike-nyc] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_citibike_nyc_def = {
  .id = "gbfs-citibike-nyc", .collector = "transport",
  .name = "Citi Bike New York — GBFS stations",
  .update_interval_sec = 900, .run = run,
  .category = "transport", .type = "api",
  .url = "https://gbfs.lyft.com/gbfs/2.3/bkn/en/station_information.json",
  .description = "Every Citi Bike dock in New York City and Jersey City with live bike/dock counts.",
  .license = "Lyft/NYC Citi Bike GBFS data licence — public GBFS feed listed in the MobilityData systems catalog, published for reuse.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_citibike_nyc_def)
