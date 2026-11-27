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
 * AND allSettled PARITY MADE THE FUSIONS THE ONE PLACE A FIXED UPSTREAM
 * CANNOT BE HEARD (2026-11-12). Swallowing one upstream's failure is right —
 * a fusion of three should survive one bad afternoon — but swallowing ALL of
 * them reports a success nobody had. Measured on `unified-airports`, whose two
 * upstreams both said so in as many words:
 *
 *   [mlit-p02-airports] no mirror served a parseable P02 GeoJSON — reporting
 *                       a failed fetch, not an empty register
 *   [overpass] airport-infra no endpoint answered
 *   [unified-airports] emitted 0
 *   [sched] unified-airports run rc=0 records=0 134195ms stored=0 bytes=41188
 *
 * 134 seconds, 41,188 bytes — the KSJ dead-mirror byte count CLAUDE.md
 * records — and `rc=0 records=0 stored=0`, byte for byte what a fusion with
 * nothing new to fuse looks like. The first of those two sentences is the
 * 2026-11-05 `lib/mlit_ksj.h` repair talking, so this is rule 1's "look for
 * the other copies" in a form that pass could not have seen: the other copies
 * were not parallel implementations but CONSUMERS of the function it fixed.
 * Four of the six KSJ rows that repair covers are read by a fusion
 * (mlit-c02-ports, mlit-n02-stations, mlit-p02-airports, mlit-p11-bus-stops),
 * and every one of those fusions reported rc=0.
 *
 * So TOTAL failure returns -1 and `sched_disclose_fetch_failure` files the
 * record; partial failure stays silent, and the reason is a measurement
 * rather than a preference: all 22 upstream ids these nine fusions name are
 * themselves REGISTERED sources (checked against the binary's own registry,
 * 22 of 22), so each discloses on its own schedule and a second notice under
 * the fusion's id would be the duplicate noise rule 1 says these disclosures
 * cannot afford. The three outcomes are counted and printed per run, which is
 * also how a dangling upstream id would now show (`not registered` — zero
 * today, and the zero was checked, not assumed).
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
 * hand-port (custom mmsi/imo/freshness fusion that the factory can't model).
 *
 * `served`, if non-NULL, is INCREMENTED when the upstream ran (rc>=0),
 * whether or not it had anything to give — so a caller can tell "every
 * upstream refused" from "they all ran and the fusion is quiet", which the
 * return value cannot do: it is 0 for both. Both callers discarded the
 * return value entirely, so each had to count this itself; it is a parameter
 * rather than a second function so a third caller cannot skip it by
 * accident. */
int unified_capture(const source_ctx *ctx, const char *upstream_id,
                    cJSON *out, int *served);

/* keyfns/n_keys may be 0 → no dedupe (raw merge). coord_precision = JS
 * dedupeOpts.coordPrecision (default 4). filter/post may be NULL. */
int unified_collect(const source_ctx *ctx, intel_sink *sink,
                    const char *source_id,
                    const unified_upstream *ups, int n_ups,
                    const unified_keyfn *keyfns, int n_keys,
                    int coord_precision,
                    unified_filterfn filter, unified_postfn post);

#endif
