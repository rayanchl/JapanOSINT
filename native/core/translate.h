/* core/translate.h — backend machine translation (JA→EN) at ingest, plus the
 * English side of full-text search (roadmap 29).
 *
 * WHY THIS EXISTS. Translation used to live only in the iOS client, which
 * means the server never held an English word: intel_items_fts indexed
 * Japanese only, so every FTS-backed surface — /api/intel/items?q=,
 * exportapi, jp_corpus_lookup, and critically alert_eval's `q` predicate —
 * was Japanese-only. An English-speaking analyst could not write an alert
 * rule that ever fires. Client-side translation cannot fix that: an alert
 * fires on the server, hours before any client sees the row. So the
 * translation has to be produced at ingest and indexed server-side.
 *
 * The translator is the local llama-server already used for corpus NER
 * (entity_enrich) and triage — core/llm.h. No new dependency, no per-token
 * cost, and the same degradation story: llm_healthy() says no, we do nothing
 * this tick and come back later. Ingest is never blocked; core/intel.c's
 * emit() path is untouched and knows nothing about this module.
 *
 * ── FTS DECISION: English is appended into the existing `keywords` column ──
 *
 * intel_items_fts WAS fts5(uid UNINDEXED, title, body, summary, keywords,
 * tokenize='unicode61 remove_diacritics 1'). Two options were on the table.
 *
 * [UPDATE: option (a) was later taken for a DIFFERENT payload — link, author,
 * tags and flattened properties are now real FTS columns, and core/fts_schema.c
 * is the rebuild engine whose absence is lamented below. It answers the
 * resumability objection with atomicity instead (one transaction, rolls back
 * to the intact index, retries next boot) and it CARRIES `keywords` OVER, so
 * everything this file says about English living in `keywords` is still true
 * and still the right call: that text is not derivable from intel_items, so a
 * column of its own would have to be repopulated by re-running the LLM.]
 *
 * (a) Add title_en/body_en/summary_en columns to the FTS table. REJECTED.
 *     FTS5 has no ALTER TABLE ADD COLUMN — changing the column set means
 *     DROP TABLE intel_items_fts; CREATE VIRTUAL TABLE ...; then re-INSERT
 *     every intel_items row (each one a MeCab fts_segment() call on title,
 *     body AND summary) and rewrite every intel_items_fts_uid_map rowid.
 *     That is a full-corpus rebuild: single-writer, hours of wall clock on a
 *     real corpus, and — the disqualifying part — NOT resumable. A rebuild
 *     interrupted at 40% leaves an FTS index that silently answers every
 *     query, alert predicate, export and corpus lookup with a fraction of the
 *     corpus. Making a purely additive feature able to corrupt the primary
 *     search index is not an acceptable trade.
 *     I checked whether a rebuild path already exists so this cost could be
 *     borrowed rather than written: `_fts_meta` (name, fingerprint, columns,
 *     tokenizer, version, rebuilt_at) IS in schema.sql and looks purpose-built
 *     for exactly this. It is dead. schema.sql is generated verbatim from the
 *     live Node database, and `_fts_meta` came along with it; grep across
 *     native/ finds the CREATE TABLE and nothing else — zero readers, zero
 *     writers, no fingerprint computation, no rebuild function anywhere in the
 *     C backend. Option (a) therefore means writing the whole rebuild engine
 *     from scratch, not calling one.
 *
 * (b) Append the English text into the existing `keywords` FTS column. TAKEN.
 *     `keywords` is already declared, already tokenized, already covered by an
 *     unqualified `intel_items_fts MATCH ?` (which searches all indexed
 *     columns) and by snippet(intel_items_fts,-1,...) — so every existing
 *     search surface picks English up with ZERO changes to any query in the
 *     codebase. And it is empty: core/intel.c's fts_write() is the only writer
 *     of this table and it hardcodes keywords='' ("enricher fills later" — the
 *     enricher never landed). Nothing is displaced. Cost is one UPDATE of one
 *     FTS row per translated item: incremental, resumable, and individually
 *     cheap. No rebuild, no migration, no window where search is wrong.
 *
 *     What (b) gives up, honestly: you cannot column-qualify a query as
 *     `keywords:tokyo` and have it mean "English only" — English and any
 *     future keyword payload share one column. That costs nothing today (no
 *     caller column-qualifies) and if a keyword enricher ever lands it must
 *     co-write this column rather than clobber it. Noted here so it is found.
 *
 * ── KEEPING FTS IN SYNC WITHOUT TOUCHING intel.c ──
 *
 * fts_write() DELETEs and re-INSERTs the whole FTS row on every upsert, with
 * keywords='' — so a scheduled collector re-emitting an unchanged item wipes
 * its English from the index. We cannot hook intel.c. The fix uses the one
 * signal the re-insert leaves behind: the new FTS row gets a fresh, strictly
 * larger rowid, and intel_items_fts_uid_map.rowid is updated to it. So
 * translate_fts_resync() keeps a rowid watermark and only has to look at map
 * rows above it — O(items re-ingested since the last pass), not O(corpus).
 * The watermark is seeded to MAX(rowid) on first ever run (nothing is
 * translated yet, so nothing can be stale), which is why enabling this module
 * does NOT trigger a corpus-wide sweep at boot.
 *
 * ── LANGUAGE DETECTION ──
 *
 * intel_items.language is frequently NULL and, where present, untrustworthy
 * (collectors set it from feed metadata). Sending English rows to the LLM is
 * pure cost for zero benefit, so language is not trusted on its own. See
 * translate_is_japanese() for the exact rule and threshold.
 *
 * ── MACHINE OUTPUT IS LABELLED, ORIGINALS ARE NEVER TOUCHED ──
 *
 * The _en columns are additive; title/body/summary are never written by this
 * module. translate_shape_items() attaches the translation as a separate
 * `translation` sub-object carrying "machine": true and the engine id, so the
 * UI can badge it. ?lang_view defaults to `ja`, for which the shaper returns
 * NULL and the caller emits the original bytes — existing clients see no
 * change at all, not even a reordered key.
 *
 * ═══ DDL REQUIRED ═════════════════════════════════════════════════════════
 *
 * NEW COLUMNS on intel_items — these are ensure_column() calls, NOT ALTERs in
 * schema.sql (an ALTER there fails on the second boot with "duplicate column
 * name", sqlite3_exec abandons the rest of the script and the process never
 * starts):
 *
 *   ensure_column(db, "intel_items", "title_en",         "TEXT");
 *   ensure_column(db, "intel_items", "body_en",          "TEXT");
 *   ensure_column(db, "intel_items", "summary_en",       "TEXT");
 *   ensure_column(db, "intel_items", "translated_at",    "TEXT");
 *   ensure_column(db, "intel_items", "translate_failed", "INTEGER DEFAULT 0");
 *
 * NEW TABLE + INDEXES:
 *
 *   CREATE TABLE IF NOT EXISTS translate_state (
 *     k TEXT PRIMARY KEY,
 *     v TEXT NOT NULL
 *   );
 *
 *   -- The pending-work index. Partial, so it holds ONLY rows that still need
 *   -- an LLM call: translated rows drop out (translated_at IS NOT NULL),
 *   -- permanently-failing rows drop out (translate_failed >= 3), and rows the
 *   -- detector judged non-Japanese drop out (translate_failed = -1, the
 *   -- "not applicable" sentinel). On a corpus that is fully drained this
 *   -- index is empty and each pod tick costs one empty index seek.
 *   -- The literal 3 is duplicated verbatim in every pending query (see
 *   -- TR_PENDING_WHERE in translate.c) because SQLite only uses a partial
 *   -- index when the query's WHERE syntactically implies the index's — a
 *   -- bound parameter never can. That is why the retry cap is a compiled-in
 *   -- constant and NOT an environment variable.
 *   CREATE INDEX IF NOT EXISTS idx_intel_items_translate_pending
 *     ON intel_items(fetched_at DESC)
 *     WHERE translated_at IS NULL AND translate_failed >= 0
 *       AND translate_failed < 3;
 *
 *   -- intel_items_fts_uid_map is WITHOUT ROWID keyed on uid, so its `rowid`
 *   -- column is an ordinary indexed-nowhere integer. The FTS resync scans it
 *   -- by rowid watermark; without this index that scan is O(corpus).
 *   CREATE INDEX IF NOT EXISTS idx_intel_items_fts_uid_map_rowid
 *     ON intel_items_fts_uid_map(rowid);
 *
 * ORDERING HAZARD: idx_intel_items_translate_pending references columns that
 * do not exist until the ensure_column() calls have run, and schema.sql is
 * executed by db_open() BEFORE the boot-migration block. Putting that CREATE
 * INDEX in schema.sql therefore bricks the first boot. Do not. Call
 * translate_migrate() instead — it performs every statement above in the
 * correct order and is idempotent.
 *
 * ═══ WIRING THE ORCHESTRATOR MUST DO ══════════════════════════════════════
 *
 * 1. db.c, boot-migration block, after the existing ensure_column() calls:
 *        #include "translate.h"
 *        translate_migrate(db);
 *    (This is the only required wiring. The module also self-heals: every
 *    entry point below runs translate_migrate() once on first use, so a
 *    missed call degrades to a one-time lazy migration rather than SQL
 *    errors. Wire it anyway — doing schema work on the first HTTP request is
 *    not where you want it.)
 *
 * 2. Scheduled operation: NONE. translate.c registers its own source_def
 *    ("translate", collector "_enrich", 600s) via REGISTER_SOURCE, exactly
 *    like collectors/sources/llm_enricher.c. The Makefile globs "core" .c so
 *    the constructor is linked in automatically. The underscore collector
 *    prefix makes scheduler.c skip fetch_log_write() and anomaly_detect() —
 *    required, since this pod emits zero intel_items and would otherwise log
 *    records=0 forever and trip records_drop against its own baseline.
 *
 * 3. httpd.c, operator/admin-gated routes (role owner|admin — this spends
 *    real GPU time and walks the whole corpus):
 *        POST /api/admin/translate/backfill
 *             body {"max_items": N}   (N<=0 → unlimited)
 *          → translate_backfill_start(db, max_items, &status)
 *        GET  /api/admin/translate/backfill
 *          → translate_backfill_status()                       (always 200)
 *        DELETE /api/admin/translate/backfill
 *          → translate_backfill_cancel(&status)
 *    The 202 response carries "request_id"; the EXISTING pre-auth SSE route
 *    /api/search/stream/:id streams its progress with no new plumbing,
 *    because the backfill reports through core/progress.h.
 *
 * 4. intelapi wiring for ?lang_view= (in httpd.c, at the call site — do NOT
 *    edit intelapi.c). Post-process the envelope intelapi already returned:
 *        translate_view v = translate_view_parse(qs_lang_view);
 *        char *shaped = translate_shape_items(db, body, v);
 *        if (shaped) { free(body); body = shaped; }
 *    translate_shape_items() returns NULL for the default view (and on any
 *    error), so the untouched original string is what ships. Works for both
 *    {"data":{...}} and {"data":[...]} envelopes.
 *
 * ═══ ENVIRONMENT ══════════════════════════════════════════════════════════
 *   LLM_ENABLED=true          required for the pod to translate (== enricher)
 *   TRANSLATE_ENABLED=false   kill switch; anything else = on when LLM is on
 *   TRANSLATE_BATCH           rows per pod tick / per backfill batch (25)
 *   TRANSLATE_RESYNC_BATCH    FTS map rows per resync pass (2000)
 *   TRANSLATE_BODY_MAX        bytes of body sent to the LLM (4000)
 *   TRANSLATE_MAX_TOKENS      generation cap (3072)
 *   TRANSLATE_TIMEOUT_MS      per-item LLM timeout (120000)
 *   TRANSLATE_JP_RATIO_PCT    detector threshold, percent (20)
 *   TRANSLATE_JP_MIN_CJK      detector floor, codepoints (4)
 */
#ifndef JO_TRANSLATE_H
#define JO_TRANSLATE_H

#include "db.h"
#include "llm.h"

/* Apply every column/table/index this module needs, in dependency order.
 * Idempotent (ensure_column probes PRAGMA table_info; everything else is
 * IF NOT EXISTS). Safe to call on every boot; safe to call twice. */
void translate_migrate(db_handle *db);

/* THE DETECTOR. Returns 1 if this item should be treated as Japanese.
 *
 * Rule, over the concatenation of title+summary+body:
 *   cjk    = codepoints in hiragana U+3040-309F, katakana U+30A0-30FF,
 *            CJK U+4E00-9FFF, CJK ext-A U+3400-4DBF, halfwidth katakana
 *            U+FF66-FF9F   (the same set core/fts.c uses for JP detection)
 *   latin  = ASCII letters
 *   japanese  iff  cjk >= 4  AND  cjk*100 >= (cjk+latin)*20
 *   OR        iff  `language` begins with "ja" AND cjk >= 1
 *
 * WHY 20%. Digits, punctuation, whitespace and symbols are excluded from the
 * denominator, so the ratio is CJK share of *letter* mass. Real Japanese
 * OSINT text sits at 60-95%; the tail that sits low is Japanese wrapped
 * around Latin payload (URLs, romanised names, product codes, ticker
 * symbols), which is common in this corpus and must still be translated.
 * English text contaminated with a Japanese proper noun — an English article
 * mentioning トヨタ — sits under 10%. 20% separates those two populations
 * with margin on both sides while erring, deliberately, toward translating:
 * a wrongly-translated English row costs one LLM call, a wrongly-skipped
 * Japanese row costs the analyst the feature.
 *
 * WHY THE FLOOR OF 4. Without it a two-character Latin string containing one
 * kana is 33% and "Japanese". Four CJK codepoints is roughly the shortest
 * genuinely Japanese title in the corpus (地震速報).
 *
 * WHY `language` IS NOT TRUSTED ALONE. It is NULL on most rows and derived
 * from feed metadata where present. It is used only as a positive signal
 * (lowering the floor to 1), never as a veto: a row tagged "en" whose text is
 * plainly Japanese is translated.
 *
 * Any argument may be NULL. Thresholds are env-overridable (see above). */
int translate_is_japanese(const char *language, const char *title,
                          const char *summary, const char *body);

/* ONE BATCH of translation work. Selects up to `limit` (<=0 → TRANSLATE_BATCH,
 * default 25) intel_items rows that have no translation, have not exhausted
 * their retries and were not already judged non-Japanese, newest-first via
 * idx_intel_items_translate_pending.
 *
 * Discipline (mirrors core/entity_enrich.c): the pending batch is read and the
 * statement finalized BEFORE any LLM call, so no read cursor is held across a
 * 30-second generation; each row's writes are their own short transaction; a
 * row that fails gets translate_failed incremented and is abandoned for good
 * at 3 attempts rather than burning a batch slot forever; a row the detector
 * rejects is stamped translate_failed = -1 and never looked at again.
 *
 * Returns the number of rows translated. Returns 0 immediately — no error, no
 * spin — when llm is NULL or llm_healthy() is false. `cancel` may be NULL;
 * when non-NULL a non-zero value stops the batch between rows (never mid-
 * transaction). */
int translate_run(db_handle *db, llm_client *llm, int limit,
                  volatile int *cancel);

/* Re-push English into intel_items_fts for items whose FTS row was recreated
 * by a re-ingest since the last pass (see the watermark discussion above).
 * No LLM involved, so this runs even when llama-server is down. Examines at
 * most `limit` (<=0 → TRANSLATE_RESYNC_BATCH, default 2000) uid_map rows per
 * call and is fully resumable — the watermark only advances over rows it
 * actually processed. Returns the number of FTS rows rewritten.
 *
 * First-ever call seeds the watermark to MAX(rowid) and returns 0: enabling
 * this module never sweeps an existing corpus. */
int translate_fts_resync(db_handle *db, int limit, volatile int *cancel);

/* ── admin backfill (opt-in, resumable, progress-reported) ───────────────── */

/* Start the historical backfill on a detached thread with its own
 * http_client/llm_client. Loops translate_run() in TRANSLATE_BATCH-sized
 * batches until the corpus is drained, `max_items` rows are done (<=0 →
 * unlimited), the LLM goes unhealthy, or it is cancelled. Because the unit of
 * work is one row and every row's state is durable in intel_items, a crash or
 * a cancel simply stops it — the next start resumes where it left off.
 *
 * NEVER runs automatically: nothing calls this at boot, and the scheduled pod
 * only ever drains one bounded batch per tick.
 *
 * Progress is published through core/progress.h under the returned
 * request_id, so the existing /api/search/stream/:id SSE route streams it.
 *
 * Returns a malloc'd envelope and sets *http_status:
 *   202 {"data":{"request_id":..,"status":"processing","pending":N}}
 *   409 {"error":"backfill_already_running"}
 *   503 {"error":"llm_unavailable"}
 *   500 {"error":"thread_spawn_failed"}
 * Caller frees. Single-flighted process-wide. */
char *translate_backfill_start(db_handle *db, int max_items, int *http_status);

/* Snapshot of the current/last backfill: {"data":{running, request_id,
 * progress:{...progress_to_json...}, pending}}. Always 200, always a malloc'd
 * string. Caller frees. */
char *translate_backfill_status(db_handle *db);

/* Ask the running backfill to stop after the current row. 200 with
 * {"data":{"cancelled":true}}, or 409 {"error":"no_backfill_running"}.
 * Caller frees. */
char *translate_backfill_cancel(int *http_status);

/* ── ?lang_view=en|ja|both ───────────────────────────────────────────────── */

typedef enum {
  TRANSLATE_VIEW_JA = 0,   /* default — original only, response untouched */
  TRANSLATE_VIEW_EN,       /* attach translation, mark it preferred       */
  TRANSLATE_VIEW_BOTH      /* attach translation alongside the original   */
} translate_view;

/* "en" → EN, "both" → BOTH, everything else (including NULL, "ja", garbage)
 * → JA. Unknown values deliberately fall back to the default rather than
 * erroring: a typo in a query string must not 400 an items list. */
translate_view translate_view_parse(const char *lang_view);

/* Shape an ALREADY-BUILT intelapi envelope for `view`.
 *
 * Returns NULL — meaning "ship the original bytes unchanged" — when view is
 * TRANSLATE_VIEW_JA, when the input is not a parseable envelope, or on any
 * internal failure. This is the whole guarantee that existing clients are
 * unaffected: the default path never re-serializes the response.
 *
 * Otherwise returns a malloc'd JSON string (caller frees) in which every item
 * under "data" (object or array) gains a "translation" key — either null, or:
 *   {"lang":"en","machine":true,"engine":"<llm model id>",
 *    "translated_at":"...","title":...,"summary":...[,"body":...]}
 * `body` is included only when the item itself carried a body (i.e. the full
 * single-item view), so list responses are not bloated. Original title/body/
 * summary/language are never modified or removed.
 *
 * The envelope also gains meta.lang_view ("en"|"both"); with "en" the client
 * should render the translation as primary and the original as the fallback
 * for rows where "translation" is null.
 *
 * TENANCY: this takes no tenant_id and its lookups are keyed on uid alone.
 * That is safe *only* because of how it is wired — it post-processes an
 * envelope intelapi already built under a tenant_id predicate, so every uid it
 * can see is one the caller was already authorized to receive, and it never
 * discovers a uid on its own. Do NOT repurpose it to shape a document whose
 * uids came from anywhere else; pass a tenant_id and filter on it if you do.
 * (translate_run/translate_fts_resync are likewise corpus-wide with no tenant
 * predicate, the same stance as entity_enrich.c and entity_stats.c: they are
 * background maintenance over the whole intel_items table, not request
 * handlers, and they expose nothing — they only write derived columns back
 * onto the row they read.) */
char *translate_shape_items(db_handle *db, const char *envelope_json,
                            translate_view view);

#endif /* JO_TRANSLATE_H */
