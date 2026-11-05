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

/* THIS COLLECTOR CANNOT EMIT A RECORD, and said so in a comment rather than in
 * the database (2026-11-05). It fetches JMA_SST_URL, DISCARDS the response, and
 * emits an empty array, under a comment reading "live always []" — so since it
 * was ported it has spent one request a run to store nothing, `rc=0 stored=0`
 * on every line: byte for byte what a source that ran and found nothing looks
 * like. That is rule 1's invisible nothing with no upstream to blame, and
 * `jartic_traffic.c` is the only other copy of the shape (both were faithful
 * ports of JS collectors that returned null on HTTP 200).
 *
 * The endpoint is also gone: `/gmd/kaiyou/...` 301s to the same path without
 * `gmd`, which answers **404**, and so does the whole `kaikyo/daily/`
 * directory. So it cannot be implemented from here either, and the successor
 * is unmeasured rather than diagnosed.
 *
 * It returns -1 on the failed fetch now, so sched_disclose_fetch_failure names
 * the host, the bytes and the remedy. It is NOT removed here: `core/layers.def`
 * and `core/statusapi.c` both name this id, so a removal is a count decision
 * with references to re-point and belongs with the owner, not inside a
 * disclosure fix. The evidence is on the record either way.
 *
 * If the SST grid is ever re-pointed, note that the second half of the defect
 * survives this change: a successful fetch still emits nothing, because nothing
 * maps the body. Both halves have to be fixed to make this a source. */
static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *probe = feed_get_json(ctx->http, JMA_SST_URL, 10000);
  if (!probe) {
    fprintf(stderr, "[jma-ocean-temp] %s did not answer with JSON — reporting "
                    "a failed fetch, not an empty ocean\n", JMA_SST_URL);
    return -1;
  }
  cJSON_Delete(probe);                          /* JS: response discarded */

  cJSON *features = cJSON_CreateArray();         /* live always [] — see above */
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jma-ocean-temp] emitted %d (this collector maps nothing: "
                  "the body above is discarded)\n", n);
  return n;
}

static const source_def jma_ocean_temp_def = {
  .id = "jma-ocean-temp", .collector = "environment",
  .name = "JMA Sea Surface Temp", .name_ja = "気象庁 海面水温",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(jma_ocean_temp_def)
