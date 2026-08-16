/* Mobi Bike Share Vancouver — GBFS stations.
 * Endpoint: https://gbfs.kappa.fifteen.eu/gbfs/2.2/mobi/en/station_information.json
 *           https://gbfs.kappa.fifteen.eu/gbfs/2.2/mobi/en/station_status.json
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: Mobi by Rogers / Fifteen open GBFS feed; listed in MobilityData systems.csv.
 * Parse notes: GBFS v2.2, string names. rental_uris contain internal todo:// scheme URIs and are deliberately ignored.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-mobi-vancouver", .op = "Mobi",
    .info_url = "https://gbfs.kappa.fifteen.eu/gbfs/2.2/mobi/en/station_information.json",
    .status_url = "https://gbfs.kappa.fifteen.eu/gbfs/2.2/mobi/en/station_status.json",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-mobi-vancouver] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_mobi_vancouver_def = {
  .id = "gbfs-mobi-vancouver", .collector = "transport",
  .name = "Mobi Bike Share Vancouver — GBFS stations",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://gbfs.kappa.fifteen.eu/gbfs/2.2/mobi/en/station_information.json",
  .description = "Mobi docks in Vancouver with per-vehicle-type capacity and charging-station flags.",
  .license = "Mobi by Rogers / Fifteen open GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_mobi_vancouver_def)
