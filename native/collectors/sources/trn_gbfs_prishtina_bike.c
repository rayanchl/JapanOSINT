/* Prishtina Bike, Kosovo (nextbike) — GBFS stations.
 * Discovery: https://gbfs.nextbike.net/maps/gbfs/v2/nextbike_gs/gbfs.json
 * Emits: one intel row per dock (station_id, name, short_name, region_id, live
 * counts when station_status is published) on the dock's own lat/lon. Keyless.
 * Licence: nextbike GmbH public GBFS feed; listed in MobilityData systems.csv.
 *
 * Parse notes: the language segment for this system is /sr/, so the feed URLs
 * come from the discovery document rather than being hardcoded. The network is
 * genuinely tiny (10 docks) — that is the true size, not a truncated page — and
 * it is one of the very few open mobility feeds that exist for Kosovo.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

#define PRISHTINA_GBFS "https://gbfs.nextbike.net/maps/gbfs/v2/nextbike_gs/gbfs.json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  char info[512], status[512];
  if (!trn_gbfs_discover(ctx, PRISHTINA_GBFS, "station_information",
                         info, sizeof info, NULL)) {
    fprintf(stderr, "[gbfs-prishtina-bike] discovery failed\n");
    return -1;
  }
  int have_status = trn_gbfs_discover(ctx, PRISHTINA_GBFS, "station_status",
                                      status, sizeof status, NULL);
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-prishtina-bike", .op = "Prishtina Bike",
    .info_url = info,
    .status_url = have_status ? status : NULL,
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;
  fprintf(stderr, "[gbfs-prishtina-bike] emitted %d\n", n);
  return 0;
}

static const source_def trn_gbfs_prishtina_bike_def = {
  .id = "gbfs-prishtina-bike", .collector = "transport",
  .name = "Prishtina Bike (Kosovo) — GBFS stations",
  .update_interval_sec = 1800, .run = run,
  .category = "transport", .type = "api",
  .url = PRISHTINA_GBFS,
  .description = "The full Prishtina city-bike dock network — small, but one of the very few open mobility feeds published for Kosovo.",
  .license = "nextbike GmbH public GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_prishtina_bike_def)
