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

#endif
