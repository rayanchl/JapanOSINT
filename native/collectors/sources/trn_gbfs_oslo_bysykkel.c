/* Oslo Bysykkel — GBFS v3 via Entur.
 * Endpoint: https://api.entur.io/mobility/v2/gbfs/v3/oslobysykkel/station_information
 *           https://api.entur.io/mobility/v2/gbfs/v3/oslobysykkel/station_status
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: Entur open mobility API, NLOD / CC-BY. Entur asks for an ET-Client-Name header identifying the client.
 * Parse notes: GBFS v3.0, localised name array, no .json suffix on feed URLs. station_status answered HTTP 429 during discovery — Entur rate-limits, hence the identifying header and the 15-minute interval; a 429 simply yields no live counts, not an error.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

/* Entur asks every client to identify itself with ET-Client-Name and
 * rate-limits (HTTP 429) callers that do not. */
static const char *const TRN_ENTUR_HDRS[] = { "ET-Client-Name: japanosint-native", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-oslo-bysykkel", .op = "Oslo Bysykkel",
    .info_url = "https://api.entur.io/mobility/v2/gbfs/v3/oslobysykkel/station_information",
    .status_url = "https://api.entur.io/mobility/v2/gbfs/v3/oslobysykkel/station_status",
    .hdrs = TRN_ENTUR_HDRS,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-oslo-bysykkel] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_oslo_bysykkel_def = {
  .id = "gbfs-oslo-bysykkel", .collector = "transport",
  .name = "Oslo Bysykkel — GBFS v3 via Entur",
  .update_interval_sec = 900, .run = run,
  .category = "transport", .type = "api",
  .url = "https://api.entur.io/mobility/v2/gbfs/v3/oslobysykkel/station_information",
  .description = "Oslo city-bike docks served through Entur, Norway's national mobility data hub.",
  .license = "Entur open mobility API, NLOD / CC-BY. Entur asks for an ET-Client-Name header identifying the client.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_oslo_bysykkel_def)
