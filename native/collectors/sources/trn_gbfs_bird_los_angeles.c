/* Bird Los Angeles — GBFS free vehicle status.
 * Endpoint: https://mds.bird.co/gbfs/v2/public/los-angeles/free_bike_status.json
 * Emits: one intel row per live scooter — vehicle id, live lat/lon, vehicle
 * type id, reserved/disabled flags, current_fuel_percent and the upstream
 * last_reported timestamp. Keyless.
 * Licence: Bird public GBFS feed published under LADOT's open-data permit
 * condition; listed in MobilityData systems.csv.
 *
 * Parse notes: station_information.json for this system returns 0 rows (pure
 * dockless), so only the vehicle feed is read. Positions come back with
 * repeating digits at ~1e-6 precision, i.e. vehicle-grade GPS rather than
 * survey-grade — every row is tagged geo_precision="vehicle-gps". The list runs
 * to 8,000+ scooters and refreshes every few minutes, so it is capped per run.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = trn_gbfs_vehicles(ctx, sink, "gbfs-bird-los-angeles", "Bird LA",
      "https://mds.bird.co/gbfs/v2/public/los-angeles/free_bike_status.json",
      NULL, 4000);
  if (n < 0) return -1;                      /* the fetch itself failed */
  fprintf(stderr, "[gbfs-bird-los-angeles] emitted %d\n", n);
  return 0;                                  /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_bird_los_angeles_def = {
  .id = "gbfs-bird-los-angeles", .collector = "transport",
  .name = "Bird Los Angeles — GBFS free vehicle status",
  .update_interval_sec = 300, .run = run,
  .category = "transport", .type = "api",
  .url = "https://mds.bird.co/gbfs/v2/public/los-angeles/free_bike_status.json",
  .description = "Live Bird scooter positions across Los Angeles — the largest single free-floating vehicle feed in this fleet.",
  .license = "Bird public GBFS feed published under LADOT's open-data permit condition; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_bird_los_angeles_def)
