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

/* ── abuse.ch auth ────────────────────────────────────────────────────────
 * abuse.ch moved every `*-api.abuse.ch` endpoint behind a (free) Auth-Key.
 * Measured 2026-08-01:
 *     401  urlhaus-api.abuse.ch/v1/urls/recent/
 *     401  threatfox-api.abuse.ch/api/v1/
 *     401  mb-api.abuse.ch/api/v1/          (MalwareBazaar)
 *     200  urlhaus.abuse.ch/downloads/json_recent/
 *     200  threatfox.abuse.ch/export/json/recent/
 *     200  feodotracker.abuse.ch/downloads/ipblocklist.json
 *     200  sslbl.abuse.ch/blacklist/ (both .csv blocklists)
 * So six collectors (urlhaus-jp, threatfox-jp, threat-feed, threatfeeds-world,
 * IOC_LOOKUP, HASH_LOOKUP) have been silently returning nothing — and because
 * they signalled that as a failure, the circuit breaker quarantined them.
 *
 * urlhaus-jp, threatfox-jp, threatfeeds-world and HASH_LOOKUP already resolve
 * this credential correctly and gate honestly without it. THREAT_FEED and
 * IOC_LOOKUP did not, and simply got a 401 they reported as a fetch failure.
 * This exists so they stop hand-rolling a fourth copy of the header.
 *
 * Returns an "Auth-Key: …" header string (owned by the callee, valid for the
 * process lifetime) when ABUSE_CH_AUTH_KEY (or THREATFOX_AUTH_KEY) is set,
 * else NULL. A NULL return is NOT an error: fall back to the keyless bulk
 * export where one exists, otherwise gate honestly (log once, return 0) per
 * contract R6 — never report "needs a key" as a data row or a failed fetch. */
const char *abusech_auth_header(void);

/* 1 if an abuse.ch credential is configured. */
int abusech_have_key(void);

#endif
