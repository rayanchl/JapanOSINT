/* core/reportapi.h — roadmap item 16: the case REPORT builder.
 * POST /api/cases/:id/report  {"format":"md"|"html","sections":[...]}
 *
 * WHY THIS IS NOT exportapi.h
 * exportapi is a row exporter: one result set, one shape, N rows. A report is
 * a *document* — a header, prose, findings with their notes, an entity roll-up,
 * a citation table and an audit excerpt, in that order, each read from a
 * different place. It is deliberately a separate module and it deliberately
 * reuses exportapi's streaming writer contract rather than inventing a second
 * one (`report_write_fn` is a typedef of `export_write_fn`, so httpd.c's
 * `export_mg_write` bridge serves both surfaces unchanged).
 *
 * THE ONE RULE THAT MATTERS: FINDINGS COME FROM THE SNAPSHOT
 * Every finding is rendered from `case_items.snapshot_json` — the display copy
 * frozen at pin time — and NEVER from a live re-read of intel_items /
 * entities / breach_items. Those corpora are rewritten underneath a case
 * (re-fetch, re-ingest, entity merge). If the report re-read them, re-running
 * the same report on the same closed case would silently produce a different
 * document, and the deliverable would stop being evidence of what the analyst
 * actually saw. A stale-looking snapshot is the correct output; a helpfully
 * refreshed one is a forged record. See casesapi.h design note 1.
 *
 * NO PDF. Deliberate, not missing.
 * No PDF library is linked and none should be. PDF generation in-process means
 * a font stack, a layout engine and an attack surface parsing user text, all
 * inside the request path of a long-running server. This module emits Markdown
 * and standalone HTML; converting either to PDF is a client/out-of-process
 * concern (print-to-PDF in the browser, pandoc/weasyprint in a job). The
 * generated header says so in the document itself, so a reader who was handed
 * an .html never wonders whether a "real" PDF exists somewhere.
 *
 * MARKDOWN IS THE PRIMARY FORMAT; HTML IS THE SAME DOCUMENT MODEL
 * There is exactly one document model (headings / paragraphs / tables / code
 * blocks / links) and two emitters over it, so the two formats cannot drift.
 *   · Markdown: user content is escaped for the Markdown sink — backslash,
 *     backtick, `*`, `_`, `[`, `]`, `<` and `|` are escaped in every inline and
 *     table-cell position, and newlines are folded to spaces there, so a case
 *     name containing `|` cannot break a table and a label containing a
 *     backtick cannot open a code span.
 *   · HTML: `&<>"'` are escaped in EVERY position, attributes included. A case
 *     named `<script>alert(1)</script>` renders as visible text, never as
 *     markup. URLs are additionally scheme-checked (http/https/mailto only);
 *     anything else is rendered as an inert code span rather than an <a href>,
 *     so a `javascript:` link pinned into a snapshot cannot become live.
 *   · The HTML emitter does NOT render nested Markdown (annotation `body_md`,
 *     case `summary`). It escapes it and preserves whitespace. Rendering
 *     analyst-authored Markdown into live HTML would mean shipping a
 *     sanitizer, and annotationsapi.h is explicit that body_md is stored raw
 *     precisely because analysts paste payloads and raw HTML *as evidence*.
 *     The HTML report therefore shows that text verbatim and inert.
 *
 * ── REQUIRED DDL ─────────────────────────────────────────────────────────
 *   NONE. This module creates no tables and adds no columns. It reads cases,
 *   case_items, annotations, entities (via entityapi), audit_events, users,
 *   and writes only through audit_write().
 *
 * ── OPTIONAL DDL (paid-tier report templates — the seam, not the feature) ──
 *   Per-tenant branding is a paid-tier hook. This module does not build the
 *   config surface (no CRUD, no validation, no UI contract); it only knows how
 *   to READ one optional table and degrade to a plain unbranded document when
 *   it is absent. The orchestrator may apply this now or never — the module
 *   behaves identically either way, because report_template_load() probes
 *   sqlite_master and then probes each column individually, so a table created
 *   later with only a subset of these columns still works.
 *
 *     CREATE TABLE IF NOT EXISTS tenant_report_templates (
 *       tenant_id      TEXT PRIMARY KEY REFERENCES tenants(id) ON DELETE CASCADE,
 *       classification TEXT,      -- banner text, e.g. "CONFIDENTIAL // TLP:AMBER"
 *       header_note    TEXT,      -- one line under the classification banner
 *       footer_md      TEXT,      -- appended verbatim to the document footer
 *       logo_url       TEXT,      -- HTML only; http(s) only, else ignored
 *       updated_at     TEXT NOT NULL DEFAULT (datetime('now')));
 *
 * ── SECTIONS ─────────────────────────────────────────────────────────────
 *   "header"     case name/status/priority/author/generated-at/tenant
 *   "summary"    the case `summary` field (executive summary)
 *   "findings"   case_items in pin order + their annotations
 *   "entities"   entities referenced by pinned items (entityapi_item_entities)
 *   "citations"  source_id / link / capture timestamp [+ content SHA-256]
 *   "audit"      audit_events whose target is this case
 *
 * `sections` absent or empty → all six, in the order above (the request's
 * ordering is IGNORED: a report with the citations before the findings is not
 * a report, and letting the caller reorder invites exactly that). An unknown
 * section name is a 400 `unknown_section`, never a silent drop — a section
 * missing from a deliverable because of a typo is the worst possible failure
 * mode for this endpoint. The document title and the classification banner are
 * emitted regardless of `sections`: a banner the caller can switch off is not
 * a classification marking.
 *
 * ── EVIDENCE HASHES (roadmap item 17 interop, absent-tolerant) ───────────
 * The citations table gains a `Content SHA-256` column IF an `evidence` table
 * exists. Item 17 owns that table and is being written in parallel, so this
 * module binds to it by SHAPE, not by assumption: it probes sqlite_master for
 * the table, then PRAGMA table_info for
 *     a digest column   — first of content_sha256 | sha256 | content_hash |
 *                         digest | hash
 *     a key column      — first of ref_id | item_uid | uid | ref
 *     optional          — ref_type (added to the predicate when present)
 *     optional          — tenant_id (added to the predicate when present)
 * If the table is missing, or has no recognisable digest/key pair, the column
 * is OMITTED from the table entirely (header and cells both) and the audit
 * payload records evidence:false. It never fails the report and never emits an
 * empty column that a reader could misread as "no hash on file". Column names
 * interpolated into SQL come from a fixed allowlist matched against
 * table_info — no user input reaches the statement text.
 *
 * ── TENANT ISOLATION ─────────────────────────────────────────────────────
 * The case is resolved ONLY as (id, tenant_id) — a case in another tenant is a
 * 404 `not_found`, identical to a case that does not exist, because a 403 here
 * would confirm the id is real. Annotations are additionally filtered by
 * tenant_id AND (case_id IS NULL OR case_id = this case), so a note another
 * case attached to the same intel item does not bleed into this deliverable.
 * audit_events are filtered by tenant_id. Reading requires tenant membership
 * only — tenant_resolve() has already proved that, and the read rule matches
 * casesapi.h ("read: any tenant member"). A viewer may generate a report.
 *
 * ── AUDIT ────────────────────────────────────────────────────────────────
 * Exactly one `report.generate` row per call, target = the case id (so it
 * lands in the same trail as case.create / case.item.pin and shows up in this
 * very report's audit section on the next run). Written AFTER the walk so the
 * counts are true, and written on the aborted path too: a half-delivered
 * report is still an egress event — the bytes left the building.
 * Payload: {format, sections, findings, entities, citations, audit_rows,
 *           bytes, evidence, truncated_audit, aborted}.
 *
 *   >>> MONGOOSE CONSTRAINT — same caveat exportapi.h documents <<<
 *   mg_http_write_chunk() appends to c->send, which the event loop drains only
 *   after the synchronous MG_EV_HTTP_MSG dispatch returns. So this module's
 *   memory is bounded (one 64 KB batch), but the HTTP layer accumulates the
 *   WHOLE response before a byte hits the socket. For a report that ceiling is
 *   the document size, which the per-section caps below keep in the low
 *   megabytes even for a large case — materially safer than exportapi's
 *   quarter-million-row worst case, but the same mechanism. True socket-paced
 *   backpressure needs the same two options exportapi.h lists (detached thread
 *   + mg_wakeup, or a high-water refusal resumed from MG_EV_WRITE), and it is
 *   the same httpd.c concern, deliberately outside this file.
 */
#ifndef JO_REPORTAPI_H
#define JO_REPORTAPI_H
#include "db.h"
#include "tenantapi.h"
#include "exportapi.h"
#include <stddef.h>

/* The streaming writer contract, shared verbatim with exportapi so httpd.c
 * needs exactly one bridge function for both. Bounded batch (<= 64 KB) of
 * already-formatted bytes; return 0 to continue, non-zero to abort (client
 * gone). `buf` is NOT NUL-terminated and is valid only for the call. */
typedef export_write_fn report_write_fn;

/* Per-tenant branding — the paid-tier seam. Filled from the optional
 * tenant_report_templates table; all-empty + present=0 when the table (or the
 * tenant's row) is absent, which is the free-tier document. Exposed rather
 * than kept private so a later billing/config module can populate the same
 * struct from somewhere else without touching the emitters. */
typedef struct {
  char classification[128];  /* banner text; "" → no banner emitted   */
  char header_note[256];     /* one line under the banner             */
  char footer_md[512];       /* appended to the footer, verbatim      */
  char logo_url[512];        /* HTML only, http(s) only, else ignored */
  int  present;              /* 1 → a template row was found          */
} report_template;

/* Never fails: on any error (no table, no row, SQL failure) `out` is zeroed
 * and present=0. Returns 1 if a template row was applied, 0 otherwise. */
int report_template_load(db_handle *db, const char *tenant_id,
                         report_template *out);

/* Pre-flight — everything the caller must know BEFORE committing to a 200 and
 * emitting headers, because after the first chunk there is no error status
 * left. Validates format + section names and resolves the case inside the
 * caller's tenant.
 * Returns 0 → *status == 200 and `out` carries content_type/filename; the
 * caller may emit headers and call report_run().
 * Returns non-zero → *status is 400/404/500 and out->error holds a snake_case
 * code for a `{"error":"..."}` body. Nothing has been written.
 *   400 bad_format | unknown_section | bad_request
 *   404 not_found        (missing case OR another tenant's case — same answer)
 *   500 server_error
 * `body` may be NULL/empty → format=md, all sections. */
typedef struct {
  char content_type[64];   /* text/markdown; charset=utf-8 | text/html; ... */
  char filename[160];      /* "case-<id>-<YYYYMMDDTHHMMSSZ>.<md|html>"      */
  char error[48];          /* snake_case code, set only when status != 200  */
} report_plan;

int report_plan_request(db_handle *db, const tenant_ctx *t,
                        const char *case_id, const char *body,
                        report_plan *out, int *status);

/* Build and stream the document. `body` is the same request body passed to
 * report_plan_request() (parsed again here — the plan is a validation result,
 * not a parse cache, so the two calls cannot disagree about what was asked
 * for). *out_bytes receives the number of document bytes handed to `write`.
 * Returns 0 on a completed run; non-zero if the writer aborted (partial output
 * already sent — the caller can only close the connection) or if validation
 * failed (nothing written; *status carries the code). Writes the
 * `report.generate` audit row on every path. */
int report_run(db_handle *db, const tenant_ctx *t, const char *case_id,
               const char *body, report_write_fn write, void *write_ctx,
               long *out_bytes, int *status);

/* ── HTTPD.C WIRING (orchestrator: add verbatim) ───────────────────────────
 *
 * 1. Add near the other core includes:
 *
 *      #include "reportapi.h"
 *
 * 2. This block MUST be placed IMMEDIATELY BEFORE the `/api/cases` block from
 *    casesapi.h. That block matches `starts(u, "/api/cases/")` and would
 *    otherwise swallow this path and answer 404 for act="report". Ordering is
 *    the whole wiring risk here; there is nothing else subtle.
 *
 * 3. It reuses `export_mg_write` — the file-scope helper exportapi.h asks for.
 *    If the item-7 wiring has not landed yet, add that identical helper now
 *    (it is not export-specific: it is the mongoose bridge for the shared
 *    writer contract).
 *
 *    // ---- Roadmap 16: POST /api/cases/:id/report — streaming report ----
 *    // MUST precede the /api/cases block, which matches "/api/cases/" and
 *    // would swallow this route.
 *    if (starts(u, "/api/cases/") && u.len > 18 &&
 *        memcmp(u.buf + u.len - 7, "/report", 7) == 0) {
 *      if (hm->method.len != 4 || memcmp(hm->method.buf, "POST", 4) != 0) {
 *        reply_json(c, 405, "{\"error\":\"method_not_allowed\"}"); return;
 *      }
 *      struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
 *      char xtid[128] = {0};
 *      if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
 *      tenant_ctx tc;
 *      int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
 *      if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
 *      if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
 *
 *      char cid[128] = {0};
 *      { char raw[256] = {0};
 *        size_t rl = u.len - 11 - 7;        // between "/api/cases/" and "/report"
 *        if (rl >= sizeof raw) rl = sizeof raw - 1;
 *        memcpy(raw, u.buf + 11, rl);
 *        mg_url_decode(raw, strlen(raw), cid, sizeof cid, 0); }
 *
 *      char *bdy = NULL;
 *      if (hm->body.len) { bdy = malloc(hm->body.len + 1);
 *        memcpy(bdy, hm->body.buf, hm->body.len); bdy[hm->body.len] = 0; }
 *
 *      report_plan plan; int status = 200;
 *      if (report_plan_request(g_db, &tc, cid, bdy, &plan, &status) != 0) {
 *        char eb[96];
 *        snprintf(eb, sizeof eb, "{\"error\":\"%s\"}", plan.error);
 *        free(bdy); reply_json(c, status, eb); return;
 *      }
 *      // Headers first: after the first chunk there is no error status left.
 *      mg_printf(c,
 *        "HTTP/1.1 200 OK\r\n"
 *        "Content-Type: %s\r\n"
 *        "Content-Disposition: attachment; filename=\"%s\"\r\n"
 *        "Transfer-Encoding: chunked\r\n"
 *        "Cache-Control: no-store\r\n"
 *        "X-Content-Type-Options: nosniff\r\n"
 *        "X-Accel-Buffering: no\r\n"
 *        "Access-Control-Allow-Origin: *\r\n\r\n",
 *        plan.content_type, plan.filename);
 *      c->is_resp = 0;              // we own the framing (== the export/SSE path)
 *      long bytes = 0;
 *      report_run(g_db, &tc, cid, bdy, export_mg_write, c, &bytes, &status);
 *      free(bdy);
 *      mg_http_write_chunk(c, "", 0);   // terminating 0-length chunk
 *      return;
 *    }
 *
 *  Notes the orchestrator should not "simplify" away:
 *    · `Transfer-Encoding: chunked` MUST be sent explicitly; mg_http_write_chunk
 *      writes raw chunk framing and adds no header. mg_http_reply is not used.
 *    · The terminating zero-length chunk is required (it is also what clears
 *      c->is_resp inside mongoose).
 *    · plan.filename is generated from [a-z0-9-] plus a timestamp only — the
 *      case id is sanitized to that alphabet — so it never carries user input
 *      into the header and needs no injection escaping. The case NAME is
 *      deliberately not in the filename for exactly that reason.
 *    · X-Content-Type-Options: nosniff matters for the HTML format; without it
 *      a browser can be talked into sniffing a text/markdown response as HTML.
 *    · POST, not GET, because the section selection is a body and because a
 *      report generation is an audited egress event, not a cacheable read.
 *
 * No main.c change (no worker). No Makefile change (the core sources are
 * globbed).
 * reportapi.c calls entityapi_item_entities() and audit_write(), both already
 * linked into the binary.
 * ───────────────────────────────────────────────────────────────────────── */

#endif
