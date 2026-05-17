/* collectors/transport/sources/jartic_traffic.c
 * Port of server/src/collectors/jarticTraffic.js. tryJartic() probes the
 * JARTIC road_traffic.json endpoint but never maps it (returns null even on
 * HTTP 200); the only data is the curated SEED, which rule 8 drops. Faithful
 * behaviour: GET the endpoint, emit 0 live rows. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>

#define JARTIC_URL "https://www.jartic.or.jp/d/traffic_info/road_traffic.json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *probe = feed_get_json(ctx->http, JARTIC_URL, 10000);
  if (probe) cJSON_Delete(probe);               /* JS: response discarded */

  cJSON *features = cJSON_CreateArray();         /* live always [] */
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jartic-traffic] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def jartic_traffic_def = {
  .id = "jartic-traffic", .collector = "transport",
  .name = "JARTIC Traffic", .name_ja = "JARTIC 道路交通情報",
   .update_interval_sec = 300, .run = run,
};
REGISTER_SOURCE(jartic_traffic_def)
