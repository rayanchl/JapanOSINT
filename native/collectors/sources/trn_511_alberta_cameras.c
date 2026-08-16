/* Alberta 511 — provincial traffic camera index.
 * Endpoint: https://511.alberta.ca/api/v2/get/cameras
 * Emits: one intel row per Alberta highway camera — camera Id, Source,
 * SourceId, Roadway, Direction and Location, plus every published view with its
 * image URL, status and description, pinned on the camera's own coordinates.
 * Keyless.
 * Licence: Government of Alberta 511 open API — Open Government Licence –
 * Alberta.
 *
 * Parse notes: schema is identical to Ontario's 511on.ca, so both share one
 * emitter. Note that the same vendor platform backs several US state 511s but
 * most of those require an API key (511ia.org, for instance, answers the same
 * path with an HTML app shell rather than JSON), so they are not wired up here.
 */
#include "lib/jocore.h"
#include "trn_common.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = trn_511_cameras(ctx, sink, "511-alberta-cameras",
      "https://511.alberta.ca/api/v2/get/cameras", "Government of Alberta (511)",
      "[\"transport\",\"camera\",\"alberta\",\"road\"]");
  if (n < 0) return -1;
  fprintf(stderr, "[511-alberta-cameras] emitted %d\n", n);
  return 0;
}

static const source_def trn_511_alberta_cameras_def = {
  .id = "511-alberta-cameras", .collector = "transport",
  .name = "Alberta 511 — provincial traffic camera index",
  .update_interval_sec = 86400, .run = run,
  .category = "transport", .type = "api",
  .url = "https://511.alberta.ca/api/v2/get/cameras",
  .description = "Alberta highway cameras with coordinates and view URLs, covering the Calgary and Edmonton ring roads and the provincial highway network.",
  .license = "Open Government Licence – Alberta (511 open API).",
  .free_tier = 1,
};
REGISTER_SOURCE(trn_511_alberta_cameras_def)
