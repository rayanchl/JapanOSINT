/* Ecobici Buenos Aires — GBFS v3 stations and status.
 * Endpoint: https://buenosaires.publicbikesystem.net/customer/gbfs/v3.0/station_information
 *           https://buenosaires.publicbikesystem.net/customer/gbfs/v3.0/station_status
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: Ciudad de Buenos Aires open data / PBSC GBFS feed; listed in MobilityData systems.csv.
 * Parse notes: GBFS v3.0 localised name array (en/nl/pt/es).
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-ecobici-buenos-aires", .op = "Ecobici Buenos Aires",
    .info_url = "https://buenosaires.publicbikesystem.net/customer/gbfs/v3.0/station_information",
    .status_url = "https://buenosaires.publicbikesystem.net/customer/gbfs/v3.0/station_status",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-ecobici-buenos-aires] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_ecobici_buenos_aires_def = {
  .id = "gbfs-ecobici-buenos-aires", .collector = "transport",
  .name = "Ecobici Buenos Aires — GBFS v3 stations and status",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://buenosaires.publicbikesystem.net/customer/gbfs/v3.0/station_information",
  .description = "Ecobici docks across Buenos Aires — the largest free-to-use municipal bike-share in South America.",
  .license = "Ciudad de Buenos Aires open data / PBSC GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_ecobici_buenos_aires_def)
