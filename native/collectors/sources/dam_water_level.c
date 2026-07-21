/* collectors/infrastructure/sources/dam_water_level.c
 * Port of server/src/collectors/damWaterLevel.js. tryMlitDam() only probes
 * MLIT_DAM_URL for reachability and ALWAYS returns null (no public JSON
 * endpoint; per-dam EUC-JP scraping is policy-discouraged); collect() then
 * does `if(!live) features=[]`. SEED_DAMS/generateSeedData is NOT in the
 * live path (RULE 8 → drop). Faithful port: reachability fetch (parity with
 * Node hitting the host), then emit 0 features. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdlib.h>

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *t = feed_get_text(ctx->http,
                          "http://www1.river.go.jp/cgi-bin/DspDamData.exe",
                          10000);
  free(t);                                    /* result discarded, as in JS */
  cJSON *empty = cJSON_CreateArray();
  int n = geojson_emit_features(sink, "dam-water-level", empty);
  cJSON_Delete(empty);
  return n >= 0 ? 0 : -1;
}

static const source_def dam_water_level_def = {
  .id = "dam-water-level", .collector = "infrastructure",
  .name = "Dam Water Levels", .name_ja = "\xe3\x83\x80\xe3\x83\xa0\xe8\xb2\xaf\xe6\xb0\xb4\xe4\xbd\x8d",
   .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(dam_water_level_def)
