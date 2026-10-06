/* core/searchapi.h — /api/search wiring (port of server/src/routes/search.js).
 *   POST /api/search/analyze {query,max_rounds?} -> {request_id,...}
 *   GET  /api/search/suggest?q=                  -> {suggestions:[..≤9]}
 *   GET  /api/search/results/:id                 -> snapshot | from-store
 *   GET  /api/search/runs?limit&mine&cursor      -> the workspace's runs
 *   GET  /api/search/stream/:id?key=<stream_key> -> SSE (pre-auth) — handled
 *                                                   in httpd.c via progress.h.
 * A run belongs to the tenant that started it: results answer only that
 * tenant, and the stream needs the run's stream_key (handed out by analyze
 * and by results) or an Authorization header resolving to the owner tenant.
 * The request_id alone — which share links carry — opens neither.
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
 *   500  no owner tenant, or the owner row could not be written
 * On success `*out_status` is 200 and the body also carries `stream_key`.
 * `out_status` may be NULL. */
char *searchapi_analyze(db_handle *db, const char *tenant_id,
                        const char *user_id, const char *query, int max_rounds,
                        int *out_status);

/* {"suggestions":[...]} (never NULL; "[]" on failure). Caller frees. */
char *searchapi_suggest(const char *q);

/* progress snapshot for :id (plus its `stream_key`), or the
 * reconstructed-from-store row, or NULL (caller: 404 not_found) — NULL too
 * when `tenant_id` did not start the run. Caller frees. */
char *searchapi_results(db_handle *db, const char *tenant_id, const char *id);

/* GET /api/search/runs?limit=&mine=1&cursor= — every run started in the
 * workspace `tenant_id`, newest first: request_id, query, user_id (author),
 * mine, created_at, status (running|completed|error|unknown), phase, and a
 * synthesis preview with its full byte length. page {limit,count,total,
 * next_cursor}; meta.scope is "workspace", or "user" with ?mine=1. Nothing
 * crosses workspaces. limit defaults to 50, clamps to 1..200. NULL + 500 on a
 * database failure. Caller frees. */
char *searchapi_runs(db_handle *db, const char *tenant_id, const char *user_id,
                     int limit, int mine_only, const char *cursor,
                     int *out_status);

#endif
