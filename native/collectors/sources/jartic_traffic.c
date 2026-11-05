/* collectors/transport/sources/jartic_traffic.c
 * Port of server/src/collectors/jarticTraffic.js. tryJartic() probes the
 * JARTIC road_traffic.json endpoint but never maps it (returns null even on
 * HTTP 200); the only data is the curated SEED, which rule 8 drops. Faithful
 * behaviour: GET the endpoint, emit 0 live rows. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include <stdio.h>

#define JARTIC_URL "https://www.jartic.or.jp/d/traffic_info/road_traffic.json"

/* THE SECOND COPY of the shape jma_ocean_temp.c describes, and the expensive
 * one: `update_interval_sec = 300`, so this has been spending **288 requests a
 * day** on an endpoint it never maps. Measured 2026-11-05: JARTIC_URL answers
 * **HTTP 404** (146 bytes of nginx's page), and the header above already says
 * the collector "never maps it (returns null even on HTTP 200)" — so even a
 * live endpoint would emit nothing. `rc=0 records=0 stored=0` throughout.
 *
 * Returns -1 on the failed fetch so the run is visible in the database rather
 * than only in this comment. Not removed here: `collectors/sources/
 * unified_highway.c` fuses this id as its `congestion` upstream and
 * `core/layers.def` names it, so a removal is a count decision with references
 * and belongs with the owner. See jma_ocean_temp.c for the full reasoning. */
static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *probe = feed_get_json(ctx->http, JARTIC_URL, 10000);
  if (!probe) {
    fprintf(stderr, "[jartic-traffic] %s did not answer with JSON — reporting "
                    "a failed fetch, not empty roads\n", JARTIC_URL);
    return -1;
  }
  cJSON_Delete(probe);                          /* JS: response discarded */

  cJSON *features = cJSON_CreateArray();         /* live always [] — see above */
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jartic-traffic] emitted %d (this collector maps nothing: "
                  "the body above is discarded)\n", n);
  return n;
}

static const source_def jartic_traffic_def = {
  .id = "jartic-traffic", .collector = "transport",
  .name = "JARTIC Traffic", .name_ja = "JARTIC 道路交通情報",
   .update_interval_sec = 300, .run = run,
};
REGISTER_SOURCE(jartic_traffic_def)
