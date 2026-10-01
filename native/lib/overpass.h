/* lib/overpass.h — OSM Overpass toolkit, port of _liveHelpers.js
 * fetchOverpass / fetchOverpassTiled / fetchOverpassWaysTiled. Backs ~101
 * collectors: each becomes a tiny source.c supplying the query body + a
 * per-element map callback that builds the GeoJSON Feature (exact JS
 * property key order → featureUid hash parity via lib/geojson.c).
 *
 * Faithful to the JS data path. Two JS behaviours are intentionally NOT
 * ported because they are correctness-neutral for emitted rows:
 *   - the per-query 6h in-memory cache (scheduler interval already throttles
 *     upstream; each body is distinct so cross-collector cache never hits);
 *   - the per-host 2-concurrent/500ms politeness queue (single-threaded
 *     scheduler runs one source at a time anyway).
 * The curated offline SEED fallback is collector-specific synthetic data and
 * lives in each source.c if/when ported — not in this toolkit. */
#ifndef JO_OVERPASS_H
#define JO_OVERPASS_H
#include "../source.h"
#include "../third_party/cJSON.h"

/* Map one OSM element at (lon,lat) to a GeoJSON Feature cJSON. Return NULL to
 * skip. The returned object's ownership transfers to the toolkit. Build
 * `properties` keys in the SAME order as the JS mapFn — featureUid uses
 * sha1(JSON.stringify{g,p}) for the OSM-id minority. */
typedef cJSON *(*overpass_map)(cJSON *el, int idx, double lon, double lat,
                               void *ud);

/* Way variant: `coords` is a cJSON array of [lon,lat] pairs (el.geometry). */
typedef cJSON *(*overpass_way_map)(cJSON *el, int idx, cJSON *coords,
                                   void *ud);

/* bodyfn writes the Overpass body for a tile bbox "s,w,n,e" into out. */
typedef void (*overpass_bodyfn)(const char *bbox, char *out, size_t n,
                                void *ud);

/* el.tags?.[k] as a C string, or NULL (=== JS undefined for `||` chains).
 * Use `ov_tag(el,"name") ? ... : "Hospital"` to mirror `tags?.name || 'X'`. */
const char *ov_tag(cJSON *el, const char *k);

/* Per-feature consumer for overpass_collect_via(). Called once per mapped
 * Feature, in element order. OWNERSHIP: `f` is borrowed — the toolkit frees it
 * after the call, exactly as it would after geojson_emit_features(). Return >=0
 * to count the feature, <0 to skip it (mirrors camera_upsert's contract). */
typedef int (*overpass_emit)(cJSON *f, void *ud);

/* overpass_collect() with the emit step replaced: same fetch, same endpoint
 * failover, same `map` callback, but each Feature goes to `emit` instead of
 * geojson_emit_features(). For collectors that own a different keyspace —
 * public_cameras routes through camera_upsert so its rows land in the camera
 * keyspace (record_type='camera', uid 'camera-discovery|…') and show on the
 * cameras layer, which the default geojson path cannot do.
 * Returns #features accepted by `emit`, or -1 if every endpoint failed. */
int overpass_collect_via(const source_ctx *ctx, const char *body,
                         int query_timeout, int timeout_ms,
                         overpass_map map, void *ud,
                         overpass_emit emit, void *eud);

/* Single nationwide query. `body` = inner clause(s) referring to area.jp,
 * e.g. "node[\"amenity\"=\"hospital\"](area.jp);way[...](area.jp);". Toolkit
 * wraps it as [out:json][timeout:QT];area["ISO3166-1"="JP"][admin_level=2]
 * ->.jp;(BODY);out center;. query_timeout<=0 → 180, timeout_ms<=0 → 60000.
 * Emits via geojson_emit_features(sink, ctx->source_id, …).
 * Returns #emitted, or -1 if every endpoint failed (caller may seed). */
int overpass_collect(const source_ctx *ctx, intel_sink *sink,
                     const char *body, int query_timeout, int timeout_ms,
                     overpass_map map, void *ud);

/* 12-tile nationwide fanout, deduped by "type/id". bodyfn must NOT reference
 * area.jp — use the bbox. query_timeout<=0 → 180, timeout_ms<=0 → 60000. */
int overpass_tiled_collect(const source_ctx *ctx, intel_sink *sink,
                           overpass_bodyfn bodyfn, int query_timeout,
                           int timeout_ms, overpass_map map, void *ud);

/* 12-tile `out geom;` way fanout (full polyline), deduped by way id.
 * query_timeout<=0 → 180, timeout_ms<=0 → 120000. */
int overpass_ways_collect(const source_ctx *ctx, intel_sink *sink,
                          overpass_bodyfn bodyfn, int query_timeout,
                          int timeout_ms, overpass_way_map map, void *ud);

#endif
