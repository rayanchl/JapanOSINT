/* Bluebikes Boston — GBFS stations.
 * Endpoint: https://gbfs.lyft.com/gbfs/1.1/bos/en/station_information.json
 *           https://gbfs.lyft.com/gbfs/1.1/bos/en/station_status.json
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: Public GBFS feed (MobilityData systems.csv), published for reuse.
 * Parse notes: GBFS v1.1 (older schema, still data.stations[]). free_bike_status is present but empty for this system.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-bluebikes-boston", .op = "Bluebikes",
    .info_url = "https://gbfs.lyft.com/gbfs/1.1/bos/en/station_information.json",
    .status_url = "https://gbfs.lyft.com/gbfs/1.1/bos/en/station_status.json",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-bluebikes-boston] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_bluebikes_boston_def = {
  .id = "gbfs-bluebikes-boston", .collector = "transport",
  .name = "Bluebikes Boston — GBFS stations",
  .update_interval_sec = 900, .run = run,
  .category = "transport", .type = "api",
  .url = "https://gbfs.lyft.com/gbfs/1.1/bos/en/station_information.json",
  .description = "Bluebikes docks across metro Boston, including stations sited at MBTA rail stops — first/last-mile ground-access analysis.",
  .license = "Public GBFS feed (MobilityData systems.csv), published for reuse.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_bluebikes_boston_def)
