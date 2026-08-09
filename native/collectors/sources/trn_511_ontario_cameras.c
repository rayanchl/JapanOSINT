/* Ontario 511 — provincial traffic camera index.
 * Endpoint: https://511on.ca/api/v2/get/cameras
 * Emits: one intel row per MTO highway camera — camera Id, Source (e.g. RWIS),
 * SourceId, Roadway, Direction and Location, plus every published view with its
 * image URL, status and description, pinned on the camera's own coordinates.
 * Keyless.
 * Licence: Ontario Ministry of Transportation 511 open API — Open Government
 * Licence – Ontario.
 *
 * Parse notes: top-level JSON array. Latitude/Longitude are numbers with
 * excessive precision. Views[] is per direction; a camera whose views are all
 * Status != "Enabled" has no live image, so the enabled count is emitted
 * explicitly rather than every camera being presented as live.
 */
#include "../../lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = trn_511_cameras(ctx, sink, "511-ontario-cameras",
      "https://511on.ca/api/v2/get/cameras", "Ontario MTO (511)",
      "[\"transport\",\"camera\",\"ontario\",\"road\"]");
  if (n < 0) return -1;                     /* the fetch itself failed */
  fprintf(stderr, "[511-ontario-cameras] emitted %d\n", n);
  return 0;                                 /* parsed fine → 0 (R3) */
}

static const source_def trn_511_ontario_cameras_def = {
  .id = "511-ontario-cameras", .collector = "transport",
  .name = "Ontario 511 — provincial traffic camera index",
  .update_interval_sec = 86400, .run = run,
  .category = "transport", .type = "api",
  .url = "https://511on.ca/api/v2/get/cameras",
  .description = "Every Ontario MTO highway camera with exact coordinates, roadway, direction and one image URL per view — a ~1,000-camera index for the 400-series and QEW corridors.",
  .license = "Open Government Licence – Ontario (MTO 511 open API).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_511_ontario_cameras_def)
