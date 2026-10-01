/* core/dataapi.h — generic collector data-layer endpoint.
 *
 * Port of server/src/routes/data.js (the /api/data/:id route family +
 * respondWithData). The Node route is ~50 explicit per-id router.get()
 * handlers that all funnel into one respondWithData(); under the unified C
 * ABI (source_kind deleted — every collector is a registered source_def in
 * registry.c) that collapses to a single dispatcher keyed on the source id.
 *
 * The sweep layers (unified-* / cameras / unified-stations / ...) are
 * already served by core/sweepapi.c; the caller tries that first. The
 * dataapi_layer entrypoint
 * therefore only needs the *non-sweep* registered collectors:
 *
 *   registry_get(id) → run() through an in-memory CAPTURE intel_sink
 *   (reconstruct each emitted item's Feature from geometry_geojson +
 *   properties_json, exactly like lib/unified.c's cap_emit) → assemble the
 *   canonical { type:'FeatureCollection', features, _meta:{...} } envelope
 *   respondWithData() returns, gated by the collector_cache TTL.
 *
 * Returns: a malloc'd FeatureCollection JSON string (caller frees), OR NULL
 * iff `id` is genuinely not a registered source AND has no intel_items rows —
 * i.e. dataapi cannot handle it and the caller should fall through (501).
 * A registered (or rows-bearing) id ALWAYS yields an FC, never NULL: the
 * empty-FC-when-no-collector graceful path from data.js's respondWithData is
 * preserved (no 404 for a known-but-empty source).
 */
#ifndef JO_DATAAPI_H
#define JO_DATAAPI_H
#include "db.h"

char *dataapi_layer(db_handle *db, const char *id);

/* GET /api/layers/:layerId/geojson — the fused FeatureCollection of a LAYER
 * (curated in core/layers.def, declared by member sources, or a generated
 * `rt-<slug>` / `unassigned-geocoded` catch-all): every geocoded intel_items
 * row of the layer's member sources (or of the catch-all's predicate),
 * bounded by limit/offset over a total order with in-band
 * records_available / records_used / truncated / next_offset meta (house
 * rule 2 — one crime layer measured ~300 MB unbounded). limit<=0 → 10000,
 * capped at 50000; offset<=0 → 0. NULL iff `layer_id` names no layer this
 * server knows (caller → 404). A known-but-empty layer serves an honest
 * empty FC, never invented points. */
char *dataapi_layer_fc(db_handle *db, const char *layer_id,
                       int limit, int offset);

#endif
