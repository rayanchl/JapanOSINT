/* core/content_change.h — roadmap 26: content change detection for scraped and
 * web_request sources.
 *
 * WHY THIS EXISTS, AND WHY IT IS NOT core/maint_detect.c
 *
 * maint_detect.c answers "did the collector break?" — status, record counts,
 * duration. This module answers a different and, for an analyst, more
 * interesting question: "the collector worked fine, and the PAGE said something
 * different today." A ministry advisory page that quietly gains a paragraph, a
 * prefectural evacuation notice that changes a number, a vendor security page
 * that drops a product from its supported list — none of that trips a fetch
 * failure. Nothing in the product noticed it before this module.
 *
 * THE ENTIRE FEATURE IS THE NORMALIZER, AND THAT IS NOT A FIGURE OF SPEECH
 *
 * A raw byte-for-byte comparison of two fetches of the same public web page is
 * different essentially every time: a rendered "Last updated 2026-07-26
 * 14:03:11" stamp, a fresh CSRF token in a hidden input, an AdSense slot id, a
 * "1,284 views" counter, an asset URL with ?v=8f3c9d1e. Ship that and every
 * watched page reports a change every tick. The feature then trains the analyst
 * to ignore content_change items, which is strictly worse than not shipping it:
 * a real change now arrives inside a stream the analyst has learned to skip.
 *
 * So the pipeline is: extract text → normalize → hash the NORMALIZED text →
 * compare to the previous snapshot for the same (source_id, ref_key). What is
 * stripped, and why, is documented exhaustively below because the strip list IS
 * the product decision — everything else here is mechanics.
 *
 * ─── WHAT IS STRIPPED, AND WHY ─────────────────────────────────────────────
 *
 * STAGE 1 — markup removal (HTML input only; JSON/text skips straight to 3).
 *
 *   <script> <style> <noscript> <template> <svg> <canvas> <iframe> <object>
 *   <embed> <ins>  — element AND contents dropped.
 *     script/style/template: never rendered prose. Inline <script> is where
 *     nonces, CSRF tokens, build hashes, epoch timestamps and analytics config
 *     actually live, so leaving it in guarantees a change on every fetch.
 *     <ins> is AdSense's own tag; <iframe>/<object>/<embed> are ad and widget
 *     slots whose ids rotate per impression. <svg>/<canvas> carry generated
 *     element ids and no text a reader sees. <noscript> is usually the tracking
 *     pixel fallback.
 *   <!-- ... -->  — HTML comments dropped. Build ids, cache timestamps, server
 *     hostnames and "generated in 41ms" footers hide here.
 *   <!DOCTYPE ...> and processing instructions dropped.
 *   ALL REMAINING TAGS AND ALL ATTRIBUTES are dropped by lib/htmlparse.c's
 *     html_strip(), which this module reuses rather than reimplements. That one
 *     fact removes a whole class of noise for free and is worth stating
 *     explicitly: hidden CSRF/nonce inputs, data-* analytics attributes, class
 *     names carrying ad slot ids, and cache-buster query strings inside src=/
 *     href= attributes are all ATTRIBUTE values, and none of them survive.
 *   Block-level tags (p, div, li, tr, td, th, h1..h6, section, article, ul, ol,
 *     table, pre, blockquote, form, br, hr, …) are replaced by a line break
 *     BEFORE html_strip runs, because html_strip collapses all whitespace and
 *     would otherwise hand back one enormous line — and a one-line document has
 *     no useful line diff.
 *
 * STAGE 2 — HTML entity decoding (&amp; &lt; &gt; &quot; &#39; &nbsp; &#NNN;
 *   &#xHH;). Not noise removal: a page that switches from "&amp;" to "&#38;"
 *   is not a content change, and decoding makes those identical.
 *
 * STAGE 3 — per-line token normalization. Each token is replaced by a
 *   placeholder when it matches one of these, and by nothing otherwise:
 *
 *   [DATE]   ISO-8601 dates and date-times (2026-07-26, 2026-07-26T14:03:11Z,
 *            with or without offset); numeric dates with / . or - separators
 *            where one component is a 4-digit year; Japanese dates carrying
 *            年/月/日 with digits.
 *   [TIME]   Clock times hh:mm and hh:mm:ss (with optional fraction, am/pm, or
 *            trailing zone); Japanese 時/分/秒 clock tokens.
 *   [AGO]    Relative timestamps: "<n> seconds|minutes|hours|days|weeks|months|
 *            years ago", "just now", and the Japanese "<n>分前 / 時間前 / 日前 /
 *            秒前 / 週間前 / か月前". These change on every single fetch by
 *            construction and are the single noisiest thing on a modern page.
 *   [EPOCH]  Bare 13-digit integers (millisecond epoch) and bare 10-digit
 *            integers in [1000000000, 2147483647] (second epoch, 2001–2038).
 *            Deliberately NOT all 10-digit numbers: Japanese phone numbers are
 *            10–11 digits, and a phone number changing on a contact page is a
 *            real change. Numbers outside that window are left alone.
 *   [TOKEN]  Opaque high-entropy tokens: runs of [A-Za-z0-9_=-] of length >= 28
 *            that mix letters and digits, plus pure-hex runs of length >= 32.
 *            This is the CSRF field / session id / nonce / JWT / asset
 *            fingerprint class. The character set deliberately excludes '/'
 *            and '.', so a URL is never swallowed whole by this rule — a link
 *            changing is a real change and must survive.
 *   [COUNT]  A number (with , . separators and optional K/M/万/千 suffix)
 *            immediately followed by a view-counter unit: views, view, hits,
 *            reads, downloads, likes, comments, subscribers, plays, online,
 *            users, members, 閲覧, アクセス, PV. Also the "Views: 1,284" label
 *            form. These are absolute counters that tick without the content
 *            changing.
 *
 *   URL-shaped tokens are canonicalized instead of replaced: the fragment is
 *   dropped and cache-buster / analytics query parameters are removed by NAME
 *   (v, ver, _, t, ts, cb, cachebust, cachebuster, rnd, rand, nocache, _t,
 *   __cb, utm_*, gclid, fbclid, msclkid, _ga, _gl). Every other parameter is
 *   kept in its original order, because a changed query parameter is usually
 *   exactly the change worth reporting.
 *
 * STAGE 4 — whitespace. Runs of ASCII whitespace collapse to one space; lines
 *   are trimmed; lines that normalize to empty are dropped entirely. This is
 *   what makes a reflow of the HTML (a template change that alters indentation
 *   but not text) a non-event.
 *
 * ─── WHICH WAY THIS ERRS, STATED PLAINLY ───────────────────────────────────
 *
 * IT ERRS TOWARD UNDER-STRIPPING. Over-stripping hides a real change, and a
 * change-detector that silently misses changes is not merely noisy, it is
 * actively misleading — the analyst reasonably concludes the page is stable.
 * A false positive costs one glance at a diff that turns out to be a counter;
 * a false negative costs the finding. Concretely, the conservative choices are:
 *
 *   - 10-digit numbers are only treated as epochs inside the plausible epoch
 *     window, so phone numbers, postal codes and IDs survive.
 *   - The opaque-token threshold is 28 characters, not 16. A 16-character
 *     alphanumeric run is frequently a product code, an advisory id
 *     (JVNDB-2026-001234 survives — it contains '-' but is 18 chars) or a
 *     Japanese licence number. 28 is past anything the corpus uses as content.
 *   - Ad and analytics removal is done by ELEMENT (script/ins/iframe/…), never
 *     by guessing at class names like "ad" or "banner". Matching class
 *     substrings would eat "advisory", "address", "adopted", 広告 in prose,
 *     and every element whose id happens to contain those letters.
 *   - Counter stripping requires the unit word to be adjacent to the number.
 *     A bare "1,284" on its own line stays: it might be a case count.
 *   - "件" (Japanese counter suffix) is NOT in the counter unit list even
 *     though "1,284件" is often a view count, because it is at least as often
 *     "1,284 cases" / "1,284 records" — the exact number an analyst is watching.
 *
 * Two consequences follow and are accepted: pages with a rotating quote-of-the
 * -day, a randomised related-links block, or a server-rendered A/B variant WILL
 * report changes on most fetches. The correct fix for those is per-source
 * (turn sources.watch_content off for that source), not a broader strip rule.
 *
 * ─── DIFF ──────────────────────────────────────────────────────────────────
 *
 * The emitted intel item carries a real unified diff of the normalized text,
 * produced by a suffix-DP LCS over lines with 3 lines of context (implemented
 * here; no dependency added). Both directions are bounded:
 *
 *   - Common prefix and suffix are trimmed before the DP runs, which on a real
 *     page collapses a 6,000-line document to a few dozen changed lines.
 *   - The DP only runs when BOTH trimmed middles are <= CC_LCS_MAX_LINES (800),
 *     i.e. at most 800*800 uint16 cells = 1.28 MB of scratch. Above that the
 *     module does NOT attempt an LCS: it emits a single degenerate hunk listing
 *     up to CC_COARSE_LINES removed lines then up to CC_COARSE_LINES added
 *     lines, and sets "diff_truncated": true in properties. A wrong diff is
 *     worse than an honest partial one.
 *   - The rendered diff is capped at JO_CCHANGE_DIFF_MAX bytes (default 65536).
 *     On overflow it stops at a line boundary and appends
 *     "@@ diff truncated @@", and "diff_truncated" is true. The hash comparison
 *     is unaffected — a truncated diff never suppresses the EVENT, only its
 *     rendering.
 *
 * WHEN THE DIFF IS EMPTY BUT THE HASH CHANGED. extract_text is stored capped at
 * JO_CCHANGE_TEXT_MAX bytes (default 262144) while the hash is computed over
 * the FULL normalized text. A change past that offset is therefore DETECTED but
 * not SHOWN; properties then carry "diff_available": false with
 * "diff_unavailable_reason": "beyond_stored_text" (or "no_baseline_text" when
 * the previous snapshot predates text storage). Hashing the truncated text
 * instead would have made the two agree by making the detector blind, which is
 * the over-stripping failure in another costume.
 *
 * ─── STORAGE: WHY THE RAW DOCUMENT IS NOT IN SQLITE ────────────────────────
 *
 * content_snapshots stores the NORMALIZED TEXT only, capped, and never raw
 * HTML. The raw document goes to core/evidence.c's content-addressed blob store
 * (roadmap 17) — temp-file + fsync + rename, SHA-256 addressed, deduplicated
 * across sources, reaped under a byte budget. Reimplementing any of that here
 * would be a second, worse blob store.
 *
 * The reuse is deliberately NARROW, and the narrowness is the design decision:
 * content_change calls evidence_capture() ONLY when it has decided a change is
 * genuine, under item_uid "cc:<source_id>|<ref_key>". Rationale:
 *   - On the normal path this module runs one line after evidence_http_hook()
 *     in the same function over the same bytes, so evidence has usually stored
 *     the blob already; evidence dedups by content hash, so the second call
 *     costs a stat() and adds one custody ROW that ties those bytes to the
 *     ref_key that changed. That row is the useful part.
 *   - Calling it on EVERY fetch would append a custody row per tick to an
 *     append-only, chain-hashed table for content nobody has asked about. The
 *     chain is a security primitive; padding it with a row a minute per source
 *     dilutes it for no analytic gain.
 *   - It is a no-op unless the operator has set sources.capture_evidence=1, so
 *     raw preservation stays opt-in and this module cannot fill a disk.
 * The emitted item carries "raw_sha256" (the evidence blob's content address)
 * and "evidence_item_uid", so the raw bytes are one operator-gated lookup away:
 *   GET /api/intel/items/cc:<source_id>|<ref_key>/evidence
 *
 * ─── SCHEMA ────────────────────────────────────────────────────────────────
 *
 * (A) core/schema.sql — SAFE to append verbatim (a brand-new table; nothing in
 *     it references a column added by a migration):
 *
 *   CREATE TABLE IF NOT EXISTS content_snapshots (
 *     source_id TEXT NOT NULL, ref_key TEXT NOT NULL,
 *     content_sha256 TEXT NOT NULL, extract_text TEXT,
 *     captured_at TEXT NOT NULL DEFAULT (datetime('now')),
 *     PRIMARY KEY (source_id, ref_key, content_sha256));
 *   CREATE INDEX IF NOT EXISTS idx_content_snapshots_recent
 *     ON content_snapshots(source_id, ref_key, captured_at DESC);
 *
 *   idx_content_snapshots_recent is the only hot read this module makes — "the
 *   most recent snapshot for (source_id, ref_key)" runs on every watched fetch.
 *   Without it that is a scan of the whole snapshot table per fetch.
 *
 * (B) ONE NEW COLUMN on an existing table — orchestrator: add to db.c's
 *     boot-migration block, NEVER to schema.sql (an ALTER there bricks the
 *     second boot; a column appended to `CREATE TABLE IF NOT EXISTS sources`
 *     never reaches a deployed database):
 *
 *   ensure_column(db, "sources", "watch_content", "INTEGER");
 *
 *   Tri-state ON PURPOSE, which is why it has no NOT NULL DEFAULT:
 *     NULL -> follow the type default: watched iff sources.type is 'scraped'
 *             or 'web_request'. This is the shipped behaviour for all 415
 *             sources and requires no operator action.
 *     0    -> never watch this source (the escape hatch for the rotating-quote
 *             page described above).
 *     1    -> always watch, even for type 'api' or 'dataset' (a JSON endpoint
 *             whose payload is a document worth diffing).
 *
 * (C) ONE INDEX THAT REFERENCES THAT NEW COLUMN, and therefore MUST NOT go into
 *     schema.sql:
 *
 *   CREATE INDEX IF NOT EXISTS idx_sources_watch_content
 *     ON sources(watch_content) WHERE watch_content IS NOT NULL;
 *
 *   db_open() hands schema.sql to ONE sqlite3_exec() BEFORE the boot-migration
 *   block runs. On an existing database sources.watch_content does not exist at
 *   that moment, so a CREATE INDEX naming it fails, exec abandons the remainder
 *   of the script, and the process never starts. It is therefore created by
 *   content_change_ensure_schema() below, immediately AFTER its ensure_column()
 *   call. That function performs (B), then (C), then (A), is idempotent, runs
 *   its DDL at most once per process and is also called lazily from every entry
 *   point — so a missed wiring degrades to "the first watched fetch pays for
 *   the migration", not to a failure.
 *
 * ─── WIRING THE ORCHESTRATOR MUST DO ───────────────────────────────────────
 *
 * 1) core/db.c, in db_open()'s boot-migration block, next to the other
 *    ensure_column() calls (and next to simhash_ensure_schema(db)):
 *
 *        #include "content_change.h"
 *        content_change_ensure_schema(db);
 *
 * 2) core/scheduler.c — bind the scope, exactly alongside the evidence scope
 *    that is already there in scheduler_run_source():
 *
 *        #include "content_change.h"
 *        ...
 *        evidence_scope_begin(db, d->id, NULL);
 *        content_change_scope_begin(db, d->id);      // <-- ADD
 *        int rc = d->run(&ctx, &sink);
 *        content_change_scope_end();                 // <-- ADD
 *        evidence_scope_end();
 *
 *    The same pair belongs around `def->run(&ctx, &ds.base)` in
 *    core/osint_dispatch.c:216, for the same reason evidence's does. Sources
 *    run on the calling thread, so a thread-local scope is exactly the right
 *    lifetime; a source that spawns its own threads captures nothing from them
 *    (fail-closed).
 *
 * 3) core/httpclient.c — THE capture point, ONE line, immediately AFTER the
 *    existing evidence_http_hook() call on the non-retryable path in
 *    http_request() and BEFORE `return`:
 *
 *        #include "content_change.h"
 *        ...
 *        evidence_http_hook(method, url, headers, r.status, r.body, r.body_len);
 *        content_change_http_hook(method, url, r.status,           // <-- ADD
 *                                 r.body, r.body_len);
 *
 *    It must come after evidence's hook so the raw blob is already on disk by
 *    the time this module records its content address. It never touches `out`,
 *    never changes the return value and cannot fail the fetch — see the
 *    contract on content_change_capture() below. When no scope is bound (every
 *    non-collector fetch: LLM calls, webhook delivery, the geo proxy) it is one
 *    thread-local load and a branch.
 *
 * 4) The retention pod. NEW FILE native/collectors/sources/content_change_pod.c
 *    (the Makefile globs that directory, so there is no build change):
 *
 *        #include "../../source.h"
 *        #include "../../core/content_change.h"
 *
 *        static int run(const source_ctx *ctx, intel_sink *sink) {
 *          (void)sink;                    // retention writes nothing to intel
 *          return content_change_sweep(ctx->db, 20000, ctx->cancel) < 0 ? -1 : 0;
 *        }
 *
 *        static const source_def content_change_def = {
 *          .id = "content-change-retention", .collector = "_maint",
 *          .name = "Content Snapshot Retention",
 *          .name_ja = "コンテンツ差分スナップショット保持",
 *          .update_interval_sec = 900, .run = run };
 *        REGISTER_SOURCE(content_change_def)
 *
 *    collector MUST be "_maint": scheduler.c skips fetch_log_write() and
 *    anomaly_detect() for underscore-prefixed collectors, and this job emits
 *    zero intel_items, so it would otherwise log records=0 forever and trip
 *    records_drop against its own baseline. It also inherits the scheduler's
 *    skip-if-running serialisation, so two sweeps can never overlap.
 *
 * 5) No httpd routes. This module exposes none; its output is ordinary intel,
 *    already served by GET /api/intel/items?record_type=content_change.
 *
 * ─── COST AND TUNING ───────────────────────────────────────────────────────
 *
 *   JO_NO_CONTENT_CHANGE      set → detection disabled process-wide
 *   JO_CCHANGE_MAX_DOC_BYTES  4194304  (4 MiB; larger documents skipped whole)
 *   JO_CCHANGE_TEXT_MAX       262144   (256 KiB of normalized text stored)
 *   JO_CCHANGE_KEEP           5        (snapshots kept per source_id+ref_key)
 *   JO_CCHANGE_MAX_AGE_DAYS   90       (sweep drops snapshots older than this)
 *   JO_CCHANGE_DIFF_MAX       65536    (rendered diff byte cap)
 *
 * Per watched fetch with an UNCHANGED page — the overwhelmingly common case —
 * the cost is: one pass over the document to strip markup, one html_strip pass,
 * one normalization pass, one SHA-256 (~1 ms/MB), and ONE indexed SELECT. No
 * write, no transaction, no SQLite write-lock contention. The three passes are
 * linear and allocate at most 2x the document, which is why the 4 MiB document
 * ceiling exists: it is the memory bound, not a policy.
 *
 * A CHANGED page additionally pays the LCS (bounded above), one INSERT, one
 * retention DELETE, one evidence_capture() and one intel emit (which itself
 * runs MeCab over the diff via fts_write — the reason the item body carries a
 * 4 KiB excerpt of the diff and properties carries the full one).
 *
 * KNOWN LIMITATION, called out deliberately: CC_NORM_VERSION is recorded in
 * every emitted item's properties, but content_snapshots has no version column
 * (its DDL is fixed by the roadmap). Raising CC_NORM_VERSION changes every
 * normalized hash, so the first fetch after a bump reports a change for every
 * watched ref_key. An operator bumping it must run
 *     DELETE FROM content_snapshots;
 * in the same deploy to re-baseline silently. This is not handled automatically
 * because the alternative — silently swallowing the first change after any
 * version bump — would hide a real change that happened to coincide with it.
 */
#ifndef JO_CONTENT_CHANGE_H
#define JO_CONTENT_CHANGE_H

#include "db.h"
#include <stddef.h>

/* Bumping this invalidates every stored hash. See the limitation note above. */
#define CC_NORM_VERSION 1

/* Trimmed-middle line count above which the LCS is not attempted and a coarse
 * hunk is emitted instead. 800*800 uint16 cells = 1.28 MB of scratch. */
#define CC_LCS_MAX_LINES 800

/* Lines listed on each side of the coarse (non-LCS) diff path. */
#define CC_COARSE_LINES 200

/* Unified-diff context lines around each hunk. */
#define CC_DIFF_CONTEXT 3

/* Hard ceilings that bound memory on a pathological document. A page with more
 * lines than this is truncated to the first CC_MAX_LINES and the emitted item
 * says so; a single line longer than this is truncated (minified JSON on one
 * line is the usual cause). */
#define CC_MAX_LINES 20000
#define CC_MAX_LINE_BYTES 2000

/* content_change_capture() / _http_hook() return codes. Every value is a normal
 * outcome; the return is advisory and callers on the fetch path ignore it. */
#define CC_CHANGED         0   /* hash differed; intel item emitted           */
#define CC_BASELINE        1   /* first sighting; snapshot stored, NO event   */
#define CC_UNCHANGED       2   /* normalized hash identical to the last one   */
#define CC_SKIP_DISABLED   3   /* kill switch, no scope, or watch_content=0   */
#define CC_SKIP_TYPE       4   /* source is not scraped/web_request           */
#define CC_SKIP_TOO_BIG    5   /* over JO_CCHANGE_MAX_DOC_BYTES               */
#define CC_SKIP_EMPTY      6   /* nothing survived extraction/normalization   */
#define CC_ERR           (-1)  /* logged and swallowed; never propagated      */

/* Idempotent migration: ensure_column() for sources.watch_content, then the
 * partial index over it, then content_snapshots + its index. Safe on every boot
 * and from every thread; runs its DDL at most once per process. Never fails
 * loudly — a failure degrades this module to a no-op. */
void content_change_ensure_schema(db_handle *db);

/* THE CAPTURE ENTRY POINT. Extracts text from `body`, normalizes it, hashes the
 * normalized text and compares against the most recent snapshot for
 * (source_id, ref_key). First sighting stores a baseline and emits NOTHING. A
 * differing hash stores a new snapshot, prunes to JO_CCHANGE_KEEP, hands the
 * raw bytes to evidence_capture() and emits ONE intel item through the standard
 * intel_sink with record_type='content_change', its properties carrying the
 * unified diff.
 *
 * CONTRACT: this can never fail the fetch or the ingest that triggered it. It
 * has no error path that propagates — every allocation failure, SQLite error,
 * malformed document and absent table is swallowed and reported only through
 * the advisory return code and stderr. It never longjmps, never aborts, never
 * blocks on the network, and is re-entrancy guarded (an emit that provokes an
 * outbound request cannot recurse into capture on the same thread).
 *
 * `ref_key` identifies the thing being watched WITHIN the source — a URL for a
 * scraped page, an item uid for a document inside a feed. NULL means "use the
 * canonicalized `url`", which is the right answer for every scraped source.
 * `content_type` may be NULL (HTML is then sniffed from the first byte).
 * `body` need not be NUL-terminated; `body_len` is authoritative. */
int content_change_capture(db_handle *db, const char *source_id,
                           const char *ref_key, const char *url,
                           const void *body, size_t body_len,
                           const char *content_type);

/* Thread-local scope — the bridge that lets httpclient.c, which knows nothing
 * about the database or which source it is fetching for, participate. Mirrors
 * evidence_scope_begin/end deliberately: one lifetime rule for both hooks, bound
 * in the same place. Every begin must be paired with an end on the same thread;
 * end is safe to call unpaired. */
void content_change_scope_begin(db_handle *db, const char *source_id);
void content_change_scope_end(void);

/* The single line httpclient.c calls. No-op returning CC_SKIP_DISABLED when no
 * scope is bound on this thread, when the method is not GET, or when the status
 * is not 200 — a 404 body and a rate-limit page are not "the content changed",
 * and recording them as such would produce a change event on every recovery. */
int content_change_http_hook(const char *method, const char *url, long status,
                             const void *body, size_t body_len);

/* Retention sweep for the pod (wiring 4). Deletes snapshots beyond the
 * JO_CCHANGE_KEEP most recent per (source_id, ref_key) and any snapshot older
 * than JO_CCHANGE_MAX_AGE_DAYS. `limit` caps rows deleted by this call (<=0 →
 * a built-in cap of 20000, never unbounded). `cancel` may be NULL; when
 * non-NULL it is polled between the two statements. Returns rows deleted, or
 * -1 if the schema could not be prepared. 0 is the normal steady state.
 *
 * Retention is ALSO enforced inline on every stored snapshot, so this pod is
 * not what keeps the table bounded for live sources — it is what reclaims keys
 * that stopped being fetched (a source removed, a URL that 404s now), which
 * inline pruning by definition never revisits. */
long content_change_sweep(db_handle *db, int limit, volatile int *cancel);

/* Pure normalizer, exposed for tests and for callers that want the same text
 * this module hashes. Returns malloc'd normalized text (lines joined by '\n',
 * no trailing newline), caller frees; NULL if nothing survived. `content_type`
 * may be NULL. No database, no allocation beyond the result and O(len) scratch,
 * thread-safe. */
char *content_change_normalize(const void *body, size_t body_len,
                               const char *content_type);

/* Pure unified diff between two normalized texts (either may be NULL).
 * Returns malloc'd diff text, caller frees, or NULL when the texts are equal or
 * both empty. `label` names the file in the ---/+++ header. The out params may
 * each be NULL; *truncated is set when either bound described above was hit. */
char *content_change_diff(const char *label, const char *old_text,
                          const char *new_text, int *out_added,
                          int *out_removed, int *out_truncated);

#endif
