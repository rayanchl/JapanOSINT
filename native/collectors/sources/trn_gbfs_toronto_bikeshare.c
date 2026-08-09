/* Bike Share Toronto — GBFS v3 stations and status.
 * Endpoint: https://toronto.publicbikesystem.net/customer/gbfs/v3.0/station_information
 *           https://toronto.publicbikesystem.net/customer/gbfs/v3.0/station_status
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: City of Toronto / PBSC open GBFS feed; listed in MobilityData systems.csv.
 * Parse notes: GBFS v3.0 — name is an ARRAY of {text,language} objects, not a string (trn_gbfs_name handles it); feed URLs have no .json suffix; status uses num_vehicles_available, not num_bikes_available.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-toronto-bikeshare", .op = "Bike Share Toronto",
    .info_url = "https://toronto.publicbikesystem.net/customer/gbfs/v3.0/station_information",
    .status_url = "https://toronto.publicbikesystem.net/customer/gbfs/v3.0/station_status",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-toronto-bikeshare] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_toronto_bikeshare_def = {
  .id = "gbfs-toronto-bikeshare", .collector = "transport",
  .name = "Bike Share Toronto — GBFS v3 stations and status",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://toronto.publicbikesystem.net/customer/gbfs/v3.0/station_information",
  .description = "Bike Share Toronto docks with live availability and an ISO8601 last_reported per dock.",
  .license = "City of Toronto / PBSC open GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_toronto_bikeshare_def)
