/* Divvy Chicago — GBFS stations and free bikes.
 * Endpoint: https://gbfs.lyft.com/gbfs/2.3/chi/en/station_information.json
 *           https://gbfs.lyft.com/gbfs/2.3/chi/en/station_status.json
 *           https://gbfs.lyft.com/gbfs/2.3/chi/en/free_bike_status.json
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Plus one row per free-floating vehicle with its live GPS position, type and
 * remaining range. Keyless.
 * Licence: Public GBFS feed (MobilityData systems.csv), published for reuse.
 * Parse notes: Discovery gbfs.divvybikes.com/gbfs/2.3/gbfs.json redirects the
 * feed URLs to the gbfs.lyft.com/gbfs/2.3/chi/en/ prefix used here.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-divvy-chicago", .op = "Divvy",
    .info_url = "https://gbfs.lyft.com/gbfs/2.3/chi/en/station_information.json",
    .status_url = "https://gbfs.lyft.com/gbfs/2.3/chi/en/station_status.json",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  /* Free-floating vehicles are capped per run: these feeds run to
   * thousands of vehicles and refresh every few minutes. */
  int v = trn_gbfs_vehicles(ctx, sink, "gbfs-divvy-chicago", "Divvy",
                            "https://gbfs.lyft.com/gbfs/2.3/chi/en/free_bike_status.json", NULL, 4000);
  if (v > 0) n += v;
  fprintf(stderr, "[gbfs-divvy-chicago] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_divvy_chicago_def = {
  .id = "gbfs-divvy-chicago", .collector = "transport",
  .name = "Divvy Chicago — GBFS stations and free bikes",
  .update_interval_sec = 900, .run = run,
  .category = "transport", .type = "api",
  .url = "https://gbfs.lyft.com/gbfs/2.3/chi/en/station_information.json",
  .description = "Divvy docks in Chicago plus the free-floating e-bike fleet with live positions.",
  .license = "Public GBFS feed (MobilityData systems.csv), published for reuse.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_divvy_chicago_def)
