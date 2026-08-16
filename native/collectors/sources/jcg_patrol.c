/* collectors/safety/sources/jcg_patrol.c
 * Port of server/src/collectors/jcgPatrol.js. tryJcg() probes the JCG kouhou
 * index page but never maps it (returns null even on HTTP 200); the only data
 * is the curated SEED, which rule 8 drops. Faithful behaviour: GET the page,
 * emit 0 live rows. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>

#define JCG_URL "https://www.kaiho.mlit.go.jp/info/kouhou/index.html"

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *probe = feed_get_text(ctx->http, JCG_URL, 10000);
  if (probe) free(probe);                        /* JS: response discarded */

  cJSON *features = cJSON_CreateArray();         /* live always [] */
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jcg-patrol] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def jcg_patrol_def = {
  .id = "jcg-patrol", .collector = "safety",
  .name = "JCG Patrol Bases", .name_ja = "海上保安庁 巡視船基地",
   .update_interval_sec = 604800, .run = run,
};
REGISTER_SOURCE(jcg_patrol_def)
