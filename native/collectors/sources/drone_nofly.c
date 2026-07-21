/* collectors/safety/sources/drone_nofly.c
 * Port of server/src/collectors/droneNofly.js. tryMlitDrone() only probes
 * MLIT_DRONE_URL and ALWAYS returns null (the page is an HTML index, not a
 * dataset); collect() then does `if(!live) features=[]`. SEED_NOFLY_ZONES/
 * generateSeedData is NOT in the live path (RULE 8 → drop). Faithful port:
 * reachability fetch (parity with Node hitting the host), emit 0 features. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdlib.h>

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *t = feed_get_text(ctx->http,
                          "https://www.mlit.go.jp/koku/koku_fr10_000041.html",
                          10000);
  free(t);                                    /* result discarded, as in JS */
  cJSON *empty = cJSON_CreateArray();
  int n = geojson_emit_features(sink, "drone-nofly", empty);
  cJSON_Delete(empty);
  return n >= 0 ? 0 : -1;
}

static const source_def drone_nofly_def = {
  .id = "drone-nofly", .collector = "safety",
  .name = "Drone No-Fly Zones", .name_ja = "ドローン飛行禁止区域",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(drone_nofly_def)
