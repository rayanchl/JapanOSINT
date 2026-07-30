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

/* query may be NULL/empty → returns NULL (caller: 400 query_required).
 * Otherwise spawns the run and returns {request_id,status,query}. Frees.
 * status_out (optional) carries the HTTP status the caller should reply with
 * when this returns NULL: 400 (no query / spawn failed) or 429 when the
 * concurrent-run cap (JO_SEARCH_MAX_CONCURRENT, default 4) is already
 * saturated — an unbounded thread-per-search is a trivial DoS. */
char *searchapi_analyze(db_handle *db, const char *query, int max_rounds,
                        int *status_out);

/* {"suggestions":[...]} (never NULL; "[]" on failure). Caller frees. */
char *searchapi_suggest(const char *q);

/* progress snapshot for :id, or the reconstructed-from-store row, or NULL
 * (caller: 404 not_found). Caller frees. */
char *searchapi_results(db_handle *db, const char *id);

#endif
