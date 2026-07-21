/* lib/threatintel.c — port of threatIntelCollectorFactory.js. See header. */
#include "threatintel.h"
#include "geojson.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int threatintel_collect(const source_ctx *ctx, intel_sink *sink,
                         const char *env_key, const char *const *fallbacks,
                         ti_run run, void *ud) {
  if (!run) return -1;

  const char *key = NULL;
  if (env_key) {
    const char *v = getenv(env_key);
    if (v && *v) key = v;
    for (int i = 0; !key && fallbacks && fallbacks[i]; i++) {
      v = getenv(fallbacks[i]);
      if (v && *v) key = v;
    }
    if (!key) {                       /* JS "<id>_no_key": 0 features */
      fprintf(stderr, "[threatintel] %s gated (no %s)\n",
              ctx->source_id, env_key);
      return 0;
    }
  }

  cJSON *features = run(key, ctx, ud);
  if (!features || !cJSON_IsArray(features)) {  /* JS "<id>_error" */
    if (features) cJSON_Delete(features);
    fprintf(stderr, "[threatintel] %s no features\n", ctx->source_id);
    return 0;
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[threatintel] %s emitted %d\n", ctx->source_id, n);
  return n;
}
