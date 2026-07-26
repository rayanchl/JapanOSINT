/* core/simhash.h — cross-source near-duplicate detection and clustering
 * (roadmap item 25). With 415 registered sources one earthquake, one JPCERT
 * advisory, one ministry press release arrives twenty times, reworded by
 * twenty aggregators. The feed is then twenty rows about one event and the
 * analyst reads the same thing twenty times.
 *
 * WHY SIMHASH AND NOT SOMETHING ELSE
 *
 *  - Exact-hash dedup (uid / link / md5 of body) already happens upstream and
 *    catches nothing here: the twenty copies are genuinely different byte
 *    strings — different lead paragraphs, different boilerplate, different
 *    translations of the same JMA bulletin.
 *  - Pairwise similarity (cosine over tf-idf, edit distance) is O(n^2) over a
 *    corpus this size and cannot be run on the ingest path at all.
 *  - Simhash (Charikar) maps a document to a 64-bit fingerprint whose HAMMING
 *    DISTANCE tracks cosine similarity of the underlying token vector. It is
 *    one integer per row, it is computed in one pass over tokens we already
 *    have, and — critically — near-duplicates can be FOUND without comparing
 *    every pair, via the banded index described below.
 *
 * WHY fts_segment() AND NOT A LOCAL TOKENIZER. The fingerprint is built from
 * the same MeCab surface forms core/intel.c's fts_write() feeds into
 * intel_items_fts. Two different tokenizers would draw two different token
 * vectors, so "these two items cluster together" and "these two items match
 * the same search" would stop being the same statement about the same text.
 * A clustering that disagrees with search is worse than none: the analyst
 * searches, gets a hit, opens the cluster, and the cluster contains items the
 * search would never have returned.
 *
 * ─── THE BANDED INDEX, AND WHY 4 BANDS CATCH EVERY PAIR WITHIN DISTANCE 3 ───
 *
 * A full pairwise scan is not an option, so candidates come from an LSH-style
 * banded index. The 64 bits are cut into SIMHASH_BANDS (4) DISJOINT bands of
 * SIMHASH_BAND_BITS (16) bits; band b of fingerprint h is (h >> 16*b) & 0xFFFF
 * and one row per (band_idx, band_val, uid) goes into simhash_bands.
 *
 * PIGEONHOLE ARGUMENT. Let x and y be fingerprints with Hamming distance
 * d(x,y) <= SIMHASH_MAX_DISTANCE (3). The differing bit positions form a set D
 * with |D| <= 3. The 4 bands PARTITION the 64 bit positions, so each element
 * of D lies in exactly one band; 3 elements can therefore touch AT MOST 3 of
 * the 4 bands. By pigeonhole at least one band contains no element of D, and
 * in that band x and y are bit-for-bit equal — so they share a
 * (band_idx, band_val) key and y is returned by x's band lookup.
 *
 * Hence: querying all 4 bands yields a candidate superset with ZERO false
 * negatives for d <= 3. It also yields false positives (two fingerprints can
 * share a 16-bit band and be 30 bits apart), which is why every candidate is
 * confirmed with a true popcount(x ^ y) <= 3 before it is allowed to cluster.
 * The generalisation is the standard one: k bands catch every pair within
 * distance k-1. Raising SIMHASH_MAX_DISTANCE above 3 WITHOUT raising
 * SIMHASH_BANDS silently turns the index lossy — the recall guarantee is a
 * property of the pair (bands, distance), not of either alone.
 *
 * Cost: 4 index seeks per item instead of one scan of the corpus. Expected
 * bucket occupancy at N rows is N / 65536 per band, so 200k items average ~3
 * candidates per band. Degenerate buckets are bounded by
 * SIMHASH_MAX_CANDIDATES (see below).
 *
 * ─── THE MINIMUM TOKEN FLOOR (the single most important knob here) ──────────
 *
 * A simhash of an empty document is all-zero, and all-zero is at distance 0
 * from every other empty document. A simhash of a two-token document is
 * decided by two votes, so two items sharing one token land a handful of bits
 * apart. Left alone, a clustering feature merges every one-line item in the
 * corpus into one enormous cluster — every "更新情報", every "Weekly summary",
 * every empty-bodied RSS stub from twenty different feeds — and that is
 * strictly worse than having no clustering at all, because it is a confident
 * false claim rather than an absence.
 *
 * So: an item is CLUSTERABLE only if it yields at least SIMHASH_MIN_TOKENS
 * (12) DISTINCT content tokens after stopword and punctuation removal.
 * Rationale for 12 rather than a smaller number: replacing one token out of k
 * flips a bit position only where the remaining k-1 votes were within one of
 * a tie, which happens for roughly sqrt(2/(pi*(k-1))) of the 64 positions, so
 * a one-token difference moves about 32*sqrt(2/(pi*(k-1))) bits — ~7.7 bits at
 * k=12, ~4 bits at k=40, ~2.5 bits at k=100. The 3-bit threshold therefore
 * means "differs by at most about one token in a hundred" for a real article
 * and "not even a single token differs" for a short one. Below k=12 the
 * fingerprint stops being a statement about content and becomes a statement
 * about which two or three stopword-adjacent tokens survived.
 *
 * Sub-floor items are NOT left unprocessed (that would make the backfill
 * non-terminating: it resumes on `simhash IS NULL`). They are stamped with the
 * sentinel simhash = 0, get cluster_id = NULL and no simhash_bands rows, and
 * are never returned as candidates. Reading the column:
 *     NULL -> not yet fingerprinted (backfill pending)
 *     0    -> fingerprinted, deliberately excluded from clustering
 *     else -> the fingerprint
 * A real document whose fingerprint happens to be exactly 0 (probability
 * 2^-64) is treated as unclusterable. That is the correct trade: one lost
 * cluster membership versus a second sentinel column.
 *
 * ─── CLUSTER IDENTITY AND REPRESENTATIVE MIGRATION ─────────────────────────
 *
 * INVARIANT: cluster_id is the uid of the EARLIEST-PUBLISHED member, keyed on
 * COALESCE(published_at, fetched_at), ties broken by uid ASC. "Who reported it
 * first" is the interesting fact about a corroborated story, so the earliest
 * member is the natural representative and the identifier is stable, readable
 * and needs no extra table.
 *
 * The obvious hazard is write amplification: when a new item is OLDER than an
 * existing cluster's representative, the representative must move and EVERY
 * member has to be relabelled, or the cluster silently splits in two. Four
 * things keep that bounded:
 *
 *  1. Relabelling is one indexed statement per absorbed cluster —
 *     UPDATE intel_items SET cluster_id=? WHERE cluster_id=? AND tenant_id=?
 *     driven by idx_intel_items_cluster. It costs O(members of that cluster),
 *     never a table scan, and followers move with their representative for
 *     free because they are addressed BY cluster_id, not enumerated.
 *  2. In the live feed the new item is almost always the newest thing in the
 *     cluster, so the representative does not move and the whole operation is
 *     one UPDATE of one row.
 *  3. THE BACKFILL PROCESSES ROWS IN ASCENDING PUBLICATION ORDER. This is the
 *     design's main trick and the reason the longest-running migration in the
 *     roadmap has no write amplification at all: a row being fingerprinted is
 *     never older than a row already fingerprinted, so it can never displace
 *     an existing representative. Every backfilled row costs exactly 4 band
 *     inserts and 1 single-row UPDATE, for the entire corpus.
 *  4. A hard ceiling. Before any migration the members to be relabelled are
 *     counted (indexed COUNT); if the total exceeds SIMHASH_MAX_RELABEL the
 *     merge is ABANDONED and the item stays in its own cluster. No single
 *     ingest call can rewrite more than that many rows, ever.
 *
 * Merge fan-out is capped at SIMHASH_MAX_MERGE_CLUSTERS (8) distinct clusters
 * per item, and this is a QUALITY bound as much as a cost bound: "distance
 * <= 3" is not transitive, so unlimited transitive merging lets a chain of
 * pairwise-near items collapse into one meaningless mega-cluster (classic
 * simhash cluster drift). An item that bridges more than 8 clusters is
 * evidence the threshold is wrong for that content, not evidence that nine
 * stories are one story.
 *
 * RE-INGEST. Collectors re-emit unchanged rows on every tick. If the
 * recomputed fingerprint equals the stored one and the row already has a
 * cluster, the call returns after a single indexed SELECT. If the fingerprint
 * changed, bands are refreshed so future items can still find this row, and:
 * a row that is its own representative (cluster_id = uid, or NULL) re-runs the
 * join, bringing its followers with it; a row that is a FOLLOWER
 * (cluster_id != uid) keeps its membership. Letting a follower defect would
 * require re-deciding the membership of everything that joined because of it,
 * which is a graph recomputation the ingest path cannot afford. To force a
 * full re-cluster of the corpus:
 *     DELETE FROM simhash_bands;
 *     UPDATE intel_items SET simhash=NULL, cluster_id=NULL;
 * and let the backfill pod rebuild it in publication order.
 *
 * TENANCY. Clustering is strictly per-tenant: the candidate query joins
 * intel_items and filters tenant_id, the relabel statements carry
 * tenant_id, and the collapse reads carry it too. simhash_bands deliberately
 * has NO tenant column — it is a pure LSH index over fingerprints, and a
 * denormalised tenant there could drift out of step with intel_items and turn
 * a stale row into a cross-tenant leak. The tenant predicate lives exactly
 * once, on the join to the authoritative table, and that join is needed anyway
 * to read the candidate's fingerprint.
 *
 * ─── REQUIRED DDL ──────────────────────────────────────────────────────────
 *
 * (A) core/schema.sql — SAFE to add verbatim (no dependency on new columns):
 *
 *   CREATE TABLE IF NOT EXISTS simhash_bands (
 *     band_idx INTEGER NOT NULL, band_val INTEGER NOT NULL, uid TEXT NOT NULL,
 *     PRIMARY KEY (band_idx, band_val, uid)) WITHOUT ROWID;
 *   CREATE INDEX IF NOT EXISTS idx_simhash_bands_lookup
 *     ON simhash_bands(band_idx, band_val);
 *   CREATE INDEX IF NOT EXISTS idx_simhash_bands_uid ON simhash_bands(uid);
 *
 *   (WITHOUT ROWID because the whole row IS the key — a rowid table would
 *   store the same three values twice. idx_simhash_bands_lookup makes the
 *   candidate seeks index-only. idx_simhash_bands_uid is required by the
 *   delete-then-reinsert on re-ingest, which is keyed by uid and would
 *   otherwise scan the whole band table on every refreshed item.)
 *
 * (B) Two NEW COLUMNS on intel_items — ensure_column(), NEVER schema.sql:
 *
 *   ensure_column(db, "intel_items", "simhash",    "INTEGER");
 *   ensure_column(db, "intel_items", "cluster_id", "TEXT");
 *
 * (C) Two indexes that REFERENCE THOSE NEW COLUMNS, and therefore MUST NOT go
 *     into schema.sql:
 *
 *   CREATE INDEX IF NOT EXISTS idx_intel_items_cluster
 *     ON intel_items(cluster_id);
 *   CREATE INDEX IF NOT EXISTS idx_intel_items_simhash_todo
 *     ON intel_items(COALESCE(published_at,fetched_at), uid)
 *     WHERE simhash IS NULL;
 *
 *   READ THIS BEFORE MOVING THEM. db_open() hands schema.sql to ONE
 *   sqlite3_exec() BEFORE the boot-migration block runs. On an existing
 *   database the simhash/cluster_id columns do not exist at that moment, so a
 *   CREATE INDEX naming them fails, exec abandons the rest of the script, and
 *   the process never starts. Both indexes are therefore created by
 *   simhash_ensure_schema() below, immediately AFTER its ensure_column()
 *   calls. That function is idempotent and does (A), (B) and (C) itself, so
 *   wiring it into db.c is sufficient and (A) in schema.sql is only an
 *   optimisation for fresh databases.
 *
 *   idx_intel_items_simhash_todo is what makes the backfill linear rather than
 *   quadratic: it satisfies both the `simhash IS NULL` predicate and the
 *   ascending-publication ORDER BY (verified with EXPLAIN QUERY PLAN — no temp
 *   b-tree, no table scan), so each batch is a short index range read instead
 *   of a full scan plus a sort. Without it the sweep re-scans and re-sorts the
 *   whole table once per batch, i.e. O(N^2/batch). Building it costs ONE table
 *   scan the first time simhash_ensure_schema() runs on an existing corpus,
 *   which is a real (one-off) boot delay on a large database and is the price
 *   of the migration not being quadratic. It is PARTIAL, so it shrinks as the
 *   migration proceeds and is empty when the corpus is fully fingerprinted;
 *   steady-state cost is two index touches per ingested row (in on INSERT,
 *   out when the fingerprint is stamped). The indexed expression must be
 *   spelled exactly as the query spells it or SQLite will not match it.
 *
 * ─── WIRING THE ORCHESTRATOR MUST DO ───────────────────────────────────────
 *
 * 1) core/db.c, in db_open()'s boot-migration block, next to the other
 *    ensure_column() calls and BEFORE db_seed_sources(db):
 *
 *        #include "simhash.h"
 *        simhash_ensure_schema(db);
 *
 *    This is the ONLY required schema wiring; it performs (B) then (C) then
 *    (A) in that order. Everything else in this module also calls it lazily
 *    (once per process, mutex-guarded), so a missed call degrades to "the
 *    first ingested row pays for the migration", not to a failure.
 *
 * 2) core/intel.c — the ingest chokepoint. ONE call in emit(), AFTER the
 *    commit, right next to the existing alert_eval_on_item() call:
 *
 *        #include "simhash.h"                     // with the other includes
 *        ...
 *          int changes = sqlite3_changes(h);
 *          fts_write(h, uid, it->title, it->body, it->summary);
 *          sqlite3_exec(h, "COMMIT", NULL, NULL, NULL);
 *
 *          if (changes)                                       // <-- ADD THIS
 *            simhash_on_item(st->db,                          // <-- LINE
 *                            st->tenant_id[0] ? st->tenant_id : "legacy",
 *                            uid, it->title, it->body);
 *
 *          if (changes && is_new)
 *            alert_eval_on_item(...);
 *
 *    NOT gated on is_new, unlike alert_eval: an UPDATE can change title/body,
 *    and a stale fingerprint would cluster the item by text it no longer has.
 *    The unchanged-row case is cheap by construction (see RE-INGEST above).
 *    AFTER the commit, never inside it, for the same reason alert_eval is:
 *    a clustering write must never be able to roll back the ingested row.
 *
 *    ZERO-EXTRA-MECAB VARIANT (optional, later). simhash_on_item() runs
 *    fts_segment() over title and body, duplicating the segmentation
 *    fts_write() just did — MeCab is the dominant cost of both. If the
 *    orchestrator ever hoists the `char *st = fts_segment(title); char *sb =
 *    fts_segment(body);` pair out of fts_write() so the segmented strings
 *    outlive the transaction, call simhash_on_item_segmented(db, tenant, uid,
 *    st, sb) after the COMMIT instead and the fingerprint becomes free. Both
 *    entry points produce identical fingerprints; this is purely a cost
 *    optimisation and is deliberately NOT required, because it would mean
 *    editing the hot path for a saving that only matters at high ingest rates.
 *
 * 3) core/httpd.c — ?collapse=1 on GET /api/intel/items. The collapse is a
 *    post-pass over the envelope intelapi_list_items() already produced, so
 *    every existing filter, the FTS branch, the keyset cursor and the breach
 *    adapter path all keep working untouched:
 *
 *        char *out = intelapi_list_items(db, &q);
 *        const char *cl = <?collapse query param>;
 *        if (out && cl && (!strcmp(cl,"1") || !strcmp(cl,"true"))) {
 *          char *c = simhash_collapse(db, tenant_id, out);
 *          if (c) { free(out); out = c; }        // NULL -> keep uncollapsed
 *        }
 *
 * 4) The backfill pod. NEW FILE native/collectors/sources/simhash_pod.c (the
 *    Makefile globs that directory, so there is no build change):
 *
 *        #include "../../source.h"
 *        #include "../../core/simhash.h"
 *
 *        static int run(const source_ctx *ctx, intel_sink *sink) {
 *          (void)sink;                  // fingerprinting writes in place
 *          return simhash_backfill(ctx->db, 20000, ctx->cancel) < 0 ? -1 : 0;
 *        }
 *
 *        static const source_def simhash_def = {
 *          .id = "simhash-backfill", .collector = "_maint",
 *          .name = "Near-Duplicate Fingerprint Backfill",
 *          .name_ja = "近似重複フィンガープリント補完",
 *          .update_interval_sec = 300, .run = run };
 *        REGISTER_SOURCE(simhash_def)
 *
 *    collector MUST be "_maint" (any leading-underscore name works).
 *    scheduler.c skips fetch_log_write() and anomaly_detect() for
 *    underscore-prefixed collectors, and this job needs the skip on both
 *    counts: it emits zero intel_items, so it would log records=0 forever and
 *    trip records_drop against its own baseline, and a sweep over an
 *    un-fingerprinted corpus runs for minutes, which trips duration_outlier.
 *    Neither is a fault — that is what this job looks like. It also inherits
 *    the scheduler's skip-if-running serialisation, so two sweeps can never
 *    overlap. The per-tick row cap plus the 300s interval bound the migration
 *    to a steady trickle that never competes hard with live ingest for
 *    SQLite's single write lock; once the corpus is fingerprinted each tick
 *    costs one empty index range read.
 *
 * 5) No other httpd wiring. This module exposes no routes of its own.
 */
#ifndef JO_SIMHASH_H
#define JO_SIMHASH_H

#include <stdint.h>
#include "db.h"

/* Fingerprint width is fixed at 64 bits (the column is an SQLite INTEGER).
 * BANDS * BAND_BITS must equal 64, and the recall guarantee requires
 * MAX_DISTANCE < BANDS — see the pigeonhole argument above. */
#define SIMHASH_BANDS         4
#define SIMHASH_BAND_BITS     16
#define SIMHASH_MAX_DISTANCE  3

/* Distinct content tokens (post stopword/punctuation removal) an item must
 * yield to be clustered at all. Below this the fingerprint is degenerate and
 * would merge every one-line item in the corpus; see the long note above.
 * Sub-floor items are stamped simhash=0, cluster_id=NULL. */
#define SIMHASH_MIN_TOKENS    12

/* Token budget per document. Bounds the ingest-path cost of a pathological
 * body; the first 2048 tokens of an article already determine its fingerprint
 * far more precisely than the 3-bit threshold can resolve. */
#define SIMHASH_MAX_TOKENS    2048

/* Title tokens vote this many times. A headline is ~15 tokens against a body
 * of several hundred, so an unweighted fingerprint is essentially a fingerprint
 * of the body's boilerplate. 3 gives the headline real influence (~8% of the
 * vote mass on a 500-token article) without letting a shared stock headline
 * override differing bodies. */
#define SIMHASH_TITLE_WEIGHT  3

/* Candidates examined per item. A 16-bit band bucket is ~N/65536 rows, so this
 * is unreachable in normal data; it is a latency guard against a degenerate
 * bucket. Hitting it costs recall inside that one bucket only. */
#define SIMHASH_MAX_CANDIDATES 256

/* Distinct clusters one item may merge in a single call. Cost bound AND drift
 * bound — "distance <= 3" is not transitive. */
#define SIMHASH_MAX_MERGE_CLUSTERS 8

/* Hard ceiling on rows relabelled by one representative migration. Beyond it
 * the merge is abandoned and the item stays in its own cluster: an unbounded
 * rewrite on the ingest path is a worse outcome than a missed merge. */
#define SIMHASH_MAX_RELABEL 5000

/* Rows fingerprinted per backfill write transaction. Segmentation and hashing
 * happen OUTSIDE the transaction; only the writes are inside it, so a batch
 * commits in single-digit milliseconds and never stalls httpd's writers or
 * blows up the WAL. */
#define SIMHASH_BATCH 200

/* Duplicates enumerated per cluster in a collapsed response. cluster_size
 * still reports the TRUE total and duplicates_truncated says the list was cut,
 * so corroboration is never under-reported — only the enumeration is bounded. */
#define SIMHASH_MAX_DUPLICATES_LISTED 50

/* Idempotent: ensure_column() for intel_items.simhash / .cluster_id, then the
 * two indexes that depend on them, then simhash_bands + its indexes. Safe on
 * every boot and from every thread; runs its DDL at most once per process.
 * Never fails loudly — a failure degrades this module to a no-op. */
void simhash_ensure_schema(db_handle *db);

/* Pure fingerprint over ALREADY-SEGMENTED text (either argument may be NULL).
 * Returns the 64-bit simhash, or 0 when the document is unclusterable.
 * `out_tokens`, if non-NULL, receives the DISTINCT content-token count, which
 * is what SIMHASH_MIN_TOKENS is compared against. No DB, no allocation,
 * thread-safe. Exposed for tests and for callers that already hold segmented
 * text. */
uint64_t simhash_text(const char *seg_title, const char *seg_body,
                      int *out_tokens);

/* Hamming distance between two fingerprints. */
int simhash_distance(uint64_t a, uint64_t b);

/* THE INGEST HOOK. Fingerprints the item, refreshes its band rows and settles
 * its cluster membership. Call AFTER the ingest transaction has committed
 * (see wiring note 2).
 *
 * Cheap and total by contract: it swallows every SQLite error, never aborts,
 * never leaves a transaction open, and returns silently when the uid has no
 * intel_items row. Losing a cluster edge is survivable; failing the ingest
 * that produced the row is not. If a transaction is already open on this
 * connection it joins it instead of nesting a BEGIN.
 *
 * `tenant_id` is a hint only — the row's own tenant_id is read back and is
 * authoritative for candidate selection and relabelling, so a mislabelled call
 * can never cluster one tenant's items into another's. `title`/`body` are RAW
 * text; they are segmented here. */
void simhash_on_item(db_handle *db, const char *tenant_id, const char *uid,
                     const char *title, const char *body);

/* Same, for callers that already hold MeCab-segmented title/body (see the
 * zero-extra-MeCab note in wiring 2). Identical fingerprints, one less pass. */
void simhash_on_item_segmented(db_handle *db, const char *tenant_id,
                               const char *uid, const char *seg_title,
                               const char *seg_body);

/* ?collapse=1 — collapse an intelapi_list_items() envelope to one row per
 * cluster. `list_json` is the malloc'd envelope from intelapi_list_items()
 * (or breach_adapter_list(); same shape). Returns a NEW malloc'd envelope,
 * caller frees BOTH, or NULL if the input is not a recognisable envelope, in
 * which case the caller keeps the uncollapsed one.
 *
 * Each surviving row gains four keys, appended after its existing ones:
 *   "cluster_id"           uid of the earliest-published member
 *   "cluster_size"         TOTAL members corpus-wide, not just on this page
 *   "cluster_source_count" distinct source_ids in the cluster
 *   "duplicates"           [{"source_id":…,"uid":…}, …] — the OTHER members,
 *                          publication order, at most
 *                          SIMHASH_MAX_DUPLICATES_LISTED entries
 *   "duplicates_truncated" true when that cap cut the list
 *
 * `duplicates` is the POINT of this feature, not a leftover. "Twenty
 * independent sources reported this" is corroboration and is often the most
 * valuable thing on the row; the duplicates are folded into the row, never
 * dropped. Membership is read from the whole corpus by cluster_id, NOT from
 * the current page, so a cluster whose other nineteen members are on page
 * seven still reports twenty.
 *
 * Rows whose uid has no intel_items row (breach adapter synthetics) and rows
 * not yet fingerprinted are passed through as singletons — collapse is always
 * safe to enable, including mid-backfill.
 *
 * `page` is left untouched ON PURPOSE: the cursor is a property of the
 * UNDERLYING scan, so paging stays lossless and complete even though `data`
 * is shorter than `limit`. meta.collapse reports rows_in/rows_out so a client
 * can tell how much was folded. Singletons keep the same shape (size 1, empty
 * duplicates) so a client never has to branch. */
char *simhash_collapse(db_handle *db, const char *tenant_id,
                       const char *list_json);

/* Resumable backfill for rows with simhash IS NULL, in ASCENDING publication
 * order — which is what makes it free of representative migration (see point
 * 3 of the migration note above).
 *
 * `limit` caps rows processed by THIS call (<=0 means "until exhausted or
 * cancelled"). Work is committed in SIMHASH_BATCH-row transactions, with
 * segmentation and hashing done outside them, so no single transaction is ever
 * held for more than a few milliseconds and a kill at any moment loses at most
 * one batch. `cancel` may be NULL; when non-NULL it is polled between rows and
 * between batches, and a set flag stops after the in-flight batch commits —
 * the return value is then the count actually written, not an error.
 *
 * Returns rows fingerprinted, or -1 if the schema could not be prepared. A
 * return of 0 means the corpus is fully fingerprinted, which is the normal
 * steady state and costs one empty index range read. */
long simhash_backfill(db_handle *db, int limit, volatile int *cancel);

#endif
