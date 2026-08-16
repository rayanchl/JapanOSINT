/* Dott Brussels — GBFS live vehicles and mandated virtual parking bays.
 * Endpoints: https://gbfs.api.ridedott.com/public/v2/brussels/free_bike_status.json
 *            https://gbfs.api.ridedott.com/public/v2/brussels/station_information.json
 * Emits: one intel row per live e-scooter/e-bike (vehicle id, live lat/lon,
 * type, reserved/disabled flags, remaining range) and one row per virtual
 * parking bay (bay name, capacity per vehicle type, bay lat/lon). Keyless.
 * Licence: Dott public GBFS feed published to satisfy Brussels' mandated
 * open-data requirement; listed in MobilityData systems.csv.
 *
 * Parse note: station_information here is NOT physical docks — every entry is
 * is_virtual_station:true, a mandated parking bay. trn_gbfs_stations tags those
 * rows geo_precision="parking-zone" so a bay is never read as survey-grade
 * infrastructure. The vehicle feed is capped per run (5,000+ vehicles, 5-minute
 * refresh).
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int v = trn_gbfs_vehicles(ctx, sink, "gbfs-dott-brussels", "Dott Brussels",
      "https://gbfs.api.ridedott.com/public/v2/brussels/free_bike_status.json",
      NULL, 4000);
  if (v < 0) return -1;                      /* the fetch itself failed */

  trn_gbfs_cfg cfg = {
    .sid = "gbfs-dott-brussels", .op = "Dott Brussels",
    .info_url =
      "https://gbfs.api.ridedott.com/public/v2/brussels/station_information.json",
    .status_url = NULL,
    .hdrs = NULL,
  };
  int s = trn_gbfs_stations(ctx, sink, &cfg);
  int n = v + (s > 0 ? s : 0);
  fprintf(stderr, "[gbfs-dott-brussels] emitted %d\n", n);
  return 0;                                  /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_dott_brussels_def = {
  .id = "gbfs-dott-brussels", .collector = "transport",
  .name = "Dott Brussels — GBFS virtual stations and live vehicles",
  .update_interval_sec = 300, .run = run,
  .category = "transport", .type = "api",
  .url = "https://gbfs.api.ridedott.com/public/v2/brussels/free_bike_status.json",
  .description = "Live Dott e-scooters and e-bikes in Brussels plus the mandated virtual parking bays — the densest street-level micromobility picture available for an EU capital without a key.",
  .license = "Dott public GBFS feed published under Brussels' mandated open-data requirement; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_dott_brussels_def)
