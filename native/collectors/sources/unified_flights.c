/* collectors/transport/sources/unified_flights.c
 * Port of server/src/collectors/unifiedFlights.js — a thin passthrough:
 * `return getSnapshot()` from planeAdsbPoller. The poller's OpenSky-live ⊕
 * adsb.lol fusion (mergeLiveByIcao + classifyMilitary) is reproduced by the
 * registered `plane-adsb` source; unified-flights just captures it and
 * re-emits under source_id `unified-flights` so /api/data/unified-flights +
 * the unified read-side work. Mirrors the concurrent session's bespoke
 * unified_ais_ships.c shape (lib/unified.h capture sink); no dedupe — the
 * poller already deduped (JS is a pure passthrough). _meta dropped per
 * RULE 8; emitted via the geojson sink. */
#include "../../source.h"
#include "../../lib/unified.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *raw = cJSON_CreateArray();
  unified_capture(ctx, "plane-adsb", raw);   /* == planeAdsbPoller snapshot */
  int n = geojson_emit_features(sink, "unified-flights", raw);
  cJSON_Delete(raw);
  fprintf(stderr, "[unified-flights] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def unified_flights_def = {
  .id = "unified-flights", .collector = "transport",
  .name = "Unified Flights (live ADS-B)", .name_ja = "Unified Flights",
   .update_interval_sec = 60, .run = run };
REGISTER_SOURCE(unified_flights_def)
