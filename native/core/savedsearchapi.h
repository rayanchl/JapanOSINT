/* core/savedsearchapi.h — roadmap 38: saved searches, per-user search history,
 * and the ONE canonical permalink state codec shared by web and iOS.
 *
 * Three surfaces that all answer "how do I get back to what I was looking at":
 *
 *   saved_searches   a named, re-runnable query the analyst chose to keep
 *   search_history   an automatic, capped trail of what they actually ran
 *   permalink        a stateless, opaque encoding of view state for sharing
 *
 * ── Privacy stance (the reason this module is not tenant-wide) ──────────────
 * Search history is PER USER, never per tenant. Every statement that touches
 * search_history carries BOTH `tenant_id=?` AND `user_id=?`, and there is no
 * endpoint, parameter or role that widens it: a tenant owner cannot read an
 * analyst's history through this module, by construction. That is deliberate.
 * A query log is not activity metadata — it is the analyst's line of enquiry,
 * and in an OSINT product "who is my colleague investigating, and did they
 * search their own name" is exactly the surveillance an owner role must not
 * silently confer. Saved searches follow the same rule for the same reason
 * (owner-scoped reads/writes); sharing a search with a team is a permalink or
 * an alert rule, both of which are explicit acts.
 *
 * ── Permalinks carry VIEW STATE ONLY, and NEVER authorization ───────────────
 * A permalink token is an opaque, UNSIGNED, UNAUTHENTICATED blob that arrives
 * from a URL a stranger may have pasted. It encodes where the camera was and
 * which filters were set — nothing else. It carries no tenant_id, no user_id,
 * no role, no case membership and no result rows, and decoding it grants
 * nothing. Every route reached with decoded state MUST re-authorize the
 * caller from the session exactly as if the parameters had been typed by
 * hand. If a future field would let a token widen what its bearer can see,
 * that field does not belong in the token — it belongs behind a real grant.
 * Consequently the decoder never touches the database and never sees a
 * tenant_ctx: permalinkapi() below is deliberately stateless so this property
 * is enforced by the signature, not by reviewer vigilance.
 *
 * The decoder is written for hostile input: bounded token length, bounded
 * decoded JSON, a version prefix that must match, every string capped, every
 * number clamped, unknown keys dropped. Encode and decode share ONE
 * normalizer (pl_normalize in the .c), so a token this server mints can
 * always be read back by this server, and no bound can drift between the two
 * directions.
 *
 * ── Deliberate deviations from house convention (and why) ───────────────────
 *  1. Saved-search and history writes are allowed for role `viewer`. The house
 *     rule is analyst+ for writes, but these rows are the caller's OWN view
 *     state, readable by nobody else, and refusing a viewer the ability to
 *     bookmark their own query buys no safety. `to-alert` is the one write
 *     that escapes the user's own row (it creates tenant-visible, outbound-
 *     mailing infrastructure) and it requires analyst+, checked here.
 *  2. Lists are limit-paged, not keyset-paged. Saved searches are personal and
 *     few; history is hard-capped at SS_HISTORY_KEEP rows per user. A cursor
 *     encoder here would be dead code on a set that cannot grow.
 *  3. Only saved_search.create / .delete / .to_alert are audited. Reads and
 *     runs are NOT: search_history already is the record of what ran, and
 *     copying it into audit_events would place the same investigative content
 *     under a second retention and access regime — the exact mistake
 *     annotationsapi.c avoids with note bodies.
 *
 * ── Reusing alertsapi's rule validation (NOT forked) ────────────────────────
 * POST /api/saved-searches/:id/to-alert does not re-implement validate_rule().
 * It builds the rule request body (name + derived predicate + the channels the
 * caller supplied, verbatim) and re-enters the PUBLIC alertsapi() entrypoint
 * as `POST /api/alerts`. Every channel/type/target/secret/email/dedup/storm
 * check therefore runs in exactly one place, alertsapi.c's static
 * validate_rule(), and its 400 body is propagated to our caller unchanged.
 * ORCHESTRATOR: there is nothing to wire for this — savedsearchapi.c includes
 * alertsapi.h and the linker already has alertsapi.c (the core sources are
 * globbed by the Makefile).
 * If some future caller ever needs to validate WITHOUT creating a rule, the
 * correct move is to export alertsapi.c's validate_rule() as
 * `alertsapi_validate_rule()` and have both callers use it — NOT to copy it
 * into a second, weaker validator.
 */
#ifndef JO_SAVEDSEARCHAPI_H
#define JO_SAVEDSEARCHAPI_H
#include "db.h"
#include "tenantapi.h"

/* ── REQUIRED DDL (orchestrator: append verbatim to core/schema.sql) ────────
 * Two new tables + two indexes. No ALTER TABLE, no new columns on existing
 * tables, so no db.c ensure_column() migration is needed.
 *
 *   CREATE TABLE IF NOT EXISTS saved_searches (
 *     id TEXT PRIMARY KEY, tenant_id TEXT NOT NULL, user_id TEXT NOT NULL,
 *     name TEXT, kind TEXT NOT NULL CHECK(kind IN ('intel','osint','entity','breach','map')),
 *     params_json TEXT NOT NULL, pinned INTEGER NOT NULL DEFAULT 0,
 *     created_at TEXT NOT NULL DEFAULT (datetime('now')),
 *     last_run_at TEXT, run_count INTEGER NOT NULL DEFAULT 0);
 *   CREATE INDEX IF NOT EXISTS idx_saved_searches_owner
 *     ON saved_searches(tenant_id, user_id, pinned DESC, created_at DESC);
 *
 *   CREATE TABLE IF NOT EXISTS search_history (
 *     id INTEGER PRIMARY KEY AUTOINCREMENT, tenant_id TEXT NOT NULL, user_id TEXT NOT NULL,
 *     kind TEXT NOT NULL, params_json TEXT NOT NULL, result_count INTEGER,
 *     ts TEXT NOT NULL DEFAULT (datetime('now')));
 *   CREATE INDEX IF NOT EXISTS idx_search_history_owner
 *     ON search_history(tenant_id, user_id, ts DESC);
 *
 * Note the owner indexes lead with (tenant_id, user_id): every read path in
 * this module filters on both, so the index and the privacy rule agree.
 * search_history deliberately has no FK to saved_searches — history is a
 * record of what ran, and deleting the bookmark must not rewrite the trail.
 */

/* Limits, exposed so clients and tests can agree with the server. */
#define SS_PARAMS_MAX     16384   /* bytes of compact params_json            */
#define SS_NAME_MAX       200     /* chars, trimmed                          */
#define SS_HISTORY_KEEP   1000    /* newest rows kept per (tenant,user)      */
#define PL_TOKEN_MAX      4096    /* chars of "v1.<b64url>" accepted         */
#define PL_STATE_MAX      3072    /* bytes of compact state JSON encoded     */

/* ── /api/saved-searches[...] ───────────────────────────────────────────────
 *   GET    /api/saved-searches?kind=&limit=   list MINE (pinned first)
 *   POST   /api/saved-searches                create  {name?,kind,params|params_json,pinned?}
 *   GET    /api/saved-searches/:id            fetch mine
 *   PATCH  /api/saved-searches/:id            {name?,params?,pinned?} — `kind`
 *                                             is immutable (see .c for why)
 *   DELETE /api/saved-searches/:id            204
 *   POST   /api/saved-searches/:id/run        bump last_run_at/run_count and
 *                                             append history. Does NOT execute
 *                                             the search — see below.
 *   POST   /api/saved-searches/:id/to-alert   {channels:[...],name?,...} → an
 *                                             alert_rules row. analyst+.
 *
 * `seg` = the saved-search id, "" for the collection (never NULL).
 * `act` = "run" | "to-alert", "" for the row itself (never NULL).
 * `qs`  = raw query string, may be NULL. `body` = raw body, may be NULL.
 * Sets *status; returns a malloc'd JSON body, or NULL. NULL + 204 is an empty
 * success (DELETE); NULL + anything else means the caller emits a generic
 * error of that status. Caller frees.
 *
 * A row is only ever visible to the user who created it, in the tenant it was
 * created in. Someone else's id returns 404, not 403 — a 403 would confirm the
 * row exists, which is itself a leak about what a colleague saved.
 *
 * /run does NOT execute the query. It is bookkeeping plus a handoff: it
 * returns the saved search (including `params`) so the client re-issues the
 * real request against /api/intel/items, /api/search/analyze, /api/entities,
 * the breach reader or the map layer endpoint, exactly as if the analyst had
 * typed the filters. Executing here would mean this module reimplementing five
 * query surfaces and their permission checks — five more places to get a
 * tenant predicate wrong. The response carries meta.executed=false and
 * meta.execute_hint so no client mistakes the bookkeeping for results.
 */
char *savedsearchapi(db_handle *db, const tenant_ctx *t, const char *method,
                     const char *seg, const char *act, const char *qs,
                     const char *body, int *status);

/* ── /api/search-history ────────────────────────────────────────────────────
 *   GET    /api/search-history?kind=&limit=   newest-first, MINE ONLY
 *   DELETE /api/search-history                clear MINE ONLY → {"ok":true,"deleted":n}
 *
 * There is no ?user_id=, no ?all=1 and no admin variant, and adding one would
 * defeat the point of the table (see the privacy note at the top). limit
 * defaults to 50 and clamps to 1..200. Sets *status; malloc'd; caller frees. */
char *searchhistoryapi(db_handle *db, const tenant_ctx *t, const char *method,
                       const char *qs, int *status);

/* Append one history row for (tenant,user) and prune to the newest
 * SS_HISTORY_KEEP. `kind` must be one of the five kinds (anything else is
 * ignored); `params_json` must be a JSON object serialization within
 * SS_PARAMS_MAX (anything else is ignored). `result_count` < 0 stores NULL.
 *
 * Fails silently, exactly like audit_write(): a history row must never break
 * the search it describes.
 *
 * ORCHESTRATOR (optional, not required for item 38): to make history reflect
 * every search rather than only saved-search runs, call this from the existing
 * search handlers in httpd.c after a successful response, e.g. in the
 * /api/intel/items block:
 *     search_history_record(g_db, tc.tenant_id, tc.user_id, "intel",
 *                           params_json, (long)returned_rows);
 * It is a plain void call with no ordering constraints. */
void search_history_record(db_handle *db, const char *tenant_id,
                           const char *user_id, const char *kind,
                           const char *params_json, long result_count);

/* ── /api/permalink[...] ────────────────────────────────────────────────────
 *   GET  /api/permalink/:token   → {"data":{<canonical state>}}
 *   POST /api/permalink          → {"data":{"token":"v1...","state":{...}}}
 *        body: the state object, or {"state":{...}}
 *
 * Stateless by design: no db_handle, no tenant_ctx, no I/O. See the
 * VIEW-STATE-ONLY note at the top of this header — the absent parameters are
 * the enforcement. Sets *status; malloc'd; caller frees.
 * 400 invalid_permalink (unparseable/unknown version/oversized token),
 * 400 state_too_large, 400 invalid_state, 405 method_not_allowed. */
char *permalinkapi(const char *method, const char *token, const char *body,
                   int *status);

/* The codec itself, exported so any other module (export links, alert
 * notification deep-links, the iOS bridge) mints tokens the same way instead
 * of inventing a second format.
 *
 * Canonical state object (all keys optional; decode always emits all of them,
 * using null / [] for absent, so clients get one stable shape):
 *
 *   { "kind":      "intel"|"osint"|"entity"|"breach"|"map" | null,
 *     "params":    {...} | null,                 (search params, ≤SS_PARAMS_MAX)
 *     "map":       {"lat":,"lon":,"zoom":,"bearing":,"pitch":} | null,
 *     "layers":    ["…"],                        (≤32 names, ≤64 chars each)
 *     "time":      {"from":"…","to":"…"} | null, (≤40 chars each)
 *     "entity_id": "…" | null,                   (≤128 chars)
 *     "case_id":   "…" | null }                  (≤128 chars)
 *
 * Wire form is "v1." + base64url(compact JSON with one-letter keys). The
 * version prefix is what lets v2 add fields without a v1 link decoding into
 * a plausible-but-wrong view; an unknown prefix is rejected outright rather
 * than best-effort parsed. Both functions accept either the long keys above
 * or the short wire keys as input, so a token round-trips byte-stably.
 *
 * permalink_encode: NULL if the input is not a JSON object or the compact
 * state exceeds PL_STATE_MAX. permalink_decode: NULL for any malformed,
 * over-long, wrong-version or non-object token. Both malloc'd; caller frees. */
char *permalink_encode(const char *state_json);
char *permalink_decode(const char *token);

#endif

/* ── WIRING (orchestrator: add to core/httpd.c) ──────────────────────────────
 *
 * (a) with the other includes at the top of httpd.c:
 *
 *       #include "savedsearchapi.h"
 *
 * (b) three blocks, inserted immediately AFTER the existing `/api/annotations`
 *     block (the one ending `reply_json(c, status, body); free(body); return; }`)
 *     and BEFORE the `/api/members` block. Same tenant-resolution and
 *     body-read shape as its neighbours; note "/api/saved-searches/" is 20
 *     bytes and "/api/permalink/" is 15:
 *
 *   -- 1. saved searches --------------------------------------------------
 *   if (eq(u, "/api/saved-searches") || starts(u, "/api/saved-searches/")) {
 *     struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
 *     char xtid[128] = {0};
 *     if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
 *     tenant_ctx tc;
 *     int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
 *     if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
 *     if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
 *
 *     char sid[160] = {0}, act[64] = {0};
 *     if (u.len > 20) {                          |* after "/api/saved-searches/" *|
 *       char rest[256] = {0};
 *       size_t rl = u.len - 20; if (rl >= sizeof rest) rl = sizeof rest - 1;
 *       memcpy(rest, u.buf + 20, rl);
 *       char *sl = strchr(rest, '/');
 *       if (sl) { *sl = 0; snprintf(act, sizeof act, "%s", sl + 1); }
 *       mg_url_decode(rest, strlen(rest), sid, sizeof sid, 0);
 *     }
 *     char meth[12] = {0};
 *     size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
 *     memcpy(meth, hm->method.buf, ml);
 *     char *bdy = NULL;
 *     if (hm->body.len) { bdy = malloc(hm->body.len + 1);
 *       memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
 *     char qsb[512] = {0};
 *     if (hm->query.len && hm->query.len < sizeof qsb)
 *       memcpy(qsb, hm->query.buf, hm->query.len);
 *
 *     int status = 200;
 *     char *body = savedsearchapi(g_db, &tc, meth, sid, act, qsb, bdy, &status);
 *     free(bdy);
 *     if (!body && status == 204) { mg_http_reply(c, 204, "", ""); return; }
 *     if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
 *     reply_json(c, status, body); free(body); return;
 *   }
 *
 *   -- 2. search history (collection only, no path params) ------------------
 *   if (eq(u, "/api/search-history")) {
 *     struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
 *     char xtid[128] = {0};
 *     if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
 *     tenant_ctx tc;
 *     int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
 *     if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
 *     if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
 *     char meth[12] = {0};
 *     size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
 *     memcpy(meth, hm->method.buf, ml);
 *     char qsb[512] = {0};
 *     if (hm->query.len && hm->query.len < sizeof qsb)
 *       memcpy(qsb, hm->query.buf, hm->query.len);
 *     int status = 200;
 *     char *body = searchhistoryapi(g_db, &tc, meth, qsb, &status);
 *     if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
 *     reply_json(c, status, body); free(body); return;
 *   }
 *
 *   -- 3. permalink (no tenant_resolve: the codec is stateless) -------------
 *   if (eq(u, "/api/permalink") || starts(u, "/api/permalink/")) {
 *     static char tok[PL_TOKEN_MAX + 8];          |* 4 KB — too big for stack *|
 *     tok[0] = 0;
 *     if (u.len > 15) {                           |* after "/api/permalink/" *|
 *       size_t rl = u.len - 15;
 *       if (rl <= PL_TOKEN_MAX)
 *         mg_url_decode(u.buf + 15, rl, tok, sizeof tok, 0);
 *       else tok[0] = 0;                          |* over-long → 400 below *|
 *     }
 *     char meth[12] = {0};
 *     size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
 *     memcpy(meth, hm->method.buf, ml);
 *     char *bdy = NULL;
 *     if (hm->body.len) { bdy = malloc(hm->body.len + 1);
 *       memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
 *     int status = 200;
 *     char *body = permalinkapi(meth, tok, bdy, &status);
 *     free(bdy);
 *     if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
 *     reply_json(c, status, body); free(body); return;
 *   }
 *
 * `static char tok[]` is safe here: httpd.c's event handler is single-threaded
 * (one mongoose loop) and the buffer is consumed before the call returns. Keep
 * the permalink block INSIDE the authenticated region with its neighbours —
 * not because the codec needs a session (it does not), but so an unauthenticated
 * scanner cannot use it as a free JSON parser.
 *
 * No main.c change: there is no worker. No Makefile change: the core sources
 * are globbed. schema.sql: the two CREATE blocks above.
 */
