/* Capital Bikeshare Washington DC — GBFS stations and free bikes.
 * Endpoint: https://gbfs.lyft.com/gbfs/2.3/dca-cabi/en/station_information.json
 *           https://gbfs.lyft.com/gbfs/2.3/dca-cabi/en/station_status.json
 *           https://gbfs.lyft.com/gbfs/2.3/dca-cabi/en/free_bike_status.json
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Plus one row per free-floating vehicle with its live GPS position, type and
 * remaining range. Keyless.
 * Licence: Public GBFS feed (MobilityData systems.csv), published for reuse.
 * Parse notes: GBFS v2.3 data.stations[] / data.bikes[].
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-capital-bikeshare-dc", .op = "Capital Bikeshare",
    .info_url = "https://gbfs.lyft.com/gbfs/2.3/dca-cabi/en/station_information.json",
    .status_url = "https://gbfs.lyft.com/gbfs/2.3/dca-cabi/en/station_status.json",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  /* Free-floating vehicles are capped per run: these feeds run to
   * thousands of vehicles and refresh every few minutes. */
  int v = trn_gbfs_vehicles(ctx, sink, "gbfs-capital-bikeshare-dc", "Capital Bikeshare",
                            "https://gbfs.lyft.com/gbfs/2.3/dca-cabi/en/free_bike_status.json", NULL, 4000);
  if (v > 0) n += v;
  fprintf(stderr, "[gbfs-capital-bikeshare-dc] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_capital_bikeshare_dc_def = {
  .id = "gbfs-capital-bikeshare-dc", .collector = "transport",
  .name = "Capital Bikeshare Washington DC — GBFS stations and free bikes",
  .update_interval_sec = 900, .run = run,
  .category = "transport", .type = "api",
  .url = "https://gbfs.lyft.com/gbfs/2.3/dca-cabi/en/station_information.json",
  .description = "Capital Bikeshare docks across DC, Arlington, Alexandria and Montgomery County plus the live dockless e-bike fleet.",
  .license = "Public GBFS feed (MobilityData systems.csv), published for reuse.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_capital_bikeshare_dc_def)
