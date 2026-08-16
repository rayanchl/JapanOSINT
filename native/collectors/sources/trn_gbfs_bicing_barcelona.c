/* Bicing Barcelona — GBFS v3 stations and status.
 * Endpoint: https://barcelona.publicbikesystem.net/customer/gbfs/v3.0/station_information
 *           https://barcelona.publicbikesystem.net/customer/gbfs/v3.0/station_status
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: Ajuntament de Barcelona / PBSC open GBFS feed; listed in MobilityData systems.csv.
 * Parse notes: GBFS v3.0 — localised name array (ca/en/es); station_status at the sibling path.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-bicing-barcelona", .op = "Bicing",
    .info_url = "https://barcelona.publicbikesystem.net/customer/gbfs/v3.0/station_information",
    .status_url = "https://barcelona.publicbikesystem.net/customer/gbfs/v3.0/station_status",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-bicing-barcelona] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_bicing_barcelona_def = {
  .id = "gbfs-bicing-barcelona", .collector = "transport",
  .name = "Bicing Barcelona — GBFS v3 stations and status",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://barcelona.publicbikesystem.net/customer/gbfs/v3.0/station_information",
  .description = "Bicing docks in Barcelona with live vehicle/dock counts and street address.",
  .license = "Ajuntament de Barcelona / PBSC open GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_bicing_barcelona_def)
