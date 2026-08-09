/* lib/threatintel.c — port of threatIntelCollectorFactory.js. See header. */
#include "threatintel.h"
#include "geojson.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

/* ── abuse.ch Auth-Key (see header for the measured 401/200 split) ──────── */
static char        g_abusech_hdr[160];
static int         g_abusech_ok;
static pthread_once_t g_abusech_once = PTHREAD_ONCE_INIT;

static void abusech_init(void) {
  /* Same variable the existing key-gated abuse.ch collectors already use
   * (urlhaus-jp, threatfox-jp via threatintel_collect; threatfeeds-world and
   * HASH_LOOKUP inline). Deliberately NOT a new name — one credential should
   * not have two spellings. */
  const char *k = getenv("ABUSE_CH_AUTH_KEY");
  if (!k || !*k) k = getenv("THREATFOX_AUTH_KEY");
  if (!k || !*k) return;
  /* Truncation would send a corrupt key and get a confusing 401, so refuse
   * rather than half-send it. */
  int n = snprintf(g_abusech_hdr, sizeof g_abusech_hdr, "Auth-Key: %s", k);
  if (n <= 0 || (size_t)n >= sizeof g_abusech_hdr) {
    fprintf(stderr, "[abuse.ch] ABUSECH_AUTH_KEY too long (%d bytes); ignored\n", n);
    g_abusech_hdr[0] = 0;
    return;
  }
  g_abusech_ok = 1;
}

/* pthread_once, not a plain flag: collectors run on the scheduler's worker
 * pool and several abuse.ch sources can initialise this concurrently. */
const char *abusech_auth_header(void) {
  pthread_once(&g_abusech_once, abusech_init);
  return g_abusech_ok ? g_abusech_hdr : NULL;
}

int abusech_have_key(void) {
  pthread_once(&g_abusech_once, abusech_init);
  return g_abusech_ok;
}

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
