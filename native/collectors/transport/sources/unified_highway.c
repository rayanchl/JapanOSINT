/* collectors/transport/sources/unified_highway.c
 * Port of server/src/collectors/unifiedHighway.js (createUnifiedCollector).
 * Fuses highway-traffic (kind 'node') + jartic-traffic (kind 'congestion'),
 * both already-registered upstreams. No dedupe (node/congestion geometries
 * are disjoint), no filter, no postProcess. _meta envelope dropped per
 * RULE 8 (same as every other port). */
#include "../../../source.h"
#include "../../../lib/unified.h"
#include "../../../third_party/cJSON.h"

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const unified_upstream ups[] = {
    { "highway-traffic", "node" },
    { "jartic-traffic",  "congestion" },
  };
  int n = unified_collect(ctx, sink, "unified-highway", ups, 2,
                          NULL, 0, 4, NULL, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def unified_highway_def = {
  .id = "unified-highway", .collector = "transport",
  .name = "Expressways (fused)",
  .name_ja = "\xe9\xab\x98\xe9\x80\x9f\xe9\x81\x93\xe8\xb7\xaf (\xe7\xb5\xb1\xe5\x90\x88)",
   .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(unified_highway_def)
