/* core/collector_ttl.{c,h} — the WRITER for collector_ttls.
 *
 * WHY THIS EXISTS. core/dataapi.c (get_ttl_ms) and core/intelapi.c (the
 * source list's `ttl_ms`) have read `collector_ttls` since the port, and
 * nothing in the C tree wrote it: the comment beside the dataapi reader says
 * the Node side created and seeded it, and the Node side is gone. So every
 * source answered the 15-minute default and the table could not be changed
 * short of hand-editing the database (found 2026-11-12, the owner's decision
 * being "wire writers"). This is the operator surface for it:
 *
 *   GET    /api/admin/collector-ttls          every stored row + the bounds
 *   GET    /api/admin/collector-ttls/:key     one key, as the readers resolve it
 *   PUT    /api/admin/collector-ttls/:key     {"ttl_ms": N}
 *   DELETE /api/admin/collector-ttls/:key     back to the default
 *
 * all behind the platform-operator gate /api/admin/thresholds uses, every
 * write audited ("collector_ttl.set" / "collector_ttl.reset", old and new).
 *
 * THE KEY IS A SOURCE ID, because that is what both readers look up: dataapi
 * passes the /api/data/:id id and intelapi each source's id. A key is
 * accepted only when registry_get() knows it, so a typo is a 404 rather than
 * a row nothing will ever read. (The table's name is Node's: there a
 * "collector" was what this tree calls a source.)
 *
 * THE BOUNDS ARE THE CACHE'S OWN. collcache_set() clamps every TTL it stores
 * into [1 min, 24 h] (core/collcache.c clamp_ttl, the collectorCache.js
 * rule), so a value outside that would be accepted here and silently not be
 * the TTL in force. Out of range is therefore a 400, not a clamp.
 *
 * WHEN IT APPLIES. The freshness test reads the ttl_ms stamped on the cached
 * row when it was written (collector_cache.ttl_ms), so a new TTL governs the
 * next cache write for that source; the `ttl_ms` intelapi reports changes at
 * once. The payload says so rather than leaving a reader to wonder. */
#ifndef JO_COLLECTOR_TTL_H
#define JO_COLLECTOR_TTL_H

#include <stddef.h>
#include "db.h"

#define COLLECTOR_TTL_DEFAULT_MS (15LL * 60 * 1000)        /* dataapi/intelapi */
#define COLLECTOR_TTL_MIN_MS     (60LL * 1000)             /* collcache clamp  */
#define COLLECTOR_TTL_MAX_MS     (24LL * 60 * 60 * 1000)   /* collcache clamp  */

/* Idempotent: the table (schema.sql already declares it) and the updated_by
 * column this writer adds. Safe to call on every request. */
void collector_ttl_migrate(db_handle *db);

/* The route bodies. Each returns a malloc'd JSON body and sets *status; NULL
 * only on OOM. `user_id` is the operator, written to the audit chain. */
char *collector_ttl_api_list(db_handle *db, int *status);
char *collector_ttl_api_get(db_handle *db, const char *key, int *status);
char *collector_ttl_api_put(db_handle *db, const char *key, const char *body,
                            size_t body_len, const char *user_id, int *status);
char *collector_ttl_api_delete(db_handle *db, const char *key,
                               const char *user_id, int *status);

#endif
