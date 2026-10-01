/* collectors/environment/sources/jma_ocean_temp.c
 * Port of server/src/collectors/jmaOceanTemp.js. tryJmaSst() probes the JMA
 * daily SST HQ JSON but never maps it (returns null even on HTTP 200); the
 * only data is the curated SEED, which rule 8 drops. Faithful behaviour: GET
 * the endpoint, emit 0 live rows. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include <stdio.h>

#define JMA_SST_URL "https://www.data.jma.go.jp/gmd/kaiyou/data/db/kaikyo/daily/sst_HQ.json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *probe = feed_get_json(ctx->http, JMA_SST_URL, 10000);
  if (probe) cJSON_Delete(probe);               /* JS: response discarded */

  cJSON *features = cJSON_CreateArray();         /* live always [] */
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jma-ocean-temp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def jma_ocean_temp_def = {
  .id = "jma-ocean-temp", .collector = "environment",
  .name = "JMA Sea Surface Temp", .name_ja = "気象庁 海面水温",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(jma_ocean_temp_def)
