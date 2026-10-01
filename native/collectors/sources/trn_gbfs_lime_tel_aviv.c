/* Lime Tel Aviv — GBFS free vehicle status.
 * Endpoint: https://data.lime.bike/api/partners/v2/gbfs/tel_aviv/free_bike_status
 * Emits: one intel row per live vehicle — vehicle id, live lat/lon, vehicle
 * type, reserved/disabled flags and remaining battery range in metres. Keyless.
 * Licence: Lime public partner GBFS feed; listed in MobilityData systems.csv.
 *
 * Parse notes: the URL has NO .json suffix. station_information for this system
 * is a single degenerate row covering the whole city, so it is deliberately NOT
 * used — emitting it would put one point somewhere in Tel Aviv that stands for
 * 5,000 vehicles (R2). The vehicle list is capped per run.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = trn_gbfs_vehicles(ctx, sink, "gbfs-lime-tel-aviv", "Lime Tel Aviv",
      "https://data.lime.bike/api/partners/v2/gbfs/tel_aviv/free_bike_status",
      NULL, 4000);
  if (n < 0) return -1;                      /* the fetch itself failed */
  fprintf(stderr, "[gbfs-lime-tel-aviv] emitted %d\n", n);
  return 0;                                  /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_lime_tel_aviv_def = {
  .id = "gbfs-lime-tel-aviv", .collector = "transport",
  .name = "Lime Tel Aviv — GBFS free vehicle status",
  .update_interval_sec = 300, .run = run,
  .category = "transport", .type = "api",
  .url = "https://data.lime.bike/api/partners/v2/gbfs/tel_aviv/free_bike_status",
  .description = "Live Lime e-scooters and e-bikes across Tel Aviv with per-vehicle position and remaining battery range.",
  .license = "Lime public partner GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_lime_tel_aviv_def)
