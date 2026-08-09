/* Beryl Greater Manchester — GBFS bays and bikes.
 * Endpoint: https://beryl-gbfs-production.web.app/v2_2/Greater_Manchester/station_information.json
 *           https://beryl-gbfs-production.web.app/v2_2/Greater_Manchester/station_status.json
 *           https://beryl-gbfs-production.web.app/v2_2/Greater_Manchester/free_bike_status.json
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Plus one row per free-floating vehicle with its live GPS position, type and
 * remaining range. Keyless.
 * Licence: Beryl public GBFS feed; listed in MobilityData systems.csv.
 * Parse notes: GBFS v2.2, string names; free_bike_status is non-empty on this scheme.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-beryl-manchester", .op = "Beryl",
    .info_url = "https://beryl-gbfs-production.web.app/v2_2/Greater_Manchester/station_information.json",
    .status_url = "https://beryl-gbfs-production.web.app/v2_2/Greater_Manchester/station_status.json",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  /* Free-floating vehicles are capped per run: these feeds run to
   * thousands of vehicles and refresh every few minutes. */
  int v = trn_gbfs_vehicles(ctx, sink, "gbfs-beryl-manchester", "Beryl",
                            "https://beryl-gbfs-production.web.app/v2_2/Greater_Manchester/free_bike_status.json", NULL, 4000);
  if (v > 0) n += v;
  fprintf(stderr, "[gbfs-beryl-manchester] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_beryl_manchester_def = {
  .id = "gbfs-beryl-manchester", .collector = "transport",
  .name = "Beryl Greater Manchester — GBFS bays and bikes",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://beryl-gbfs-production.web.app/v2_2/Greater_Manchester/station_information.json",
  .description = "Beryl bays across Greater Manchester plus the free-floating fleet. Beryl publishes ~25 UK schemes on the identical path shape.",
  .license = "Beryl public GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_beryl_manchester_def)
