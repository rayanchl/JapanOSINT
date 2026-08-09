/* Veturilo Warsaw (nextbike) — GBFS stations and per-bike positions.
 * Discovery: https://gbfs.nextbike.net/maps/gbfs/v2/nextbike_vw/gbfs.json
 * Emits: one intel row per dock (station_id, name, short_name, capacity, live
 * vehicles/docks available) on the dock's own lat/lon, plus one row per bike
 * with its individual live position and type — nextbike publishes per-bike
 * lat/lon, which is unusually granular for a docked system. Keyless.
 * Licence: nextbike GmbH public GBFS feed; listed in MobilityData systems.csv.
 *
 * Parse note (trap): the nextbike feed URLs carry a LANGUAGE segment that
 * differs per system (/pl/ here, /sv/ in Gothenburg, /sr/ in Prishtina), so the
 * URLs are read out of the discovery document instead of being hardcoded.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

#define VETURILO_GBFS "https://gbfs.nextbike.net/maps/gbfs/v2/nextbike_vw/gbfs.json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  char info[512], status[512], bikes[512];
  if (!trn_gbfs_discover(ctx, VETURILO_GBFS, "station_information",
                         info, sizeof info, NULL)) {
    fprintf(stderr, "[gbfs-veturilo-warsaw] discovery failed\n");
    return -1;
  }
  int have_status = trn_gbfs_discover(ctx, VETURILO_GBFS, "station_status",
                                      status, sizeof status, NULL);
  int have_bikes  = trn_gbfs_discover(ctx, VETURILO_GBFS, "free_bike_status",
                                      bikes, sizeof bikes, NULL);

  trn_gbfs_cfg cfg = {
    .sid = "gbfs-veturilo-warsaw", .op = "Veturilo",
    .info_url = info,
    .status_url = have_status ? status : NULL,
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                     /* the fetch itself failed */
  if (have_bikes) {
    int v = trn_gbfs_vehicles(ctx, sink, "gbfs-veturilo-warsaw", "Veturilo",
                              bikes, NULL, 4000);
    if (v > 0) n += v;
  }
  fprintf(stderr, "[gbfs-veturilo-warsaw] emitted %d\n", n);
  return 0;                                 /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_veturilo_warsaw_def = {
  .id = "gbfs-veturilo-warsaw", .collector = "transport",
  .name = "Veturilo Warsaw — GBFS stations and free bikes (nextbike)",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = VETURILO_GBFS,
  .description = "Veturilo docks in Warsaw plus thousands of individually-positioned bikes — nextbike publishes per-bike lat/lon.",
  .license = "nextbike GmbH public GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_veturilo_warsaw_def)
