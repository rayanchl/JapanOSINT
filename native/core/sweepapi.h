/* core/sweepapi.h — /api/data/<id> read wiring for the sweep layers
 * (port of transportRead.js / unifiedStationsRead.js / cameras.js read side).
 * unified-<mode> → intel_items rows for source_id=unified-<mode> as a
 * lines+stations FeatureCollection; cameras → camera_fc_json;
 * unified-stations → clusterer line-dots; unified-station-footprints →
 * footprints. Read-time stitch/Chaikin/RDP smoothing is the documented
 * follow-up (raw stored fragments are returned — correct data). */
#ifndef JO_SWEEPAPI_H
#define JO_SWEEPAPI_H
#include "db.h"

/* Instant read path for /api/data/:layer. Serves the prebuilt FC from
 * collector_cache; if cold (cron hasn't built it yet) returns a BOUNDED
 * raw-points FC immediately. NEVER stitches/smooths or full-scans on the
 * request path. NULL iff `id` is not a sweep layer (caller falls through to
 * dataapi_layer). */
char *sweepapi_data(db_handle *db, const char *id);

/* WRITE-TIME heavy build (stitch + Chaikin/RDP smooth, full scan) — invoked
 * by the scheduler after a map_cron source runs, NOT on the HTTP path.
 * Returns the full malloc'd FC for a sweep `id`, or NULL if not a sweep
 * layer. Caller persists it via collcache_set(). */
char *sweepapi_build(db_handle *db, const char *id);

/* 1 if `id` is one of the sweep layers handled here. */
int   sweepapi_is_sweep(const char *id);

#endif
