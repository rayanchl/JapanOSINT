/* lib/mlit_ksj.h — port of server/src/utils/mlitNormalizer.js (KSJ_CONFIG
 * per-dataset field-alias maps + geometryToPoint + the createMlitKsjCollector
 * try-env-url → mirrors path). Per HARD RULE 8 / the JSON_API fallback-drop
 * decision: only the MLIT primary live path (env URL then mirrors) is ported;
 * the OSM Overpass fallback and `_meta` envelope are intentionally dropped
 * (correctness-neutral: real rows when an MLIT GeoJSON mirror is up, 0 when
 * down — identical contract to every other ported live collector).
 *
 * Each mlit-* collector becomes a tiny source.c calling mlit_ksj_collect with
 * its code + env key + mirror list. Property key insertion order mirrors JS
 * `{[idField]:idValue, ...norm, ...extras, source}` exactly (uid stability). */
#ifndef JO_MLIT_KSJ_H
#define JO_MLIT_KSJ_H
#include "../source.h"

/* code: "N02"|"P02"|"N05"|"N07"|"P11"|"C02". mirrors = NULL-terminated-ish
 * array of n_mirrors fallback URLs (tried after getenv(env_key) if set).
 * abolished_only = N05's `filter:(f)=>f.properties.status==='abolished'`
 * (pass 0 for every other dataset). Emits the first non-empty mirror's
 * normalized features via geojson_emit_features; returns #emitted (>=0) or -1
 * on hard failure.
 *
 * NO MIRROR RESPONDING IS A FAILURE (-1), not success. This line used to say
 * the opposite and the .c implemented it, so six national registers fetched
 * two 404 pages each and reported `rc=0 records=0` — the invisible nothing of
 * rule 1. See the comment at the return site for the measurement. A mirror
 * that really serves an EMPTY array is still an honest 0. */
int mlit_ksj_collect(const source_ctx *ctx, intel_sink *sink,
                     const char *code, const char *env_key,
                     const char *const *mirrors, int n_mirrors,
                     int abolished_only);

#endif
