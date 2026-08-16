/* BicikeLJ Ljubljana — GBFS v3 via Cyclocity.
 * Endpoint: https://api.cyclocity.fr/contracts/ljubljana/gbfs/v3/station_information.json
 *           https://api.cyclocity.fr/contracts/ljubljana/gbfs/v3/station_status.json
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: JCDecaux Cyclocity open GBFS feed; listed in MobilityData systems.csv.
 * Parse notes: Same shape as Dublinbikes; names carry Slovene diacritics — UTF-8 is passed through untouched.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-bicikelj-ljubljana", .op = "BicikeLJ",
    .info_url = "https://api.cyclocity.fr/contracts/ljubljana/gbfs/v3/station_information.json",
    .status_url = "https://api.cyclocity.fr/contracts/ljubljana/gbfs/v3/station_status.json",
    .hdrs = NULL,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-bicikelj-ljubljana] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_bicikelj_ljubljana_def = {
  .id = "gbfs-bicikelj-ljubljana", .collector = "transport",
  .name = "BicikeLJ Ljubljana — GBFS v3 via Cyclocity",
  .update_interval_sec = 600, .run = run,
  .category = "transport", .type = "api",
  .url = "https://api.cyclocity.fr/contracts/ljubljana/gbfs/v3/station_information.json",
  .description = "BicikeLJ docks in Ljubljana with live status.",
  .license = "JCDecaux Cyclocity open GBFS feed; listed in MobilityData systems.csv.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_bicikelj_ljubljana_def)
