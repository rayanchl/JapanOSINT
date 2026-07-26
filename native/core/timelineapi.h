/* core/timelineapi.h — roadmap 18 (backend half): GET /api/timeline, one
 * temporally sorted view over the three streams the platform actually
 * produces: collected intel (`intel_items`), extracted entity mentions
 * (`entity_mentions`) and fired alerts (`alert_events`).
 *
 * WHY a dedicated module rather than three client-side fetches: the merge has
 * to happen where the sort key lives. The three tables store timestamps in two
 * different textual shapes (intel_items writes Node ISO
 * "2026-07-26T15:15:33.827Z"; entity_mentions/alert_events default to SQLite's
 * datetime('now') "2026-07-21 14:19:39"), so a naive lexicographic merge in
 * the client is wrong — 'T' (0x54) sorts after ' ' (0x20). Every timestamp is
 * therefore normalised in SQL to a single fixed-width form,
 * strftime('%Y-%m-%dT%H:%M:%SZ', ts), before it is compared, bucketed, sorted
 * or handed out as a cursor. A row whose timestamp SQLite cannot parse
 * normalises to NULL and is silently excluded (it has no place on a timeline).
 *
 * ── Tenant isolation (read this before changing any SQL) ────────────────────
 * intel_items and alert_events carry `tenant_id` and are filtered on it
 * directly. `entity_mentions` has NO tenant column, so it is scoped by an
 * INNER JOIN to its parent item:
 *     JOIN intel_items i ON i.uid = em.item_uid AND i.tenant_id = ?
 * The join is deliberately INNER: a mention whose item_uid does not resolve to
 * a row in this tenant's intel_items (orphaned mention, item deleted, or item
 * owned by another tenant) is EXCLUDED, never emitted. Under-reporting a
 * timeline is a cosmetic bug; leaking another tenant's mention is not.
 * For the same reason alert_events joins its item with
 * `LEFT JOIN intel_items i ON i.uid = e.item_uid AND i.tenant_id = ?` — the
 * event itself is already tenant-scoped, but its decorations (title,
 * source_id, lat/lon) must not be borrowed from another tenant's item; a
 * cross-tenant uid collision yields NULL decorations instead.
 *
 * ── Request ─────────────────────────────────────────────────────────────────
 * GET /api/timeline?since&until&bucket=hour|day&source&bbox&case_id&limit&cursor
 *   since,until  any time string SQLite accepts. Default window = last 7 days
 *                ending now. A window wider than 366 days is rejected 400
 *                (`window_too_wide`): this DB is ~2.9 GB and an unbounded scan
 *                is a DoS against our own server.
 *   bucket       "hour" | "day" (default "day"); histogram granularity.
 *   source       source_id equality, applied to the *item* (i.source_id) for
 *                all three streams so the filter means exactly one thing.
 *   bbox         "w,s,e,n"; keeps only events whose item has coordinates
 *                inside the box (so alert events with no resolvable item drop
 *                out, by construction).
 *   case_id      restrict to uids pinned in `case_items` (roadmap 17).
 *   limit        default 100, clamped 1..500.
 *   cursor       base64url {"p":<normalised ts>,"u":<event key>} — the SAME
 *                encoding intelapi.c emits, so clients keep one cursor parser.
 *                "u" holds the opaque per-event `key` (see below), not a bare
 *                item uid, because item uids are not unique across the three
 *                streams and a keyset tiebreaker must be.
 *
 * ── Response (200) ──────────────────────────────────────────────────────────
 * {"data":{"buckets":[...],"events":[...]},"page":{...},"meta":{...}}
 * `data` is an object rather than the usual bare list because the endpoint
 * genuinely returns two first-class results; burying the histogram in `meta`
 * would misrepresent it.
 *   buckets  [{"t":"2026-07-26T00:00:00Z","intel":42,"mentions":7,"alerts":1}]
 *            oldest→newest (left-to-right histogram order), covering the WHOLE
 *            window — buckets ignore `cursor` and `limit` by design, otherwise
 *            the strip would change shape as the user pages.
 *   events   newest-first merged page. Each event:
 *            {"kind":"intel"|"mention"|"alert","ts":"…Z","uid":<item uid>,
 *             "title":str|null,"source_id":str|null,"lat":num|null,
 *             "lon":num|null,"ref":str|null,"key":str}
 *            `ref` is the stream's secondary subject (entity_id for mentions,
 *            rule_id for alerts, null for intel). `key` is the opaque unique
 *            sort/cursor key ("i:"|"m:"|"a:" prefixed); clients should treat
 *            it as opaque and navigate with `uid`.
 *   page     {"next_cursor":str|null,"limit":int,"total":null}
 *   meta     {"fetched_at","bucket","since","until","totals":{intel,mentions,
 *             alerts},"filters":{source,bbox,case_id},"notes":[str,…]}
 *
 * Errors: 400 {"error":"invalid_bucket"|"invalid_limit"|"invalid_bbox"|
 *              "invalid_time"|"invalid_window"|"window_too_wide"},
 *         404 {"error":"not_found"} (case_id not owned by this tenant),
 *         500 {"error":"server_error"}.
 *
 * ── Degradation when `case_items` does not exist yet ────────────────────────
 * roadmap 17 may not have landed in a given deployment. `case_id` is only
 * honoured after a sqlite_master lookup:
 *   - `cases` present  → the case must belong to the caller's tenant, else 404.
 *   - `cases` absent   → ownership cannot be checked; the filter still runs and
 *                        every stream stays tenant-filtered, so this cannot
 *                        leak. meta.notes gets "cases_table_missing".
 *   - `case_items` absent → the request is answered 200 with EMPTY buckets and
 *                        events plus meta.notes "case_items_table_missing".
 *                        Ignoring the filter and returning the tenant's whole
 *                        timeline would silently answer a different question.
 *
 * ── DDL for the orchestrator (all optional; the endpoint is correct without
 *    them, just slower — no new tables or columns are required) ──────────────
 *   CREATE INDEX IF NOT EXISTS idx_alert_events_tenant_ts
 *     ON alert_events(tenant_id, matched_at DESC);
 *   CREATE INDEX IF NOT EXISTS idx_em_created
 *     ON entity_mentions(created_at DESC);
 *   CREATE INDEX IF NOT EXISTS idx_intel_items_tenant_tsnorm
 *     ON intel_items(tenant_id,
 *                    strftime('%Y-%m-%dT%H:%M:%SZ',
 *                             COALESCE(published_at, fetched_at)) DESC, uid);
 * The first two are plain indexes; alert_events currently only has
 * (rule_id, matched_at) and idx_em_entity is (entity_id, created_at), neither
 * of which serves a tenant-wide time scan. The third is an expression index
 * whose expression is byte-identical to the one this module orders and ranges
 * on, which is what lets the planner use it (accepted by the bundled SQLite
 * 3.49.2; verified). Existing indexes already exploited:
 * idx_intel_items_tenant_source (source filter), idx_intel_items_geom (bbox),
 * idx_em_item (the entity_mentions → intel_items join).
 *
 * ── httpd.c wiring (orchestrator) ───────────────────────────────────────────
 * Add `#include "timelineapi.h"` next to the other core API includes, then
 * place this block inside the `starts(u, "/api/")` gate, immediately after the
 * `/api/alerts` block it mirrors:
 *
 *   ---- roadmap 18: GET /api/timeline (tenant-scoped unified timeline) ----
 *   if (eq(u, "/api/timeline")) {
 *     if (mg_strcmp(hm->method, mg_str("GET")) != 0) {
 *       reply_json(c, 405, "{\"error\":\"method_not_allowed\"}"); return; }
 *     struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
 *     char xtid[128] = {0};
 *     if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
 *     tenant_ctx tc;
 *     int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
 *     if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
 *     if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
 *     char qs[1024] = {0};
 *     size_t ql = hm->query.len < sizeof qs - 1 ? hm->query.len : sizeof qs - 1;
 *     memcpy(qs, hm->query.buf, ql);
 *     int status = 200;
 *     char *body = timelineapi_query(g_db, &tc, qs, &status);
 *     if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
 *     reply_json(c, status, body); free(body); return;
 *   }
 *
 * Read-only endpoint: no role check beyond tenant membership (a `viewer` may
 * read the timeline) and nothing to audit.
 */
#ifndef JO_TIMELINEAPI_H
#define JO_TIMELINEAPI_H
#include "db.h"
#include "tenantapi.h"

/* GET /api/timeline. `query_string` is the raw, still-percent-encoded query
 * (mongoose `hm->query`); may be NULL or "". Returns a malloc'd JSON body the
 * caller frees, and sets *status to the HTTP code (200/400/404/500). Returns
 * NULL only on allocation failure, in which case *status is 500 and the caller
 * replies {"error":"server_error"}. */
char *timelineapi_query(db_handle *db, const tenant_ctx *t,
                        const char *query_string, int *status);

#endif
