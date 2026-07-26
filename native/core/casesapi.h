/* core/casesapi.h — Roadmap 14: Cases. The analyst workspace spine.
 *
 * A case is the unit of work an analyst actually reports on: a named folder
 * with a roster, a pinned evidence set and an immutable history. Everything
 * later in the roadmap (reports, exports, sharing, review queues) hangs off
 * these four tables, so the contract here is deliberately conservative.
 *
 * Three design decisions are load-bearing and are NOT incidental:
 *
 * 1. snapshot_json is the whole point of pinning. The corpora a case cites are
 *    mutable underneath it: scraped intel_items get re-fetched and rewritten,
 *    breach shards get re-ingested, entities get merged. A pin that stores only
 *    (ref_type, ref_id) degrades into "deleted item" in a report six weeks
 *    later, which is worse than useless — it destroys the evidence trail. So
 *    every pin stores a denormalized display copy at pin time. If the caller
 *    does not supply one we synthesize it server-side by reading the referenced
 *    row. Snapshots are written once and never refreshed: they are evidence of
 *    what the analyst saw, not a cache of what the row says now.
 *
 *    Snapshot envelope (uniform whether client- or server-supplied, so the UI
 *    has exactly one shape to render):
 *      {"ref_type":"intel_item","ref_id":"<uid>","origin":"server"|"client",
 *       "resolved":true|false,"captured_at":"<ISO8601>","data":{...}|null}
 *    `resolved:false` + `data:null` means the referenced row could not be read
 *    at pin time — the pin is still recorded, and the UI can show it as a bare
 *    reference rather than silently dropping it.
 *
 *    Server-side synthesis covers intel_item (intelapi_item_by_uid), entity
 *    (direct read of `entities`) and breach_item (breach_adapter_item_by_uid,
 *    which is also the only path that guarantees leaked secrets stay redacted —
 *    a snapshot must never become a plaintext-credential side channel).
 *    feature / camera / search_run have no single canonical row reachable from
 *    this module, so they synthesize an unresolved envelope; clients that pin
 *    them are expected to pass their own snapshot_json.
 *
 * 2. Tenant isolation is enforced on `cases` and nowhere else, because nowhere
 *    else is needed and one enforcement point is auditable. `cases.tenant_id`
 *    is the only tenant column in this subsystem; case_members / case_items /
 *    case_activity are reachable only through a case id. EVERY entry into those
 *    three tables therefore goes through case_lookup(), which resolves a case
 *    id ONLY together with `tenant_id = ?`. There is no code path in this file
 *    that touches a child table with a case id that has not been through it.
 *    (The referenced corpora — intel_items, entities, breach_items — are shared
 *    across tenants by design: intel_items.tenant_id defaults to 'legacy' and
 *    is not used as a filter anywhere in the codebase. Snapshot reads are
 *    therefore deliberately not tenant-filtered; only `entities`, which has a
 *    real per-tenant column, is scoped to (tenant_id IS NULL OR tenant_id=?).)
 *
 * 3. The activity feed is written by the server, not by the client. create,
 *    pin, unpin, status change, field edit and roster change all append a
 *    case_activity row alongside the mutation. A history that only contains
 *    what users chose to type is not a history. Client-authored rows are
 *    exactly the kind='comment' ones.
 *
 * CASCADE DECISION (per the brief): db.c:87 does set `PRAGMA foreign_keys=ON`,
 * so the ON DELETE CASCADE clauses below would in fact fire on the server
 * handle. We nevertheless delete children EXPLICITLY, ordered, inside a single
 * transaction. Rationale: the pragma is per-connection, not per-database, and
 * it is set exactly once in db_open(). Any other connection to japanmap.db —
 * a maintenance worker, a CLI tool, the sqlite3 shell — silently defaults to
 * foreign_keys=OFF and would orphan case_items/case_activity/case_members
 * rows forever, with no error and no way to find them (they are keyed only by
 * a case id that no longer exists). Explicit deletes are correct under both
 * settings and cost one extra statement on a rare operation.
 *
 * ── REQUIRED DDL (orchestrator: apply verbatim to core/schema.sql) ──────────
 *
 * CREATE TABLE IF NOT EXISTS cases (
 *   id TEXT PRIMARY KEY,
 *   tenant_id TEXT NOT NULL REFERENCES tenants(id) ON DELETE CASCADE,
 *   name TEXT NOT NULL, summary TEXT,
 *   status TEXT NOT NULL DEFAULT 'open' CHECK(status IN ('open','closed','archived')),
 *   priority INTEGER NOT NULL DEFAULT 0,
 *   created_by TEXT, created_at TEXT NOT NULL DEFAULT (datetime('now')),
 *   updated_at TEXT NOT NULL DEFAULT (datetime('now')), closed_at TEXT);
 *
 * CREATE TABLE IF NOT EXISTS case_members (
 *   case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE CASCADE,
 *   user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
 *   role TEXT NOT NULL DEFAULT 'contributor' CHECK(role IN ('lead','contributor','viewer')),
 *   PRIMARY KEY (case_id, user_id));
 *
 * CREATE TABLE IF NOT EXISTS case_items (
 *   case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE CASCADE,
 *   ref_type TEXT NOT NULL, ref_id TEXT NOT NULL,
 *   label TEXT, snapshot_json TEXT,
 *   added_by TEXT, added_at TEXT NOT NULL DEFAULT (datetime('now')),
 *   PRIMARY KEY (case_id, ref_type, ref_id));
 *
 * CREATE TABLE IF NOT EXISTS case_activity (
 *   id INTEGER PRIMARY KEY AUTOINCREMENT,
 *   case_id TEXT NOT NULL REFERENCES cases(id) ON DELETE CASCADE,
 *   actor_id TEXT, kind TEXT NOT NULL,
 *   body TEXT, target_ref TEXT, mentions_json TEXT NOT NULL DEFAULT '[]',
 *   ts TEXT NOT NULL DEFAULT (datetime('now')));
 *
 * CREATE INDEX IF NOT EXISTS idx_cases_tenant_status ON cases(tenant_id, status, updated_at DESC);
 * CREATE INDEX IF NOT EXISTS idx_case_items_ref ON case_items(ref_type, ref_id);
 * CREATE INDEX IF NOT EXISTS idx_case_activity_case ON case_activity(case_id, ts DESC);
 *
 * -- added by this module (not in the brief), both backing keyset scans:
 * CREATE INDEX IF NOT EXISTS idx_cases_tenant_updated ON cases(tenant_id, updated_at DESC, id);
 * CREATE INDEX IF NOT EXISTS idx_case_items_case_added ON case_items(case_id, added_at DESC);
 * CREATE INDEX IF NOT EXISTS idx_case_members_user ON case_members(user_id);
 *
 *   · idx_cases_tenant_updated backs the unfiltered list, whose sort key is
 *     (updated_at DESC, id ASC); idx_cases_tenant_status only helps when
 *     ?status= is present.
 *   · idx_case_items_case_added backs the per-case item page; the brief's
 *     idx_case_items_ref is the reverse lookup ("which cases cite this item?"),
 *     which later roadmap items need but this module's list query cannot use.
 *   · idx_case_members_user backs "cases I am on" (the membership half of the
 *     write-authorization check, and the obvious next screen).
 *
 * ── ROUTES ─────────────────────────────────────────────────────────────────
 *   GET    /api/cases                 list, ?status=, keyset paged
 *   POST   /api/cases                 create {name,summary?,status?,priority?}
 *   GET    /api/cases/:id             detail + item counts by ref_type + roster
 *   PATCH  /api/cases/:id             partial {name,summary,status,priority}
 *   DELETE /api/cases/:id             owner/admin or case lead
 *   GET    /api/cases/:id/items       ?ref_type=, keyset paged
 *   POST   /api/cases/:id/items       pin {ref_type,ref_id,label?,snapshot_json?}
 *   DELETE /api/cases/:id/items       unpin; ref_type+ref_id from body or query
 *   GET    /api/cases/:id/activity    newest first, keyset paged
 *   POST   /api/cases/:id/activity    comment {body,mentions:[user_id]}
 *   GET    /api/cases/:id/members     roster
 *   PUT    /api/cases/:id/members     set roster {members:[{user_id,role}]}
 *
 * ── AUTHORIZATION ──────────────────────────────────────────────────────────
 *   read    any tenant member (tenant_resolve already proves membership)
 *   write   tenant role analyst|admin|owner  OR  a row in case_members
 *   roster  tenant role analyst|admin|owner  OR  case_members.role='lead'
 *           (tightened from plain "write": letting a case *viewer* rewrite the
 *           roster would let them promote themselves; the brief's write rule
 *           is about content, not about who may see the case)
 *   delete  tenant role owner|admin          OR  case_members.role='lead'
 *
 * ── AUDIT ──────────────────────────────────────────────────────────────────
 * Every mutation calls audit_write(): case.create, case.update, case.delete,
 * case.item.pin, case.item.unpin, case.activity.create, case.members.set.
 *
 * ── VOCABULARIES ───────────────────────────────────────────────────────────
 *   status     open | closed | archived
 *   priority   integer 0..5 (0 = none … 5 = critical); anything else is 400
 *   ref_type   intel_item | entity | breach_item | feature | camera | search_run
 *   case role  lead | contributor | viewer
 *   activity   created | updated | status_changed | item_pinned | item_unpinned
 *              | members_changed | comment
 *
 * Name is trimmed of ASCII whitespace and limited to 200 BYTES (matching
 * alertsapi's validate_rule, which also counts bytes, not code points).
 * summary and comment bodies are trimmed and capped at 8191 bytes; labels at
 * 511; ref_id at 599 (a breach uid is the longest real reference). Over-length
 * input is a 400, never a silent truncation.
 */
#ifndef JO_CASESAPI_H
#define JO_CASESAPI_H
#include "db.h"
#include "tenantapi.h"

/* Query-string inputs for the whole subtree. NULL/0 == absent. Modelled on
 * intelapi.h's intel_items_query rather than alertsapi's flat (cursor,limit)
 * pair, because DELETE /items takes its selector from the query string too and
 * a growing positional arg list is how tenant predicates get dropped. */
typedef struct {
  const char *status;    /* ?status=   GET /api/cases filter                 */
  const char *ref_type;  /* ?ref_type= GET items filter / DELETE selector    */
  const char *ref_id;    /* ?ref_id=   DELETE items selector (body wins)     */
  const char *cursor;    /* ?cursor=   base64url {"p":sort,"u":uid} keyset   */
  int         limit;     /* ?limit=    <=0 → 50, clamped 1..200              */
} cases_query;

/* One entrypoint for the whole /api/cases subtree.
 *   `seg`  = the case id, "" for the collection      (never NULL)
 *   `act`  = "items" | "activity" | "members", "" for the case itself
 *   `body` = raw request body, may be NULL
 *   `q`    = parsed query string, may be NULL
 * Sets *status and returns a malloc'd JSON body, or NULL. NULL + status 204 is
 * an empty success (DELETE); NULL + any other status means the caller emits a
 * generic error of that status. Caller frees. */
char *casesapi(db_handle *db, const tenant_ctx *t, const char *method,
               const char *seg, const char *act, const char *body,
               const cases_query *q, int *status);

#endif

/* ── WIRING (orchestrator: add to core/httpd.c) ─────────────────────────────
 *
 * (a) with the other includes at the top of httpd.c:
 *
 *       #include "casesapi.h"
 *
 * (b) immediately AFTER the existing `/api/alerts` block (the one ending
 *     `reply_json(c, status, body); free(body); return; }`) and before the
 *     `/api/members` block — same tenant-resolution and body-read shape,
 *     "/api/cases/" is 11 bytes where "/api/alerts/" is 12:
 *
 *   // ---- Roadmap 14: /api/cases subtree (tenant-scoped analyst workspace) ----
 *   if (eq(u, "/api/cases") || starts(u, "/api/cases/")) {
 *     struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
 *     char xtid[128] = {0};
 *     if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
 *     tenant_ctx tc;
 *     int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
 *     if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
 *     if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
 *
 *     char cid[128] = {0}, act[32] = {0};
 *     if (u.len > 11) {                          // after "/api/cases/"
 *       char rest[256] = {0};
 *       size_t rl = u.len - 11; if (rl >= sizeof rest) rl = sizeof rest - 1;
 *       memcpy(rest, u.buf + 11, rl);
 *       char *sl = strchr(rest, '/');
 *       if (sl) { *sl = 0; snprintf(act, sizeof act, "%s", sl + 1); }
 *       mg_url_decode(rest, strlen(rest), cid, sizeof cid, 0);
 *     }
 *     char meth[12] = {0};
 *     size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
 *     memcpy(meth, hm->method.buf, ml);
 *     char *bdy = NULL;
 *     if (hm->body.len) { bdy = malloc(hm->body.len + 1);
 *       memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
 *
 *     char qst[32]={0}, qrt[32]={0}, qri[512]={0}, qcu[512]={0}, qli[16]={0};
 *     mg_http_get_var(&hm->query, "status",   qst, sizeof qst);
 *     mg_http_get_var(&hm->query, "ref_type", qrt, sizeof qrt);
 *     mg_http_get_var(&hm->query, "ref_id",   qri, sizeof qri);
 *     mg_http_get_var(&hm->query, "cursor",   qcu, sizeof qcu);
 *     int hl = mg_http_get_var(&hm->query, "limit", qli, sizeof qli);
 *     cases_query cq;
 *     cq.status   = qst[0] ? qst : NULL;
 *     cq.ref_type = qrt[0] ? qrt : NULL;
 *     cq.ref_id   = qri[0] ? qri : NULL;
 *     cq.cursor   = qcu[0] ? qcu : NULL;
 *     cq.limit    = hl > 0 ? atoi(qli) : 0;
 *
 *     int status = 200;
 *     char *body = casesapi(g_db, &tc, meth, cid, act, bdy, &cq, &status);
 *     free(bdy);
 *     if (!body && status == 204) { mg_http_reply(c, 204, "", ""); return; }
 *     if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
 *     reply_json(c, status, body); free(body); return;
 *   }
 *
 * No main.c change: there is no worker here. No Makefile change: the core
 * sources are globbed. casesapi.c calls intelapi_item_by_uid() and
 * breach_adapter_item_by_uid(), both already linked into the binary.
 */
