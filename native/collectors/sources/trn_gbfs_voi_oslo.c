/* Voi Oslo — GBFS v3 parking zones via Entur.
 * Endpoint: https://api.entur.io/mobility/v2/gbfs/v3/voioslo/station_information
 * Emits: one intel row per dock (station_id, name, address, capacity,
 * live vehicles/docks available, last_reported) pinned on the dock's own
 * lat/lon as published by the operator. No coordinate is ever synthesised.
 * Keyless.
 * Licence: Entur open mobility API (NLOD / CC-BY). Send an ET-Client-Name header.
 * Parse notes: GBFS v3.0. Almost every station is is_virtual_station:true with a MultiPolygon station_area, so each row is tagged geo_precision=parking-zone rather than presented as a physical dock. Entur rate-limits (HTTP 429) clients that do not send ET-Client-Name.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

/* Entur asks every client to identify itself with ET-Client-Name and
 * rate-limits (HTTP 429) callers that do not. */
static const char *const TRN_ENTUR_HDRS[] = { "ET-Client-Name: japanosint-native", NULL };

static int run(const source_ctx *ctx, intel_sink *sink) {
  trn_gbfs_cfg cfg = {
    .sid = "gbfs-voi-oslo", .op = "Voi Oslo",
    .info_url = "https://api.entur.io/mobility/v2/gbfs/v3/voioslo/station_information",
    .status_url = NULL,
    .hdrs = TRN_ENTUR_HDRS,
  };
  int n = trn_gbfs_stations(ctx, sink, &cfg);
  if (n < 0) return -1;                    /* the fetch itself failed */
  fprintf(stderr, "[gbfs-voi-oslo] emitted %d\n", n);
  return 0;                               /* parsed fine → 0 (R3) */
}

static const source_def trn_gbfs_voi_oslo_def = {
  .id = "gbfs-voi-oslo", .collector = "transport",
  .name = "Voi Oslo — GBFS v3 parking zones via Entur",
  .update_interval_sec = 300, .run = run,
  .category = "transport", .type = "api",
  .url = "https://api.entur.io/mobility/v2/gbfs/v3/voioslo/station_information",
  .description = "Voi scooter parking zones in Oslo, republished by Entur through its national GBFS namespace.",
  .license = "Entur open mobility API (NLOD / CC-BY). Send an ET-Client-Name header.",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_gbfs_voi_oslo_def)
