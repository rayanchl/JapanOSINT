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
 * Otherwise spawns the run and returns {request_id,status,query}. Frees. */
/* Starts a pipeline run on a detached thread. Concurrency is capped
 * (JO_SEARCH_MAX, default 4) because each run owns a thread, an http_client and
 * a DB connection; at capacity this returns NULL with *status = 429 so the
 * caller can say so rather than reporting a bad request. *status is 200 on
 * success and on the plain "no query" NULL. `status` may be NULL. */
char *searchapi_analyze(db_handle *db, const char *query, int max_rounds,
                        int *status);

/* {"suggestions":[...]} (never NULL; "[]" on failure). Caller frees. */
char *searchapi_suggest(const char *q);

/* progress snapshot for :id, or the reconstructed-from-store row, or NULL
 * (caller: 404 not_found). Caller frees. */
char *searchapi_results(db_handle *db, const char *id);

#endif
