/* core/uploadapi.h — chunked client file upload → evidence blob store.
 *
 * WHY THIS EXISTS
 * Everything the platform has ingested so far arrived by *pulling* it: a
 * collector fetches a URL and core/evidence.c records the bytes on the way
 * past. An analyst who shares a PDF, a photo or a leaked spreadsheet into the
 * iOS app is the first inbound path, and it needs the same custody guarantees
 * as an outbound fetch — otherwise the one file a case actually turns on is
 * the one file with no hash, no dedup and no byte budget.
 *
 * So this module is deliberately NOT a storage subsystem. It is a reassembly
 * buffer in front of evidence_capture(). The only bytes it owns live in a
 * scratch directory for the lifetime of one upload; the moment the parts are
 * whole they are handed to the existing content-addressed store and the
 * scratch is deleted. There is exactly one blob store in this codebase.
 *
 * ──────────────────────────────────────────────────────────────────────────
 * THE CONSTRAINT THAT SHAPES THE WHOLE DESIGN
 * ──────────────────────────────────────────────────────────────────────────
 * third_party/mongoose.h:989 defines MG_MAX_RECV_SIZE as 3 MiB, and mongoose
 * buffers an entire request body in the event loop before MG_EV_HTTP_MSG
 * fires. That define is a per-CONNECTION ceiling: raising it to admit a 64 MiB
 * attachment would raise the memory ceiling of every concurrent connection on
 * the server, so it is not an acceptable fix, and MG_EV_HTTP_CHUNK streaming
 * would mean a second, hand-rolled body assembler inside the event loop.
 *
 * Therefore the split happens in the CLIENT. The file is cut into parts of at
 * most JO_UPLOAD_PART_MAX_BYTES (default 1 MiB — generous headroom under the
 * 3 MiB cap, which the request line, headers and mongoose's own framing share)
 * and each part is a separate POST. Nothing in this module ever sees more than
 * one part per request until commit, where the assembled buffer is read back
 * off disk, not out of a socket.
 *
 * Parts may arrive OUT OF ORDER and may be RETRIED — a phone on a train loses
 * its connection mid-upload constantly. Part writes are therefore idempotent
 * on (upload_id, seq): the same seq sent twice overwrites its scratch file and
 * the upload's running totals are RECOMPUTED from the parts table rather than
 * incremented, so a retry can never double-count.
 *
 * ──────────────────────────────────────────────────────────────────────────
 * BINARY BODIES — the rule this module cannot bend
 * ──────────────────────────────────────────────────────────────────────────
 * A part body is arbitrary file bytes and WILL contain NUL. Every entry point
 * takes (body, body_len) and nothing in this file calls strlen(), strdup(),
 * cJSON_Parse() or sqlite3_bind_text(..., -1, ...) on it — the length is the
 * only thing trusted to say where the body ends. The one JSON body (begin;
 * commit and abort take none) is copied into a length-checked, NUL-terminated
 * buffer before parsing rather than parsed in place: cJSON_ParseWithLength()
 * bounds the tokeniser, but its number path hands the raw pointer to strtod(),
 * which knows nothing about the length and will run off the end of a body that
 * happens to finish with a digit. A begin body is a three-field object, so the
 * copy is free; a part body is a megabyte of binary and is NEVER copied.
 * See wiring note (B) — httpd.c MUST pass hm->body.buf/hm->body.len straight
 * through and must NOT make the usual NUL-terminated malloc'd copy.
 *
 * ──────────────────────────────────────────────────────────────────────────
 * PATH SAFETY — filename is attacker-controlled and NEVER reaches a path
 * ──────────────────────────────────────────────────────────────────────────
 * Scratch paths are derived from the upload id and the part sequence number
 * ONLY:  $JO_UPLOAD_DIR/<id[0:2]>/<id>/<seq>.part
 *
 *   · the id is a uuid4 this module generates, and on every subsequent request
 *     the id taken from the URL is re-validated against a strict uuid shape
 *     (36 chars, [0-9a-f] with '-' at 8/13/18/23) BEFORE it is formatted into
 *     any path. "..", "/", "\" and NUL cannot survive that test, so traversal
 *     is impossible by construction rather than by escaping;
 *   · seq is parsed as a decimal integer and range-checked (0 .. 8191), then
 *     printed back with %ld — the string from the wire never reaches the path;
 *   · `filename` is DATA. It is stored in a column, echoed in status JSON and
 *     included (bounded) in the audit payload, and is used nowhere else. It is
 *     still sanitised on the way in — NUL, control bytes, '/', '\' and a
 *     leading '.' are dropped and the result is capped at 255 bytes — because
 *     a value that is only ever data today becomes a path the day somebody
 *     adds a download route, and because invalid UTF-8 would produce invalid
 *     JSON out of cJSON. Well-formed multi-byte UTF-8 is preserved: Japanese
 *     filenames are the normal case for this product, not an edge case.
 *   · directory teardown never does a blind recursive delete. It unlinks only
 *     entries matching "<digits>.part" or ".tmp-*" and then rmdir()s, so a
 *     misconfigured JO_UPLOAD_DIR pointing at something precious loses nothing.
 *
 * ──────────────────────────────────────────────────────────────────────────
 * WHAT IS PASSED TO evidence_capture(), AND WHY (requirement 4)
 * ──────────────────────────────────────────────────────────────────────────
 * evidence_capture() was written for an OUTBOUND fetch; there is no HTTP
 * request behind a client upload, so four of its arguments have no natural
 * value. They are filled deliberately, not left NULL by accident:
 *
 *   item_uid      "upload:<upload_id>"  — synthetic. The uid is half of
 *                 evidence's UNIQUE(content_sha256,item_uid) dedup key. A per
 *                 upload uid is the CORRECT choice here: two analysts
 *                 uploading byte-identical files are two custody events that
 *                 must both be recorded, while the blob itself is still stored
 *                 once because the blob store is content-addressed. Collapsing
 *                 them under a shared uid would lose the second ingress.
 *   source_id     "client-upload"       — a real row in `sources` (created by
 *                 uploadapi_migrate, see below). It MUST exist: evidence.c
 *                 fails closed on sources.capture_evidence, so an unknown
 *                 source_id means the bytes are silently dropped.
 *   url           "jo:upload/<upload_id>" — a non-HTTP scheme, so nobody ever
 *                 mistakes a custody row for something that was fetched. The
 *                 filename is deliberately NOT put here: evidence's redactor
 *                 only understands query strings, and a custody URL is the
 *                 field most likely to be echoed somewhere.
 *   method        "UPLOAD"              — not a real verb, and that is the
 *                 point: it marks the row as ingress, filterable in one grep.
 *   req_headers   NULL — there were none. Synthesising fake ones would put
 *                 attacker-controlled text into a field whose contract is
 *                 "what we sent upstream".
 *   status        0    — no upstream response exists. 0 is not a valid HTTP
 *                 status, so it can never be confused with one.
 *   resp_headers  NULL — likewise.
 *   content_type  the client's declared type, but only after it is validated
 *                 as a bare `type/subtype` token pair (letters, digits and
 *                 -+._ only, ≤127 bytes); anything else is passed as NULL and
 *                 evidence.c's own conservative sniff decides. A client does
 *                 not get to write arbitrary text into a custody record.
 *
 * TWO INHERITED LIMITS THE OPERATOR MUST KNOW ABOUT (both reported back in the
 * commit response's "evidence" field rather than hidden):
 *
 *   1. JO_EVIDENCE_MAX_BLOB_BYTES defaults to 8 MiB. JO_UPLOAD_MAX_BYTES
 *      defaults to 64 MiB. An upload between the two commits successfully,
 *      gets its sha256 and its audit row, and has NO BYTES STORED
 *      ("evidence":"skipped_too_big", "blob_present":false). If attachments
 *      larger than 8 MiB are wanted, raise JO_EVIDENCE_MAX_BLOB_BYTES to at
 *      least JO_UPLOAD_MAX_BYTES. uploadapi_migrate() logs a one-line warning
 *      to stderr at boot when the two are misconfigured relative to each other.
 *   2. evidence's LRU reaper protects a blob only when its item_uid is pinned
 *      into a case as ref_type='intel_item'. Our uid is "upload:<id>", so an
 *      uploaded attachment is NOT reaper-protected today. Closing that means
 *      either teaching evidence_gc's pin probe about upload uids or pinning
 *      uploads under an existing ref_type; both are edits to files this module
 *      is not permitted to touch. Stated here rather than left to be
 *      discovered when an attachment disappears from a case.
 *
 * ──────────────────────────────────────────────────────────────────────────
 * SCHEMA — orchestrator: append VERBATIM to core/schema.sql
 * ──────────────────────────────────────────────────────────────────────────
 * All CREATE ... IF NOT EXISTS; no ALTER anywhere (an ALTER in schema.sql
 * bricks the second boot — db_open() hands the whole file to one
 * sqlite3_exec()). No new columns on existing tables are required by this
 * module, so there is nothing for ensure_column() to do here EXCEPT the one
 * evidence.c already owns; uploadapi_migrate() calls
 * ensure_column(db, "sources", "capture_evidence", "INTEGER NOT NULL DEFAULT 0")
 * itself, which is idempotent and is a no-op once evidence's own boot
 * migration has run.
 *
 *   CREATE TABLE IF NOT EXISTS uploads (
 *     id TEXT PRIMARY KEY, tenant_id TEXT NOT NULL, user_id TEXT,
 *     filename TEXT, content_type TEXT, total_bytes INTEGER,
 *     received_bytes INTEGER NOT NULL DEFAULT 0,
 *     part_count INTEGER NOT NULL DEFAULT 0,
 *     sha256 TEXT, status TEXT NOT NULL DEFAULT 'open',
 *     created_at TEXT NOT NULL DEFAULT (datetime('now')),
 *     committed_at TEXT);
 *
 *   CREATE TABLE IF NOT EXISTS upload_parts (
 *     upload_id TEXT NOT NULL, seq INTEGER NOT NULL,
 *     bytes INTEGER NOT NULL DEFAULT 0,
 *     received_at TEXT NOT NULL DEFAULT (datetime('now')),
 *     PRIMARY KEY (upload_id, seq));
 *
 *   CREATE INDEX IF NOT EXISTS idx_uploads_tenant_status
 *     ON uploads(tenant_id, status, created_at DESC);
 *   CREATE INDEX IF NOT EXISTS idx_uploads_status_created
 *     ON uploads(status, created_at);
 *   CREATE INDEX IF NOT EXISTS idx_upload_parts_upload
 *     ON upload_parts(upload_id, seq);
 *
 * `upload_parts` is the one addition to the brief's DDL. It carries no bytes —
 * only (seq, size, arrival time) — and it buys three things a directory scan
 * would have to reconstruct with a readdir + stat per part: (a) idempotency,
 * via ON CONFLICT(upload_id,seq) DO UPDATE; (b) an authoritative received_bytes
 * that is recomputed with SUM() after every part instead of incremented;
 * (c) the "which seqs do I still owe you" answer that makes GET /:id useful to
 * a resuming client. Deliberately NO foreign key to uploads(id): the CASCADE
 * would fire only on the one connection where db.c sets PRAGMA foreign_keys=ON
 * and would silently orphan rows for a CLI or worker connection, so children
 * are deleted explicitly and in order — same reasoning as casesapi.h.
 *
 * The blobs are the reason parts are on the FILESYSTEM and not in SQLite. A
 * 64 MiB row in a 2.9 GB database evicts the page cache, doubles the WAL, and
 * makes VACUUM a maintenance event. Metadata is a row; payload is a file.
 *
 * ──────────────────────────────────────────────────────────────────────────
 * STORAGE GOVERNANCE — defaults
 * ──────────────────────────────────────────────────────────────────────────
 *   JO_UPLOAD_DIR             $JO_REPO_ROOT/data/uploads (0700 dirs, 0600 files)
 *   JO_UPLOAD_MAX_BYTES       67108864   (64 MiB per file; clamped 64 KiB..256 MiB)
 *   JO_UPLOAD_PART_MAX_BYTES  1048576    (1 MiB per part; clamped 64 KiB..2 MiB —
 *                                         2 MiB is the highest value that still
 *                                         leaves headroom under MG_MAX_RECV_SIZE)
 *   JO_UPLOAD_TTL_SEC         86400      (24 h IDLE ttl for an open upload)
 *   JO_UPLOAD_ROW_TTL_SEC     604800     (7 d before a committed/aborted row is
 *                                         deleted; until then a client that
 *                                         retries commit gets its answer back
 *                                         instead of a 404)
 *   UP_MAX_PARTS              8192       (compile-time; 8192 x 1 MiB = 8 GiB of
 *                                         seq space, far above the byte ceiling,
 *                                         while still bounding a seq-spraying
 *                                         client to 8192 scratch files)
 *
 * The 24 h TTL is measured from the LAST PART RECEIVED (MAX(received_at)),
 * falling back to created_at for an upload with no parts — not from
 * created_at. An upload being actively resumed across a working day must not
 * be reaped out from under the client at the 24 h mark; an upload nobody has
 * touched since yesterday is abandoned by any reasonable definition.
 *
 * ──────────────────────────────────────────────────────────────────────────
 * ROUTES  (all tenant-scoped; role analyst|admin|owner required for ALL five,
 *          including the reads — an upload in flight is unreleased evidence)
 * ──────────────────────────────────────────────────────────────────────────
 *   POST   /api/uploads/begin        {filename, content_type, total_bytes}
 *                                    → {upload_id, part_max_bytes, max_bytes,
 *                                       total_bytes, expires_in_sec}
 *   POST   /api/uploads/:id/part?seq=N   raw binary body, 1..part_max_bytes
 *                                    → {upload_id, seq, bytes, received_bytes,
 *                                       part_count, total_bytes, complete}
 *   POST   /api/uploads/:id/commit   → {upload_id, sha256, bytes, content_type,
 *                                       filename, part_count, blob_present,
 *                                       evidence}
 *   GET    /api/uploads/:id          → {..., received_seqs:[…], missing_seqs:[…]}
 *   DELETE /api/uploads/:id          → {upload_id, status:"aborted",
 *                                       bytes_discarded, parts_discarded}
 *
 * POST /api/uploads (no segment) is accepted as a synonym for .../begin.
 *
 * ERROR MODEL. A tenant mismatch is 404 "not_found", never 403 — 403 would
 * confirm the existence of another tenant's upload id, which is exactly the
 * oracle an id-probing client wants. A malformed id is 404 for the same
 * reason. 403 "forbidden" is reserved for the role check, which reveals
 * nothing. Other codes:
 *   400 bad_request / invalid_total_bytes / invalid_seq / empty_part
 *   405 method_not_allowed
 *   409 upload_not_open / upload_aborted / upload_committed / no_parts /
 *       missing_parts / length_mismatch / declared_length_exceeded
 *   413 file_too_large / part_too_large
 *   500 server_error / part_read_failed
 * Every 4xx that a client can act on carries a "meta" object with the numbers
 * it needs (the ceiling it hit, the seq it still owes, declared vs assembled).
 * A client that has to parse prose to resume an upload will not resume it.
 * begin answers 201; everything else answers 200.
 *
 * AUDIT. audit_write(db, tenant, user, "upload.commit", <upload_id>, payload)
 * on every successful commit — an inbound file is the ingress event most worth
 * having in the log. The payload carries sha256, byte count, part count,
 * validated content_type, the evidence outcome, and the filename TRUNCATED TO
 * 120 BYTES on a UTF-8 boundary. Bounding it is not cosmetic: filename is
 * attacker-controlled and audit_events rows are read by operators and exported;
 * an unbounded 64 KiB name would be a log-flooding primitive. begin / part /
 * abort are NOT audited — they are not yet an ingress, and one audit row per
 * part would put 64 rows in the log for one file.
 *
 * ──────────────────────────────────────────────────────────────────────────
 * ATTACHING A COMMITTED UPLOAD TO A CASE — the remaining gap, stated plainly
 * ──────────────────────────────────────────────────────────────────────────
 * casesapi's pin validator delegates to annotations_ref_type_valid(), whose
 * vocabulary is intel_item|entity|breach_item|feature|camera|search_run. There
 * is no member that describes an uploaded file, so
 * POST /api/cases/:id/items {"ref_type":"upload","ref_id":"<upload_id>"} will
 * 400 until one is added. That is a one-line edit to core/annotationsapi.c —
 * the shared validator this module is explicitly told to call rather than
 * duplicate — plus the matching line in casesapi.h's documented vocabulary,
 * and both are files this module may not touch. Until it lands, the client can
 * pin the upload only by supplying its own snapshot_json under an existing
 * ref_type, which is worse. This is flagged, not worked around.
 *
 * ──────────────────────────────────────────────────────────────────────────
 * WIRING — the exact edits the orchestrator must make
 * ──────────────────────────────────────────────────────────────────────────
 *
 * (A) core/httpd.c — with the other includes at the top:
 *
 *       #include "uploadapi.h"
 *
 * (B) core/httpd.c — one block. Place it next to the /api/cases block (an
 *     upload exists to become a case attachment) and BEFORE any catch-all.
 *     "/api/uploads/" is 13 bytes.
 *
 *     THE ONE THING THAT MUST NOT BE COPIED FROM THE NEIGHBOURING BLOCKS:
 *     every other subtree here does
 *         char *bdy = malloc(hm->body.len + 1); ... bdy[hm->body.len] = 0;
 *     and passes `bdy`. A part body is BINARY. It contains NUL, it is up to
 *     1 MiB, and copying it is both a needless megabyte memcpy per part and a
 *     lie about its type. Pass hm->body.buf and hm->body.len DIRECTLY —
 *     mongoose owns that buffer for the duration of the MG_EV_HTTP_MSG
 *     callback, which is strictly longer than the uploadapi() call, and
 *     uploadapi() never retains the pointer.
 *
 *   // ---- chunked client file upload → evidence blob store ----
 *   if (eq(u, "/api/uploads") || starts(u, "/api/uploads/")) {
 *     struct mg_str *xt = mg_http_get_header(hm, "X-Tenant-Id");
 *     char xtid[128] = {0};
 *     if (xt && xt->len < sizeof xtid) { memcpy(xtid, xt->buf, xt->len); xtid[xt->len]=0; }
 *     tenant_ctx tc;
 *     int tr = tenant_resolve(g_db, &usr, xt ? xtid : NULL, &tc);
 *     if (tr == -401) { reply_json(c, 401, "{\"error\":\"Auth required\"}"); return; }
 *     if (tr != 0)    { reply_json(c, 500, "{\"error\":\"Tenant resolution failed\"}"); return; }
 *
 *     char uid2[128] = {0}, act[32] = {0};
 *     if (u.len > 13) {                          // after "/api/uploads/"
 *       char rest[256] = {0};
 *       size_t rl = u.len - 13; if (rl >= sizeof rest) rl = sizeof rest - 1;
 *       memcpy(rest, u.buf + 13, rl);
 *       char *sl = strchr(rest, '/');
 *       if (sl) { *sl = 0; snprintf(act, sizeof act, "%s", sl + 1); }
 *       mg_url_decode(rest, strlen(rest), uid2, sizeof uid2, 0);
 *     }
 *     char meth[12] = {0};
 *     size_t ml = hm->method.len < sizeof meth - 1 ? hm->method.len : sizeof meth - 1;
 *     memcpy(meth, hm->method.buf, ml);
 *     char qsb[256] = {0};
 *     if (hm->query.len && hm->query.len < sizeof qsb)
 *       memcpy(qsb, hm->query.buf, hm->query.len);
 *
 *     int status = 200;
 *     char *body = uploadapi(g_db, &tc, meth, uid2, act, qsb,
 *                            hm->body.buf, hm->body.len, &status);   // RAW body
 *     if (!body) { reply_json(c, status, "{\"error\":\"server_error\"}"); return; }
 *     reply_json(c, status, body); free(body); return;
 *   }
 *
 *     uploadapi() never returns NULL with a 2xx status, so there is no 204
 *     branch to write; DELETE answers 200 with a JSON receipt.
 *
 * (C) native/main.c — one line in the serve path, alongside the other boot
 *     migrations and BEFORE httpd_serve():
 *
 *       uploadapi_migrate(&db);
 *
 *     It creates the two tables if schema.sql has not been regenerated yet,
 *     creates the scratch root, ensures the `client-upload` row in `sources`
 *     with capture_evidence=1 (WITHOUT WHICH EVERY COMMIT SILENTLY STORES NO
 *     BYTES — evidence.c fails closed on an unknown source), and warns if the
 *     upload ceiling exceeds evidence's per-blob ceiling. Idempotent; cheap;
 *     safe to call on every boot.
 *
 * (D) native/collectors/sources/upload_gc_pod.c — NEW FILE, the reaper. The
 *     Makefile globs that directory, so there is no build change. An upload
 *     begun and never committed must not leak scratch disk forever.
 *
 *       #include "../../source.h"
 *       #include "../../core/uploadapi.h"
 *
 *       static int run(const source_ctx *ctx, intel_sink *sink) {
 *         (void)sink;                    // reaps disk; emits no intel
 *         return uploadapi_gc(ctx->db, ctx->cancel) < 0 ? -1 : 0;
 *       }
 *
 *       static const source_def upload_gc_def = {
 *         .id = "upload-gc", .collector = "_maint",
 *         .name = "Abandoned Upload Reaper",
 *         .name_ja = "未完了アップロード回収",
 *         .update_interval_sec = 900, .run = run };
 *       REGISTER_SOURCE(upload_gc_def)
 *
 *     The collector MUST be "_maint" (leading underscore): scheduler.c skips
 *     fetch_log_write and anomaly_detect for underscore-prefixed collectors,
 *     and this pod needs that skip — it emits zero intel_items, so it would
 *     otherwise log records=0 forever and trip records_drop against its own
 *     baseline. It also inherits the scheduler's skip-if-running lock, so two
 *     sweeps never overlap. 900s against a 24 h TTL means an abandoned upload's
 *     scratch is released within 15 minutes of expiring.
 *
 * No Makefile change (the core sources are globbed). No schema.sql change beyond the
 * DDL block above. No edit to evidence.c, casesapi.c or annotationsapi.c.
 */
#ifndef JO_UPLOADAPI_H
#define JO_UPLOADAPI_H

#include "db.h"
#include "tenantapi.h"
#include <stddef.h>

/* Compile-time bound on the seq space. 8192 parts x the 1 MiB default part
 * size is 8 GiB, two orders above the 64 MiB byte ceiling, so this never
 * constrains a legitimate upload — it exists so a client spraying seq=999999
 * cannot create an unbounded number of scratch files before the byte ceiling
 * notices. */
#define UP_MAX_PARTS 8192

/* Upper bound on scratch directories examined by one orphan-scan pass. The
 * reaper runs on a scheduler thread shared with real collectors; an unbounded
 * walk of a pathological tree there is a stall, not a cleanup. */
#define UP_GC_ORPHAN_SCAN 2000

/* One entrypoint for the whole /api/uploads subtree.
 *
 *   `t`         tenant context; t->role gates every route at analyst+.
 *   `method`    HTTP verb, "POST" | "GET" | "DELETE".
 *   `seg`       the upload id, "" for the collection, or the literal "begin".
 *               Never NULL. Validated as a uuid before it reaches any path.
 *   `act`       "part" | "commit", "" for the upload itself. Never NULL.
 *   `qs`        raw query string, may be NULL. Only ?seq= is read.
 *   `body`      RAW request body — MAY CONTAIN NUL, MAY BE UNTERMINATED. Only
 *               ever read through body_len. May be NULL when body_len is 0.
 *   `body_len`  body length in bytes. Authoritative; strlen(body) is never
 *               called anywhere in this module.
 *
 * Sets *status and returns a malloc'd JSON body. Returns NULL only on an
 * allocation failure or a NULL db/tenant, with *status = 500 — the caller
 * emits a generic error of that status. There is no NULL+2xx case. Caller
 * frees. */
char *uploadapi(db_handle *db, const tenant_ctx *t, const char *method,
                const char *seg, const char *act, const char *qs,
                const char *body, size_t body_len, int *status);

/* Boot migration: creates `uploads` / `upload_parts` and their indexes if
 * absent, creates the scratch root 0700, ensures sources.capture_evidence
 * exists and that the `client-upload` source row exists with capture ON, and
 * warns on stderr when JO_UPLOAD_MAX_BYTES exceeds JO_EVIDENCE_MAX_BLOB_BYTES
 * (in which case large commits store no bytes). Idempotent; never fails
 * loudly — a boot must not die because a scratch directory could not be made,
 * the affected requests will report it. Call once from main.c before
 * httpd_serve(). */
void uploadapi_migrate(db_handle *db);

/* Reap abandoned uploads. Returns the number of uploads whose scratch was
 * released, or -1 on a bad handle. Three passes, all bounded and all safe to
 * run concurrently with live uploads:
 *
 *   1. open uploads idle longer than JO_UPLOAD_TTL_SEC (idle = now minus the
 *      newest of MAX(upload_parts.received_at) and created_at) → scratch
 *      purged, part rows deleted, row marked 'aborted'. The row survives so a
 *      late part gets a coherent 409 rather than a confusing 404;
 *   2. committed/aborted rows older than JO_UPLOAD_ROW_TTL_SEC → row and any
 *      residual scratch deleted;
 *   3. orphan scratch directories on disk with no `uploads` row at all —
 *      what a crash between mkdir() and INSERT leaves behind. Bounded to
 *      UP_GC_ORPHAN_SCAN directories per pass.
 *
 * `cancel` may be NULL; when non-NULL it is polled between uploads and a set
 * flag stops the sweep cleanly, returning the count actually reaped (not an
 * error) — the next tick continues where this one stopped. */
long uploadapi_gc(db_handle *db, volatile int *cancel);

#endif
