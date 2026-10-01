/* Styr & Ställ Göteborg (nextbike) — GBFS stations and per-bike positions.
 * Discovery: https://gbfs.nextbike.net/maps/gbfs/v2/nextbike_zg/gbfs.json
 * Emits: one intel row per dock (station_id, name, short_name, region, live
 * counts) on the dock's own lat/lon, plus one row per bike with its individual
 * live position. Keyless.
 * Licence: nextbike GmbH / Göteborgs Stad public GBFS feed; listed in
 * MobilityData systems.csv.
 *
 * Parse note (trap): the language segment for this system is /sv/, not the /pl/
 * of the Warsaw system on the same host — the feed URLs are therefore read from
 * the discovery document rather than hardcoded.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

#define STYR_GBFS "https://gbfs.nextbike.net/maps/gbfs/v2/nextbike_zg/gbfs.json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  char info[512], status[512], bikes[512];
  if (!trn_gbfs_discover(ctx, STYR_GBFS, "station_information",
                         info, sizeof info, NULL)) {
    fprintf(stderr, "[gbfs-styr-och-stall-goteborg] discovery failed\n");
    return -1;
  }
  int have_status = trn_gbfs_discover(ctx, STYR_GBFS, "station_status",
                                      status, sizeof status, NULL);
  int have_bikes  = trn_gbfs_discover(ctx, STYR_GBFS, "free_bike_status",
                                      bikes, sizeof bikes, NULL);

  trn_gbfs_cfg cfg = {
    .sid = "gbfs-styr-och-stall-goteborg", .op = "Styr och Stall",
    .info_url = info,
    .status_url = have_status ? status : NULL,
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;
  if (have_bikes) {
    int v = trn_gbfs_vehicles(ctx, sink, "gbfs-styr-och-stall-goteborg",
                              "Styr och Stall", bikes, NULL, 4000);
    if (v > 0) n += v;
  }
  fprintf(stderr, "[gbfs-styr-och-stall-goteborg] emitted %d\n", n);
  return 0;
}

static const source_def trn_gbfs_styr_och_stall_def = {
  .id = "gbfs-styr-och-stall-goteborg", .collector = "transport",
  .name = "Styr & Ställ Göteborg — GBFS stations and free bikes (nextbike)",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = STYR_GBFS,
  .description = "Styr & Ställ docks in Gothenburg plus every bike with an individual live position.",
  .license = "nextbike GmbH / Göteborgs Stad public GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_styr_och_stall_def)
