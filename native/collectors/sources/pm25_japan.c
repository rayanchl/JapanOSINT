/* collectors/environment/sources/pm25_japan.c
 * FEED source — port of server/src/collectors/pm25Japan.js.
 * PM2.5 has no key-free machine-readable endpoint (covered by soramame/AEROS).
 * Node returns an honest EMPTY FeatureCollection (zero features) after a
 * best-effort reachability probe. Faithful port: probe, emit NOTHING.
 * RULE 8: never fabricate — honest empty is correct here. */
#include "source.h"
#include "lib/probe.h"
#include <stdio.h>

#define PROBE "https://soramame.env.go.jp/"

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)sink;
  int reachable = probe_head(ctx->http, PROBE);
  /* Node emits zero features regardless (no public endpoint) — emit nothing. */
  fprintf(stderr, "[pm25-japan] %s; no public endpoint, emitted 0\n",
          reachable ? "pm25_japan_no_public_endpoint" : "pm25_japan_unavailable");
  return 0;
}

static const source_def pm25_japan_def = {
  .id = "pm25-japan", .collector = "environment",
  .name = "PM2.5 Distribution", .name_ja = "PM2.5分布予測",
  .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(pm25_japan_def)
