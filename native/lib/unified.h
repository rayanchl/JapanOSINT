/* lib/unified.h — P6: port of server/src/utils/unifiedCollectorTemplate.js
 * (createUnifiedCollector) + server/src/collectors/_dedupe.js
 * (mergeFeatureCollections / dedupeByKeys / normName / countBySource).
 *
 * A `unified-*` collector fuses several ALREADY-REGISTERED upstream sources.
 * In C the upstreams are looked up via registry_get(name) and run against an
 * in-memory CAPTURE sink (reconstructs each emitted item's Feature from
 * it->geometry_geojson + it->properties_json — every upstream emits via
 * geojson_emit_features so both are always set). Promise.allSettled parity:
 * an upstream whose run() returns rc<0 contributes 0 features (rejected).
 *
 * Each unified source.c supplies its dedupe key fns / filter / postProcess as
 * static C functions and calls unified_collect. The `_meta` envelope is
 * dropped per RULE 8 (consistent with every other port); features are emitted
 * via geojson_emit_features exactly like the upstreams.
 *
 * normName/toFixed are pragmatic ports (no ICU NFKC; %.*f rounding) — only
 * the id-less coord-grid fallback key + countBySource depend on them, a
 * minority; deterministic and post-parity acceptable (cf. linecolor.c). */
#ifndef JO_UNIFIED_H
#define JO_UNIFIED_H
#include "../source.h"
#include "../third_party/cJSON.h"

/* Return a key string into buf (and buf), or NULL if this fn doesn't apply
 * (JS `f.properties?.x || null`). First non-NULL keyfn wins the slot. */
typedef const char *(*unified_keyfn)(cJSON *feat, char *buf, size_t n);
typedef int  (*unified_filterfn)(cJSON *feat);   /* 1 = keep */
typedef void (*unified_postfn)(cJSON *feat);     /* mutate properties */

typedef struct { const char *name; const char *kind; } unified_upstream;

/* JS normName: NFKC(skipped).toLowerCase, strip 駅|station|停留所|バス停|stop
 * and [\s・\-()（）、,.]. Writes into out (caller-sized). Exposed for the
 * bespoke unified-ais-ships hand-port. */
void unified_norm_name(const char *raw, char *out, size_t n);

/* Run an already-registered upstream against the in-memory capture sink and
 * append its Features (each {type,geometry,properties}, reconstructed from
 * the emitted item's geometry_geojson+properties_json) to `out` (a cJSON
 * array). Promise.allSettled parity: a missing source or run() rc<0 appends
 * nothing. Returns #captured (>=0). For the bespoke unified-ais-ships
 * hand-port (custom mmsi/imo/freshness fusion that the factory can't model). */
int unified_capture(const source_ctx *ctx, const char *upstream_id,
                    cJSON *out);

/* keyfns/n_keys may be 0 → no dedupe (raw merge). coord_precision = JS
 * dedupeOpts.coordPrecision (default 4). filter/post may be NULL. */
int unified_collect(const source_ctx *ctx, intel_sink *sink,
                    const char *source_id,
                    const unified_upstream *ups, int n_ups,
                    const unified_keyfn *keyfns, int n_keys,
                    int coord_precision,
                    unified_filterfn filter, unified_postfn post);

#endif
