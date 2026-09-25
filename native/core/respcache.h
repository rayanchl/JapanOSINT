/* core/respcache.h — a small TTL cache for whole JSON response bodies.
 *
 * WHY. `/api/status` builds a 20.5 MB body from an unbounded `GROUP BY
 * source_id` over intel_items plus every row of the sources table, and
 * `/api/intel/sources` builds ~10 MB the same way. Measured 2026-09-11 against
 * a 129 MB database: **three concurrent /api/status calls took 24.9 s each and
 * blocked every other connection on the server for 24.8 s** — while five
 * concurrent collector runs, far heavier work, left /api/health at 1.7 ms,
 * because those run off the event loop. Both clients call /api/status on
 * startup, so two people opening the app at once was already that scenario.
 *
 * The answer these routes give changes at SCHEDULER cadence — a source's
 * item_count moves when a collector run stores rows, not when someone asks —
 * so recomputing it per request is pure waste. A few seconds of staleness on a
 * fleet-health dashboard is not a lie; presenting it as live would be, which is
 * why every hit carries its age (see below).
 *
 * WHAT IT IS NOT. Not a general cache, not per-user, and deliberately hard to
 * misuse for per-user data: the key is caller-chosen and the tree's own rule is
 * that only a body identical for every caller may go in one. Both current
 * callers qualify — `statusapi_build(db, include_breach)` and
 * `intelapi_intel_sources(db)` take no tenant and no user, and the operator
 * variant is a separate key. A route that gains tenant scoping must either key
 * on the tenant or stop caching; there is no "mostly the same" case.
 *
 * DISCLOSURE. A hit is never silent. `respcache_get()` hands back the age in
 * milliseconds and every caller states it — `X-Cache: hit; age=NNNms` plus a
 * standard `Age:` header — so a client reading a number that is four seconds
 * old can know that. A body is stored byte-for-byte and handed back as a copy;
 * it is never edited, because editing a cached JSON string to stamp it is how
 * you end up with two versions of the truth.
 */
#ifndef JO_RESPCACHE_H
#define JO_RESPCACHE_H

#include <stddef.h>

/* A fresh copy of the cached body for `key`, or NULL when there is no entry or
 * it is older than `ttl_sec`. `age_ms` (may be NULL) receives the entry's age.
 * Caller frees. */
char *respcache_get(const char *key, int ttl_sec, long *age_ms);

/* Store (or replace) the body for `key`. Takes a COPY; the caller keeps
 * ownership of `body`. A NULL or empty body is not stored — a failed build must
 * not be served to the next caller for the length of the TTL. */
void respcache_put(const char *key, const char *body);

/* Drop an entry (or all of them, with key == NULL). For a write path that
 * knows it has invalidated the answer. */
void respcache_drop(const char *key);

/* Counters for /api/status's own diagnostics: hits, misses, bytes held. */
void respcache_stats(long long *hits, long long *misses, size_t *bytes);

#endif /* JO_RESPCACHE_H */
