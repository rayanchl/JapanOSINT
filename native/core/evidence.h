/* core/evidence.h — roadmap 17: evidence preservation / chain of custody.
 *
 * WHY THIS EXISTS
 * An analyst who screenshots a page and pastes it into a report has an
 * assertion, not evidence. What makes a capture defensible is (a) the exact
 * bytes a source returned, (b) proof they have not changed since, and (c)
 * proof no capture was quietly removed from the record. This module provides
 * all three: a content-addressed blob store for the bytes, a SHA-256 row hash
 * over the capture metadata, and an append-only hash chain linking every row
 * to its predecessor — the SAME scheme audit_events uses (prev_hash / row_hash
 * / chain_seq, verified by tenantapi_audit_verify). It is deliberately not a
 * second scheme: one custody primitive, verified one way.
 *
 * THE THREE DESIGN DECISIONS THAT ARE NOT OBVIOUS
 *
 * 1. Blobs are content-addressed, rows are not. ~20 Japanese sources report
 *    the same earthquake with byte-identical JSON; content addressing collapses
 *    them to one file on disk while each source still gets its own custody row.
 *    The blob write is temp-file + fsync + rename, always: a torn file whose
 *    NAME claims a hash it does not contain is worse than having no evidence,
 *    because it fails verification in a way that looks like tampering.
 *
 * 2. Evidence rows are NEVER updated and NEVER deleted — not even by the
 *    reaper. Mutating a row invalidates its row_hash; deleting one breaks the
 *    prev_hash linkage of every row after it. Both are indistinguishable from
 *    tampering to evidence_verify(). So the reaper unlinks BLOBS ONLY. A row
 *    whose blob has been evicted still proves what the content hash was, when
 *    it was captured and from where; only the bytes are gone. Readers detect
 *    that by stat()ing the blob (`blob_present` in the list output, HTTP 410
 *    from evidence_raw), never by a flag column.
 *
 * 3. Redaction happens at capture, not at read. Request headers routinely
 *    carry API keys and response headers carry Set-Cookie. If those were
 *    stored raw, the evidence store would become the single richest credential
 *    dump in the product — an operator-gated one, but a dump. Any header or
 *    URL query parameter whose NAME contains auth/key/token/secret/password/
 *    cookie/session has its VALUE replaced with "[redacted]" before hashing,
 *    so the redaction is itself covered by the chain.
 *
 * KNOWN LIMITATION (called out deliberately): the `evidence` table has no
 * tenant_id — it records what the platform's collectors fetched, which is
 * server-wide, not tenant data. Consequently evidence_raw() is gated by the
 * PLATFORM OPERATOR gate (opgate_check), not by tenant role, and its audit row
 * is written under tenant_id "platform". If per-tenant evidence is ever needed,
 * that is a schema change plus a per-tenant chain, not a filter bolted on here.
 *
 * ──────────────────────────────────────────────────────────────────────────
 * SCHEMA — orchestrator: append VERBATIM to core/schema.sql
 * ──────────────────────────────────────────────────────────────────────────
 *
 *   CREATE TABLE IF NOT EXISTS evidence (
 *     id TEXT PRIMARY KEY, item_uid TEXT NOT NULL, source_id TEXT NOT NULL,
 *     captured_at TEXT NOT NULL DEFAULT (datetime('now')),
 *     request_url TEXT, request_method TEXT, request_headers TEXT,
 *     response_status INTEGER, response_headers TEXT,
 *     content_sha256 TEXT NOT NULL, content_bytes INTEGER, content_type TEXT,
 *     blob_path TEXT NOT NULL,
 *     prev_hash TEXT, row_hash TEXT, chain_seq INTEGER);
 *   CREATE INDEX IF NOT EXISTS idx_evidence_item ON evidence(item_uid, captured_at DESC);
 *   CREATE UNIQUE INDEX IF NOT EXISTS idx_evidence_sha ON evidence(content_sha256, item_uid);
 *
 * COLUMN ON AN EXISTING TABLE — orchestrator: add to db.c's boot-migration
 * block. NOT to schema.sql (an ALTER there bricks the second boot; a column
 * appended to `CREATE TABLE IF NOT EXISTS sources` never reaches a deployed
 * database at all):
 *
 *   ensure_column(db, "sources", "capture_evidence", "INTEGER NOT NULL DEFAULT 0");
 *
 * Capture is OFF for every source until an operator sets that column to 1.
 * 415 sources on schedules down to 60s would otherwise fill a disk in days;
 * defaulting this ON would be a denial-of-service shipped as a feature.
 *
 * `blob_path` is stored RELATIVE to the evidence root ("ab/abcd…") and is
 * covered by row_hash. Storing it absolute would make every row fail
 * verification the day JO_EVIDENCE_DIR moves.
 *
 * ──────────────────────────────────────────────────────────────────────────
 * WIRING — the exact edits the orchestrator must make
 * ──────────────────────────────────────────────────────────────────────────
 *
 * (A) core/httpclient.c — THE capture point. Every outbound fetch in the
 *     product goes through http_request(). Add `#include "evidence.h"` and,
 *     in http_request(), on the non-retryable path immediately AFTER
 *     `log_host(c, url, r.status, hard);` and BEFORE `return`:
 *
 *         evidence_http_hook(method, url, headers, r.status, r.body, r.body_len);
 *
 *     httpclient.c owns no db_handle, no source_id and no item_uid, so the
 *     hook reads them from a thread-local scope (see (B)). It is a no-op —
 *     one thread-local load and a branch — when no scope is bound, which is
 *     the case for every non-collector fetch (LLM calls, webhook delivery,
 *     geo proxy). It never touches `out` and never changes the return value:
 *     capture cannot fail a fetch.
 *
 *     Optional one-liner that materially improves the evidence: httpclient.c
 *     does not collect response headers today (no CURLOPT_HEADERFUNCTION), so
 *     `response_headers` from this path is NULL and `content_type` is sniffed.
 *     Adding
 *         char *ct = NULL;
 *         curl_easy_getinfo(e, CURLINFO_CONTENT_TYPE, &ct);
 *     in do_once() and plumbing it out would let the hook record the real
 *     Content-Type. Not required; the module degrades cleanly without it.
 *
 * (B) core/scheduler.c — binds the scope. In scheduler_run_source(), add
 *     `#include "evidence.h"` and wrap the run:
 *
 *         evidence_scope_begin(db, d->id, NULL);
 *         int rc = d->run(&ctx, &sink);
 *         evidence_scope_end();
 *
 *     The same two lines belong around `def->run(&ctx, &ds.base)` in
 *     core/osint_dispatch.c:216. Sources run on the calling thread, so a
 *     thread-local scope is exactly the right lifetime; a source that spawns
 *     its own threads simply captures nothing from them (fail-closed).
 *
 * (C) core/main.c — the reaper pod, next to alert_deliver_start/stop:
 *
 *         evidence_gc_start(&db);          -- after alert_deliver_start(&db)
 *         int rc = httpd_serve(&db, port);
 *         evidence_gc_stop();              -- before alert_deliver_stop()
 *
 *     Without this the byte budget is only enforced lazily (capture refuses
 *     new blobs once the cached total crosses it) and the store never shrinks.
 *     Item 4 of this module is not optional; this is how it gets scheduled.
 *
 * (D) core/httpd.c — three routes:
 *
 *       GET /api/intel/items/:uid/evidence        (plain auth, tenant-agnostic)
 *         char *body = evidence_list_for_item(g_db, uid, limit);
 *         if (!body) { reply_json(c, 500, "{\"error\":\"server_error\"}"); return; }
 *         reply_json(c, 200, body); free(body); return;
 *
 *       GET /api/evidence/verify                  (operator-gated, JSON)
 *         char *body = evidence_verify(g_db);   -- reply 500 when NULL
 *
 *       GET /api/evidence/:id/raw                 (operator-gated, BYTES)
 *         unsigned char *buf = NULL; size_t len = 0; char sha[65]; int st = 200;
 *         char *e = evidence_raw(g_db, &usr, id, &buf, &len, sha, &st);
 *         if (e) { reply_json(c, st, e); free(e); return; }
 *         mg_printf(c, "HTTP/1.1 200 OK\r\n"
 *                      "Content-Type: application/octet-stream\r\n"
 *                      "Content-Disposition: attachment; filename=\"%s.bin\"\r\n"
 *                      "X-Content-Type-Options: nosniff\r\n"
 *                      "Content-Length: %lu\r\n"
 *                      "Cache-Control: no-store\r\n\r\n", sha, (unsigned long)len);
 *         c->is_resp = 0;                       -- we own the framing
 *         mg_send(c, buf, len);
 *         free(buf); return;
 *
 *       Raw evidence is whatever a third party returned — HTML with script,
 *       a polyglot image, anything. It MUST go out as application/octet-stream
 *       with nosniff and as an attachment, and must never be echoed into an
 *       HTML page or reflected through a JSON string. That is why this one
 *       function breaks the house rule and returns bytes, not JSON.
 *
 *     Optional operator route: POST /api/evidence/gc → evidence_gc(g_db).
 *
 * ──────────────────────────────────────────────────────────────────────────
 * STORAGE GOVERNANCE — defaults
 * ──────────────────────────────────────────────────────────────────────────
 *   JO_EVIDENCE_DIR             $JO_REPO_ROOT/data/evidence   (0700 dirs, 0600 files)
 *   JO_EVIDENCE_MAX_BYTES       2147483648  (2 GiB total blob bytes)
 *   JO_EVIDENCE_MAX_BLOB_BYTES  8388608     (8 MiB; larger responses skipped)
 *   JO_EVIDENCE_GC_INTERVAL_SEC 3600        (clamped 60..86400)
 *   JO_NO_EVIDENCE              set → capture disabled process-wide
 *   JO_NO_EVIDENCE_GC           set → reaper pod not started
 *   low-water mark              90% of the budget (reap to there, not to the
 *                               line, so the next hour is not one long reap)
 *
 * COST, honestly. With capture off for a source (the default) the per-fetch
 * cost is a thread-local load plus a hash-table probe of a 60s-TTL opt-in
 * cache: unmeasurable. With capture ON: one SHA-256 over the body (~1 GB/s,
 * so ~1 ms for a 1 MB feed), one stat(), and — only when the content is new —
 * a write + fsync + rename plus one INSERT that serialises on this module's
 * chain mutex and SQLite's write lock. The fsync is the real cost and it is
 * paid on the collector thread, so a slow disk slows collection. That is the
 * trade being made, and it is why the opt-in is per source.
 */
#ifndef JO_EVIDENCE_H
#define JO_EVIDENCE_H

#include "auth.h"
#include "db.h"
#include <stddef.h>

/* evidence_capture() / evidence_http_hook() return codes. All >= 0 outcomes
 * are normal operation; the caller is expected to ignore the value. */
#define EV_CAPTURED       0   /* new custody row written                     */
#define EV_SKIP_DISABLED  1   /* kill switch, no scope, or source opted out  */
#define EV_SKIP_TOO_BIG   2   /* body over JO_EVIDENCE_MAX_BLOB_BYTES        */
#define EV_SKIP_BUDGET    3   /* store at budget; reaper has not caught up   */
#define EV_SKIP_DUP       4   /* (sha, item_uid) already recorded            */
#define EV_ERR           (-1) /* blob or row write failed (logged, swallowed)*/

/* Capture one fetch. Safe to call on every request from any thread and on any
 * arguments: NULLs, zero-length bodies and a closed/absent `capture_evidence`
 * column are all handled, nothing here can longjmp, abort or block on the
 * network, and the return value is advisory. `req_headers` is the same
 * NULL-terminated "Name: value" array http_request() takes (may be NULL);
 * `resp_headers` is a raw header block or NULL; `content_type` may be NULL, in
 * which case a conservative sniff (JSON/XML/HTML/plain, else octet-stream)
 * is recorded. Secrets in headers and in the URL query string are redacted
 * before anything is hashed or stored. */
int evidence_capture(db_handle *db, const char *item_uid, const char *source_id,
                     const char *url, const char *method,
                     const char *const *req_headers,
                     long status, const char *resp_headers,
                     const void *body, size_t body_len,
                     const char *content_type);

/* Thread-local capture scope — the bridge that lets httpclient.c, which knows
 * nothing about the database or the source it is fetching for, participate.
 * `item_uid` may be NULL, in which case captures are filed under the synthetic
 * uid "src:<source_id>"; combined with the UNIQUE(content_sha256,item_uid)
 * index that means a source re-fetching unchanged bytes produces no new row,
 * which is what keeps a 60s schedule from writing 1,440 identical rows a day.
 * Pass a real item uid when a fetch maps 1:1 to an intel item. Every begin
 * must be paired with an end on the same thread; end is safe to call unpaired. */
void evidence_scope_begin(db_handle *db, const char *source_id,
                          const char *item_uid);
void evidence_scope_end(void);

/* The single line httpclient.c calls. No-op returning EV_SKIP_DISABLED when
 * no scope is bound on this thread. */
int evidence_http_hook(const char *method, const char *url,
                       const char *const *req_headers,
                       long status, const void *body, size_t body_len);

/* Custody records for one intel item uid, newest first.
 *   {"data":[{id,captured_at,source_id,request_url,request_method,
 *             request_headers:[…],response_status,response_headers:[…],
 *             content_sha256,content_bytes,content_type,blob_path,
 *             chain_seq,prev_hash,row_hash,blob_present}],
 *    "page":{"limit":n,"count":n},
 *    "meta":{"item_uid":…,"present":n,"evicted":n}}
 * limit defaults to 50, clamped 1..200. NULL → caller replies 500.
 * Caller frees. */
char *evidence_list_for_item(db_handle *db, const char *item_uid, int limit);

/* Re-walk the whole evidence chain and report the FIRST break. Response shape
 * mirrors tenantapi_audit_verify() exactly so one client renderer serves both:
 *   ok   → {"ok":true,"count":n,"tip":"<row_hash>"|null}
 *   bad  → {"ok":false,"count":n,"brokenAt":i,"brokenId":"…","reason":"…"}
 * NULL → caller replies 500. Caller frees. */
char *evidence_verify(db_handle *db);

/* Operator-gated raw bytes. DELIBERATE DEVIATION from the house rule that
 * public functions return JSON: returning the bytes through a JSON string
 * would corrupt them and invite an HTML echo.
 *   success → returns NULL, *status=200, *out_bytes = malloc'd body (caller
 *             frees), *out_len = length, out_sha (may be NULL) = 64-hex + NUL.
 *   failure → returns a malloc'd {"error":…} envelope, *status set to
 *             401/403/404/410/500, *out_bytes untouched.
 * 410 specifically means "row intact, blob reaped" — an honest answer that a
 * 404 would hide. Writes an audit_events row on every successful reveal. */
char *evidence_raw(db_handle *db, const auth_user *u, const char *evidence_id,
                   unsigned char **out_bytes, size_t *out_len,
                   char *out_sha, int *status);

/* Reap blobs until the store is under the low-water mark: oldest-last-captured
 * first, skipping any blob referenced by an evidence row whose item_uid is
 * pinned into a case (case_items.ref_type='intel_item'). If case_items does
 * not exist in this database — sqlite_master probe, reported back as
 * "case_items_present" — then nothing can be pinned and reaping proceeds
 * unprotected; that is correct, not a silent downgrade of the guarantee.
 * Sizes come from stat(), not from content_bytes, so a partially reaped store
 * is accounted honestly. Blobs are unlinked; evidence ROWS are never touched.
 * Reaps to 90% of the budget and stops, so the next pass is not one long reap.
 * Returns {"data":{…counters…}}; NULL on bad args.
 * Caller frees. Safe to call concurrently with capture. */
char *evidence_gc(db_handle *db);

/* Background reaper pod. Idempotent; honours JO_NO_EVIDENCE_GC. Stop before
 * db_close() so the thread is never holding a statement on a closed handle. */
void evidence_gc_start(db_handle *db);
void evidence_gc_stop(void);

#endif
