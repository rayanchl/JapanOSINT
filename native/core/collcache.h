/* core/collcache.h — shared collector_cache get/set (same table + semantics
 * as dataapi.c's static cache_get/cache_set and Node collectorCache.js).
 * Used by sweepapi.c (instant read) and scheduler.c (write-time prebuild).
 * Standalone on purpose so it doesn't touch the parallel-owned dataapi.c. */
#ifndef JO_COLLCACHE_H
#define JO_COLLCACHE_H
#include "db.h"

/* Fresh cached FC JSON (age <= ttl_ms) or NULL; *age_out set on hit.
 * Caller frees. Missing table → NULL. */
char *collcache_get(db_handle *db, const char *key, long long *age_out);

/* Upsert key→fc_json with clamped TTL (15min default / 1min..24h, per
 * collectorCache.js). Best-effort; missing table silently skipped. */
void  collcache_set(db_handle *db, const char *key, const char *fc_json,
                    long long ttl_ms);

#endif
