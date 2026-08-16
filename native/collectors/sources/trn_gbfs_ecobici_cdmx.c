/* Ecobici Mexico City — GBFS stations.
 * Endpoint: https://gbfs.mex.lyftbikes.com/gbfs/en/station_information.json
 *           https://gbfs.mex.lyftbikes.com/gbfs/en/station_status.json
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: Gobierno CDMX / Lyft Urban Solutions open GBFS feed; listed in MobilityData systems.csv.
 * Parse notes: GBFS v1.0 schema. free_bike_status is present but empty for this system.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-ecobici-cdmx", .op = "Ecobici CDMX",
    .info_url = "https://gbfs.mex.lyftbikes.com/gbfs/en/station_information.json",
    .status_url = "https://gbfs.mex.lyftbikes.com/gbfs/en/station_status.json",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-ecobici-cdmx] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_ecobici_cdmx_def = {
  .id = "gbfs-ecobici-cdmx", .collector = "transport",
  .name = "Ecobici Mexico City — GBFS stations",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://gbfs.mex.lyftbikes.com/gbfs/en/station_information.json",
  .description = "Ecobici docks in Mexico City with live availability.",
  .license = "Gobierno CDMX / Lyft Urban Solutions open GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_ecobici_cdmx_def)
