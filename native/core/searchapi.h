/* core/searchapi.h — /api/search wiring (port of server/src/routes/search.js).
 *   POST /api/search/analyze {query,max_rounds?} -> {request_id,...}
 *   GET  /api/search/suggest?q=                  -> {suggestions:[..≤9]}
 *   GET  /api/search/results/:id                 -> snapshot | from-store
 *   GET  /api/search/stream/:id                  -> SSE (pre-auth; UUID =
 *                                                   capability) — handled in
 *                                                   httpd.c via progress.h.
 * analyze runs the pipeline on a detached worker thread (own http+llm, shared
 * serialized-SQLite db), returns immediately. */
#ifndef JO_SEARCHAPI_H
#define JO_SEARCHAPI_H
#include "db.h"

/* Hard ceiling on the caller-supplied `max_rounds`. Each phase-2 round is a
 * full collector fan-out plus an LLM call while holding one of the four
 * concurrent-run slots, and the parameter was previously guarded only by
 * "> 0" — {"max_rounds":2147483647} was accepted as written. 20 is four times
 * the default of 5, which is already far past the point where the pivot chain
 * stops finding anything new. */
#define SEARCH_MAX_ROUNDS_CEILING 20

/* Spawns the run and returns {request_id,status,query} (caller frees).
 *
 * `max_rounds` <= 0 means "the default 5"; anything above
 * SEARCH_MAX_ROUNDS_CEILING is silently clamped to it, the same way every
 * other numeric parameter in this tree clamps rather than 400s.
 *
 * NULL means "no run started", and `*out_status` says why so the caller can
 * pick the right HTTP code without guessing:
 *   400  query was NULL/empty/whitespace, or the run could not be started
 *   429  the concurrent-run cap is already reached (JO_MAX_CONCURRENT_SEARCHES,
 *        default 4) — a retryable condition, unlike 400
 * On success `*out_status` is 200. `out_status` may be NULL. */
char *searchapi_analyze(db_handle *db, const char *query, int max_rounds,
                        int *out_status);

/* {"suggestions":[...]} (never NULL; "[]" on failure). Caller frees. */
char *searchapi_suggest(const char *q);

/* progress snapshot for :id, or the reconstructed-from-store row, or NULL
 * (caller: 404 not_found). Caller frees. */
char *searchapi_results(db_handle *db, const char *id);

#endif
