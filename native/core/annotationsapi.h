/* core/annotationsapi.h — roadmap 15: analyst notes & annotations, a
 * tenant-scoped CRUD surface over one `annotations` table. Sibling of
 * core/alertsapi.c in shape; the interesting parts are the two rules that
 * make notes *evidence* rather than mutable scratch text:
 *
 *   1. DELETE is a SOFT delete. The row survives with `deleted_at` stamped.
 *      Notes are the human layer of the chain-of-custody work (roadmap 17):
 *      a hard DELETE would let an analyst erase their own reasoning after the
 *      fact and leave the export/attestation path attesting to a record with
 *      a hole in it. There is deliberately NO endpoint that removes a row.
 *      Purge, if it ever exists, belongs to a retention job with its own
 *      audit trail — not to a user-facing verb.
 *
 *   2. Only the AUTHOR may edit a note — not owner, not admin. An audit trail
 *      whose entries can be silently rewritten by a colleague is worthless;
 *      "who wrote this and did they later change it" must stay answerable
 *      from `author_id` + `updated_at`. owner/admin may *delete* (tombstone)
 *      any note, because moderation of visible content is a real need and a
 *      tombstone is honest about what happened. Editing someone else's note
 *      is 403, always.
 *
 * body_md is markdown and is stored RAW. We do not sanitize, escape, or
 * render it in C. Rendering is the client's job and the client is the only
 * layer that knows its own sink (SwiftUI AttributedString vs. a web view vs.
 * a PDF export), so it is also the only layer that can choose the right
 * escaping. Sanitizing here would corrupt legitimate note text (analysts
 * paste payloads, selectors, and raw HTML *as evidence*) while providing no
 * guarantee to any consumer. Please do not "fix" this by adding a sanitizer.
 *
 * ── Required DDL (orchestrator: append verbatim to core/schema.sql) ───────
 *
 *   CREATE TABLE IF NOT EXISTS annotations (
 *     id TEXT PRIMARY KEY,
 *     tenant_id TEXT NOT NULL,
 *     case_id TEXT REFERENCES cases(id) ON DELETE CASCADE,
 *     ref_type TEXT NOT NULL, ref_id TEXT NOT NULL,
 *     body_md TEXT NOT NULL, author_id TEXT,
 *     created_at TEXT NOT NULL DEFAULT (datetime('now')),
 *     updated_at TEXT, deleted_at TEXT);
 *   CREATE INDEX IF NOT EXISTS idx_annotations_ref
 *     ON annotations(ref_type, ref_id, created_at DESC);
 *   CREATE INDEX IF NOT EXISTS idx_annotations_tenant
 *     ON annotations(tenant_id, created_at DESC);
 *
 * Both indexes are lookup-shaped, not tenancy-shaped: idx_annotations_ref
 * serves "show me the notes on this intel item" (the hot read), and the
 * tenant_id predicate that EVERY query carries is satisfied by
 * idx_annotations_tenant for the unfiltered list. `cases(id)` is assumed to
 * exist (owned by the cases module); the ON DELETE CASCADE is intentional —
 * a note is scoped to its case, and deleting a case is itself an audited,
 * deliberate act.
 * ─────────────────────────────────────────────────────────────────────────
 */
#ifndef JO_ANNOTATIONSAPI_H
#define JO_ANNOTATIONSAPI_H
#include "db.h"
#include "tenantapi.h"

/* The `ref_type` vocabulary — exactly the set `case_items` uses, so a note
 * and a case item can point at the same thing with the same words. Exported
 * so the cases module can share this one validator instead of drifting a
 * second copy: intel_item | entity | breach_item | feature | camera |
 * search_run. Returns 1 if `s` is one of them, 0 otherwise (NULL → 0). */
int annotations_ref_type_valid(const char *s);

/* The vocabulary itself, so callers that need to enumerate it (case item
 * counts, UI chip sets) do not hand-maintain a second copy that drifts.
 * The returned array is static; *count receives its length. */
const char *const *annotations_ref_types(int *count);

/* One entrypoint for the whole /api/annotations subtree.
 *
 *   `t`            resolved tenant context (tenant_id / user_id / role).
 *   `method`       HTTP verb.
 *   `seg`          the single path param after /api/annotations/ — the
 *                  annotation id, or "" (or NULL) for the collection.
 *   `query_string` raw, undecoded querystring (may be NULL/empty).
 *   `body`         raw request body (may be NULL).
 *
 * Routes:
 *   GET    /api/annotations?ref_type=&ref_id=&case_id=&limit=&cursor=
 *                              &include_deleted=   → 200 {data,page,meta}
 *   POST   /api/annotations    {ref_type,ref_id,body_md,case_id?} → 201 {data}
 *   GET    /api/annotations/:id                    → 200 {data}
 *   PATCH  /api/annotations/:id  {body_md}         → 200 {data}
 *   DELETE /api/annotations/:id                    → 200 {data} (tombstone)
 *
 * DELETE answers 200 with the tombstoned row rather than 204: the caller
 * needs `deleted_at` to render "deleted by X at T", and an empty 204 would
 * imply the row is gone — which is exactly the wrong mental model here.
 *
 * Authorization: any tenant member reads; analyst+ creates; author-only
 * edits; author or owner/admin deletes. `include_deleted=1` is honoured for
 * analyst+ and silently ignored for viewer (the effective value is echoed
 * back in meta.filters.include_deleted).
 *
 * Sets *status and returns a malloc'd JSON body (caller frees). Returns NULL
 * only on an unrecoverable internal failure — caller replies 500. There is
 * no NULL-with-204 path in this module. */
char *annotationsapi(db_handle *db, const tenant_ctx *t, const char *method,
                     const char *seg, const char *query_string,
                     const char *body, int *status);

/* ── Wiring (orchestrator: apply to core/httpd.c) ─────────────────────────
 *
 * 1. Add near the other core includes at the top of httpd.c:
 *
 *      #include "annotationsapi.h"
 *
 * 2. Add this block inside fn(), alongside the /api/alerts block. Prefix
 *    lengths: "/api/annotations" is 16 chars, "/api/annotations/" is 17.
 *
 *    // ---- Roadmap 15: /api/annotations[/:id] (tenant-scoped notes) ----
 *    if (eq(u, "/api/annotations") || starts(u, "/api/annotations/")) {
 *      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
 *      char xtid[128] = {0};
 *      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
 *      tenant_ctx tc;
 *      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
 *      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
 *      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
 *
 *      char aid[128] = {0};
 *      if (u.len > 17) {                    // after "/api/annotations/"
 *        char rest[256] = {0};
 *        size_t rl = u.len - 17; if (rl >= sizeof rest) rl = sizeof rest - 1;
 *        memcpy(rest, u.buf + 17, rl);
 *        char *sl = strchr(rest, '/'); if (sl) *sl = 0;   // no sub-actions
 *        mg_url_decode(rest, strlen(rest), aid, sizeof aid, 0);
 *      }
 *      char meth[12] = {0};
 *      size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
 *      memcpy(meth, hm->method.buf, ml);
 *      char qsb[768] = {0};
 *      size_t ql = hm->query.len < sizeof qsb - 1 ? hm->query.len : sizeof qsb - 1;
 *      if (ql) memcpy(qsb, hm->query.buf, ql);
 *      char *bdy = NULL;
 *      if (hm->body.len) { bdy = malloc(hm->body.len + 1);
 *        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
 *      int status = 200;
 *      char *body = annotationsapi(g_db, &tc, meth, aid, qsb, bdy, &status);
 *      free(bdy);
 *      if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
 *      reply_json(c, status, body); free(body); return;
 *    }
 *
 *    Note: this module parses its own querystring (it needs six params, two
 *    of which are free-text), so unlike the alerts block nothing is pulled
 *    out with mg_http_get_var — the raw query is passed straight through.
 *    body_md accepts up to 64 KB, so the server's max body size must stay
 *    comfortably above that.
 * ───────────────────────────────────────────────────────────────────────── */

#endif
