/* lib/threatintel.h — port of utils/threatIntelCollectorFactory.js
 * (createThreatIntelCollector / createCollector). Backs ~18 *Jp threat-intel
 * collectors. Each becomes a tiny source.c: declare env key + a run callback
 * that fetches and returns a GeoJSON `features` array; the toolkit handles
 * credential resolution and routes features through lib/geojson.c (the same
 * collectorMirror sink path the JS FeatureCollection envelope feeds).
 *
 * JS envFor() is BYOK-aware (tenant_secret → process.env → null). Node also
 * overlays server/data/api-keys.json into process.env at boot. Here key
 * resolution is getenv(env_key) then each fallback (the api-keys.json overlay
 * for collectors mirrors the same deferral as auth JWKS). Keyless run →
 * run(NULL,…). No key & env_key set → emit nothing (JS "<id>_no_key"). */
#ifndef JO_THREATINTEL_H
#define JO_THREATINTEL_H
#include "../source.h"
#include "../third_party/cJSON.h"

/* Fetch + map. Return a cJSON array of GeoJSON Features (ownership transfers
 * to the toolkit) or NULL on failure (JS "<id>_error", 0 rows). */
typedef cJSON *(*ti_run)(const char *key, const source_ctx *ctx, void *ud);

/* env_key NULL → keyless (run(NULL,…)). fallbacks: NULL-terminated extra var
 * names tried in order. Returns #emitted, 0 if gated/empty, -1 on bad args. */
int threatintel_collect(const source_ctx *ctx, intel_sink *sink,
                         const char *env_key, const char *const *fallbacks,
                         ti_run run, void *ud);

#endif
