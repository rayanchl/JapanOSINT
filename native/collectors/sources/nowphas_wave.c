/* collectors/environment/sources/nowphas_wave.c
 * Port of server/src/collectors/nowphasWave.js. tryNowphas() probes the PARI
 * NOWPHAS pastdata page but never maps it (returns null even on HTTP 200);
 * the only data is the curated SEED, which rule 8 drops. Faithful behaviour:
 * GET the page, emit 0 live rows. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>

#define NOWPHAS_URL "https://nowphas.mlit.go.jp/pastdata"

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *probe = feed_get_text(ctx->http, NOWPHAS_URL, 10000);
  if (probe) free(probe);                        /* JS: response discarded */

  cJSON *features = cJSON_CreateArray();         /* live always [] */
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[nowphas-wave] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def nowphas_wave_def = {
  .id = "nowphas-wave", .collector = "environment",
  .name = "NOWPHAS Wave Network", .name_ja = "NOWPHAS 波浪観測網",
   .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(nowphas_wave_def)
