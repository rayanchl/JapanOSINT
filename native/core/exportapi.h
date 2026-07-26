/* core/exportapi.h — roadmap item 7: ONE generic, streaming row exporter for
 * GET /api/export/:kind?format=csv|geojson|json&<the endpoint's own filters>.
 *
 * WHY ONE EXPORTER
 * Every per-surface exporter that has ever been written in this codebase's
 * lineage drifted from the surface it exported: the list endpoint grew a
 * filter, the exporter did not, and "export what I'm looking at" silently
 * stopped meaning that. So there is exactly one export path, and for
 * kind=intel it consumes `intel_items_query` from intelapi.h VERBATIM — the
 * same struct /api/intel/items fills, parsed from the same query params by the
 * same mg_http_get_var() calls. There is deliberately no parallel export
 * filter vocabulary to drift from.
 *
 * WHY CALLBACK-DRIVEN OUTPUT
 * The database is ~2.9 GB. Every other handler in this codebase returns a
 * fully-built malloc'd JSON string; an export cannot. export_run() iterates a
 * prepared statement and hands the caller bounded ~64 KB batches through
 * `export_write_fn`, so peak allocation is one batch plus one row, regardless
 * of result-set size. A non-zero return from the writer (client gone) aborts
 * the walk cleanly — statement finalized, audit row still written.
 *
 *   >>> MONGOOSE CONSTRAINT — READ BEFORE WIRING <<<
 *   mg_http_write_chunk() → mg_send() → mg_iobuf_add(&c->send, ...) for TCP
 *   connections (third_party/mongoose.c:12330). It APPENDS to the connection's
 *   in-memory send buffer; that buffer is only drained by the event loop's
 *   write handler. Because MG_EV_HTTP_MSG dispatch is synchronous, an export
 *   driven entirely inside the handler accumulates the WHOLE response in
 *   c->send before a single byte reaches the socket.
 *   Consequence: this module's memory is bounded (one 64 KB batch), but the
 *   HTTP layer's is bounded by the RESPONSE size, not by the database size.
 *   With the plan row caps below that ceiling is ~a few hundred MB worst case
 *   on `team`, and genuinely unbounded on `enterprise`.
 *   The wiring in "HTTPD.C WIRING" below is correct and is what should land
 *   now; `export_mg_write` additionally short-circuits on c->is_closing /
 *   c->is_draining so a disconnected client stops the walk immediately. To get
 *   true socket-paced backpressure the orchestrator must either (a) run
 *   export_run() on a detached thread and feed batches back through
 *   mg_wakeup() (the pattern already used by suggest_thread() in httpd.c), or
 *   (b) make export_mg_write() return non-zero while c->send.len exceeds a
 *   high-water mark and resume from MG_EV_WRITE. Both are httpd.c concerns and
 *   are deliberately outside this file. This is stated plainly rather than
 *   pretended away.
 *
 * REQUIRED DDL
 *   NONE. This module creates no tables and adds no columns. It reads
 *   intel_items / entities / breach_items+breach_meta / alert_events+
 *   alert_rules / cases, and writes only through audit_write() into the
 *   existing audit_events table.
 *
 * OUT OF SCOPE
 *   PDF. A row exporter is not a report builder; PDF is roadmap item 16.
 */
#ifndef JO_EXPORTAPI_H
#define JO_EXPORTAPI_H
#include "db.h"
#include "tenantapi.h"
#include <stddef.h>

/* ── Governance ───────────────────────────────────────────────────────────
 *
 * 1. ROW CAP BY PLAN (tenant_ctx.plan), enforced in SQL as LIMIT cap+1 so
 *    truncation is *detected*, never guessed:
 *        free        1,000
 *        pro        50,000
 *        team      250,000
 *        enterprise unlimited
 *    An unknown/empty plan is treated as `free` — the safe direction.
 *    Truncation is NEVER silent. It surfaces in all three formats:
 *      csv     → a final `# truncated=true; ...` comment row (a deliberate
 *                RFC 4180 deviation: 4180 has no comment syntax, but a
 *                silently short file is a worse failure than a row Excel
 *                shows as text)
 *      json    → meta.export.truncated = true
 *      geojson → properties.truncated  = true
 *    …and in the audit row's payload.
 *
 * 2. AUDIT. Every export writes exactly one audit_events row via
 *    audit_write(), action "export.run", target "<kind>/<format>", payload
 *    {kind,format,plan,row_cap,filters,rows,truncated,skipped_no_geometry,
 *    aborted}. It is written AFTER the walk so `rows` is the true emitted
 *    count, and it is written even when the client disconnects mid-stream
 *    (aborted=true) — a half-delivered export is still an egress event.
 *
 * 3. TENANT ISOLATION, per kind:
 *      intel        intel_items.tenant_id IN (<tenant>, 'legacy').
 *                   'legacy' is the shared public-collector corpus (see
 *                   core/intel.c:136 and the schema default); it is not
 *                   another tenant's data. NOTE: /api/intel/items applies no
 *                   tenant predicate at all today — this export is STRICTER
 *                   than the endpoint it mirrors, on purpose. Do not "fix"
 *                   the divergence by loosening this side.
 *      entities     entities.tenant_id IS NULL OR = <tenant> (shared graph).
 *      alert_events alert_events.tenant_id = <tenant>. Strict.
 *      case         cases.tenant_id = <tenant>. Strict; if the table has no
 *                   tenant_id column the kind is REFUSED (501), because
 *                   isolation cannot be guaranteed.
 *      breach       breach_items is a GLOBAL corpus with no tenant column, so
 *                   there is nothing to isolate — but it is the one kind that
 *                   could become a secret-exfiltration channel, so:
 *                     · `?source=<breach_id>` is MANDATORY (400 without it).
 *                       breach_adapter.h's rule is that breach records are
 *                       reachable only per-source, never as an unfiltered
 *                       feed; a bulk export MUST NOT become the loophole.
 *                     · the projection is the redacted adapter field set only
 *                       (uid, source_id, breach_name, type, value, has_secret,
 *                       count, first_seen). breach_items.hash (full SHA-1 of
 *                       the identifier) and every encrypted-secret path are
 *                       NEVER selected. `value` is the identifier the adapter
 *                       already returns in properties.value and is NULL for
 *                       password rows. There is no reveal path here at all —
 *                       plaintext secrets remain exclusively behind
 *                       breach_adapter_reveal_by_uid()'s operator gate.
 *                     · role `analyst` or better is required (bulk identifier
 *                       egress); `viewer` gets 403. All other kinds are
 *                       readable by any member of the tenant.
 *
 * 4. `case` is only available once a `cases` table with a `tenant_id` column
 *    exists (roadmap item for Cases owns that DDL, not this file). Until then
 *    the kind returns 501 kind_unavailable. Its column set is discovered at
 *    runtime from PRAGMA table_info(cases) so it can never drift from the
 *    table, minus a name-based sensitive-column denylist (secret/password/
 *    token/key/hash/cipher/credential/salt/nonce).
 */

/* ── Streaming writer contract ────────────────────────────────────────────
 * Called with a bounded batch (<= 64 KB) of already-formatted output bytes.
 * Return 0 to continue, non-zero to abort (client disconnected / backpressure
 * refusal). `buf` is NOT NUL-terminated and is only valid for the call. */
typedef int (*export_write_fn)(void *ctx, const char *buf, size_t len);

/* Pre-flight: everything the caller must know BEFORE it commits to a 200 and
 * emits headers, because once the first chunk is out there is no way back to
 * an error status. Validates kind + format + per-kind mandatory filters +
 * role + kind availability, and fills the response headers.
 * Returns 0 → *status == 200, `out` carries content_type/filename, caller may
 * emit headers and call export_run().
 * Returns non-zero → *status is 400/403/501/500 and out->error holds a
 * snake_case code for a `{"error":"..."}` body. Nothing has been written. */
typedef struct {
  char content_type[64];   /* text/csv; charset=utf-8 | application/geo+json
                            * | application/json                            */
  char filename[160];      /* "<kind>-<YYYYMMDDTHHMMSSZ>.<ext>"             */
  char error[48];          /* snake_case code, set only when status != 200  */
} export_plan;

int export_plan_request(db_handle *db, const tenant_ctx *t,
                        const char *kind, const char *format,
                        const char *query_string,
                        export_plan *out, int *status);

/* Run the export, streaming through `write`. `query_string` is the raw
 * (still percent-encoded) request query — parsed here with the same
 * mg_http_get_var() calls httpd.c's intel_items_run() uses, so kind=intel
 * accepts exactly the params /api/intel/items accepts:
 *   source, q, lang, since, until, tag, record_type, sub_source_id,
 *   has_geom, cursor, limit
 * (`limit` is accepted and ignored for export — the plan row cap governs.)
 * Other kinds accept the subset their own endpoint defines; see the .c
 * per-kind blocks. Unknown params are ignored, never guessed at.
 *
 * *out_rows receives the number of rows actually emitted. *status is 200 on a
 * completed or truncated run. Returns 0 on success; non-zero if the writer
 * reported failure (partial output was already sent — the caller can only
 * close the connection) or if a pre-flight check failed (nothing written).
 * Writes the audit row on every path. */
int export_run(db_handle *db, const tenant_ctx *t, const char *kind,
               const char *format, const char *query_string,
               export_write_fn write, void *write_ctx,
               long *out_rows, int *status);

/* ── HTTPD.C WIRING (orchestrator: add verbatim) ──────────────────────────
 *
 * 1. Add near the other includes:
 *
 *      #include "exportapi.h"
 *
 * 2. Add this file-scope helper next to reply_json() (it is the bridge from
 *    the writer contract to mongoose's chunked transfer). The is_closing /
 *    is_draining test is what makes a disconnected client stop a 250k-row
 *    walk instead of formatting it into a socket nobody is reading:
 *
 *      // export_write_fn over a mongoose connection: one Transfer-Encoding
 *      // chunk per batch. Non-zero once the peer is gone → export_run stops.
 *      static int export_mg_write(void *ctx, const char *buf, size_t len) {
 *        struct mg_connection *c = (struct mg_connection *) ctx;
 *        if (c->is_closing || c->is_draining) return 1;
 *        mg_http_write_chunk(c, buf, len);
 *        return 0;
 *      }
 *
 * 3. Add this block inside fn()'s AUTHENTICATED section (after `usr` is
 *    resolved — same region as the /api/alerts and /api/members blocks — and
 *    before the trailing 404):
 *
 *      // ---- GET /api/export/:kind — streaming row export (item 7) ----
 *      if (starts(u, "/api/export/") && u.len > 12) {
 *        if (hm->method.len != 3 || memcmp(hm->method.buf, "GET", 3) != 0) {
 *          reply_json(c, 405, "{\"error\":\"method_not_allowed\"}"); return;
 *        }
 *        struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
 *        char xtid[128] = {0};
 *        if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); }
 *        tenant_ctx tc;
 *        int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
 *        if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
 *        if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
 *
 *        char kind[32] = {0};
 *        { size_t kl = u.len - 12;
 *          if (kl >= sizeof kind) kl = sizeof kind - 1;
 *          memcpy(kind, u.buf + 12, kl); }
 *        char fmt[16] = {0};
 *        if (mg_http_get_var(&hm->query, "format", fmt, sizeof fmt) <= 0)
 *          snprintf(fmt, sizeof fmt, "json");
 *        char qs[2048] = {0};
 *        { size_t ql = hm->query.len;
 *          if (ql >= sizeof qs) ql = sizeof qs - 1;
 *          memcpy(qs, hm->query.buf, ql); }
 *
 *        export_plan plan; int status = 200;
 *        if (export_plan_request(g_db, &tc, kind, fmt, qs, &plan, &status) != 0) {
 *          char eb[96];
 *          snprintf(eb, sizeof eb, "{\"error\":\"%s\"}", plan.error);
 *          reply_json(c, status, eb); return;
 *        }
 *        // Headers first: after the first chunk there is no error status left.
 *        mg_printf(c,
 *          "HTTP/1.1 200 OK\r\n"
 *          "Content-Type: %s\r\n"
 *          "Content-Disposition: attachment; filename=\"%s\"\r\n"
 *          "Transfer-Encoding: chunked\r\n"
 *          "Cache-Control: no-store\r\n"
 *          "X-Accel-Buffering: no\r\n"
 *          "Access-Control-Allow-Origin: *\r\n\r\n",
 *          plan.content_type, plan.filename);
 *        c->is_resp = 0;              // we own the framing (== the SSE path)
 *        long rows = 0;
 *        export_run(g_db, &tc, kind, fmt, qs, export_mg_write, c, &rows, &status);
 *        mg_http_write_chunk(c, "", 0);   // terminating 0-length chunk
 *        return;
 *      }
 *
 *    Notes the orchestrator should not "simplify" away:
 *      · `Transfer-Encoding: chunked` MUST be sent explicitly.
 *        mg_http_write_chunk() writes raw chunk framing and does not add the
 *        header; mg_http_reply() is not used here at all.
 *      · The terminating zero-length chunk is required and is also what sets
 *        c->is_resp = 0 inside mongoose (mongoose.c:2774).
 *      · Content-Disposition uses plan.filename, which is generated here from
 *        [a-z0-9-] + a timestamp only — it never contains user input, so it
 *        needs no header-injection escaping.
 *      · Do NOT gzip via mg_http_reply's helpers; the response is streamed.
 */

#endif
