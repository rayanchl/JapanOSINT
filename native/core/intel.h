/* core/intel.h — the single intel_sink (port of intelStore.upsertItems +
 * ftsMirror.writeOne). Every source emits here: base upsert (exact Node
 * INSERT/ON CONFLICT incl. geom_source='llm' preservation) + FTS mirror with
 * MeCab segmentation, in one transaction. */
#ifndef JO_INTEL_H
#define JO_INTEL_H
#include "../source.h"

/* Build a sink bound to db + source_id (+ optional tenant_id). */
intel_sink intel_sink_make(db_handle *db, const char *source_id,
                           const char *tenant_id);

/* A copy of `base` bound to a different source_id, keeping its db and tenant.
 * Returns 1 and fills `out` on success, 0 if `base` is not an intel sink (the
 * caller should then just use `base`). The caller owns `out` and must
 * intel_sink_free() it.
 *
 * This exists for the entity-pivot path. core/pipeline.c binds ONE sink for the
 * whole run, as "osint-search", so every dispatched service's rows were stored
 * under that id and their uids became "osint-search|<remote_key>" — pivot data
 * was not attributable to the source that produced it, two services sharing a
 * remote_key collided, and /api/intel/items?source=<service> could not find any
 * of it. Rebinding per service makes the pivot path store rows exactly the way
 * the scheduled path does. */
int intel_sink_rebind(const intel_sink *base, const char *source_id,
                      intel_sink *out);

/* BATCHED WRITES (core/intel.c, the block above emit()). A sink made here
 * groups its records into one transaction per JO_SINK_BATCH records (200) or
 * JO_SINK_BATCH_MS (500) of the oldest record's age, whichever comes first; the
 * work that needs no database runs in emit() and nothing is held open between
 * calls. emit() returns 0 for an accepted record (1/0/-1 as source.h says only
 * with JO_SINK_BATCH=1, and always for a sink from intel_sink_rebind()), -1 for
 * an item with no uid, and -1 for a record the database refused — reported
 * LATER, on a following emit of the same kind (record or notice), because the
 * refusal is learned at flush time.
 *
 * Writes whatever is buffered now. Called by intel_sink_stored(),
 * intel_sink_stored_notices() and intel_sink_free(); public for a caller that
 * is about to wait a long time. No-op for a NULL or foreign sink. */
void intel_sink_flush(intel_sink *k);

/* Records the database refused since make(): the TRUE failed count. -1 for a
 * foreign sink. */
long intel_sink_failed(const intel_sink *k);

/* Refusals not yet returned through an emit() as -1 (the last batch's, when no
 * later emit came). A caller that counts emit() failures adds this after
 * reading intel_sink_stored(), which is what flushes. -1 for a foreign sink. */
long intel_sink_failed_pending(const intel_sink *k);

/* Releases the state intel_sink_make() allocated. There was no such call
 * until the scheduler became a worker pool: every make() heap-allocates a
 * sink_state and no caller freed it, so the process leaked one per source run
 * — ~2,000 per refresh cycle, forever, on a daemon that never restarts.
 * Idempotent and NULL-safe; the sink is unusable afterwards. */
void intel_sink_free(intel_sink *k);

/* How many DISTINCT rows this sink has actually left behind since make() —
 * house rule 4b, "emitting is not storing either". The scheduler's
 * `records=N` counts emit() CALLS; a source whose records key onto each other
 * calls emit 12,648 times and leaves 31 rows, and nothing in the tree could
 * see the difference. core/intel.c carries the full definition and the reason
 * it is distinct-uid rather than rows-inserted (the latter reads as total loss
 * on every ordinary re-run of an unchanged feed, and a metric that cries wolf
 * is a metric nobody reads).
 *
 * `*exact` is set to 0 when the count is a FLOOR rather than the truth — the
 * per-run table hit its memory ceiling, or an allocation failed. Report it as
 * `>=N` in that case; do not round it off into a number you did not measure.
 *
 * Returns -1, with *exact = 1, if `k` is not a sink this file made (the
 * scheduler's counting wrapper, a NULL, a freed sink). */
long intel_sink_stored(const intel_sink *k, int *exact);

/* How many of the intel_sink_stored() uids belong to `collector-*-notice`
 * records (see intel_item_is_notice). The scheduler's `records=` excludes
 * notices, so comparing it against a `stored=` that includes them let one
 * notice row hide one collapsed record (rule 4b): 2,000 records + 1 notice,
 * 2 of them colliding, read stored=2000=records. Compare records against
 * stored MINUS this. Same -1 contract as intel_sink_stored(). */
long intel_sink_stored_notices(const intel_sink *k);

/* 1 if `it` is a `collector-*-notice` record — collector-truncation-notice,
 * collector-shape-notice, collector-status-notice (which carries
 * needs_credential): data ABOUT a run, not a record OF the source.
 *
 * ONE definition, used by every path that counts records: the scheduler's run
 * line, the entity-pivot dispatcher (osint_dispatch.c) and the sink's own
 * distinct-uid count. The dispatcher used to have no copy at all, so a keyless
 * pivot that emitted only its needs-credential notice reported success=1,
 * records=1, confidence 70 and fetch_log ok.
 *
 * The `collector-` prefix is required. Matching any record_type ending in
 * "-notice" tallied 806 table rows' real upstream records (municipal-notice,
 * procurement-notice, tender-notice, police-notice …) as notices, so sources
 * that stored thousands of announcements reported records=0. */
int intel_item_is_notice(const intel_item *it);

/* Re-mirror ONE intel_items row into intel_items_fts, reading the values back
 * out of the table. Returns 0 when the row was re-indexed, non-zero if `uid`
 * does not exist (or the read failed) — in which case the index is untouched.
 *
 * WHY THIS IS PUBLIC. emit() is the only INSERT INTO intel_items, but it is
 * not the only writer of the columns that are INDEXED. Two pods UPDATE
 * intel_items.properties directly and used to leave intel_items_fts.props
 * holding the pre-update text: station_clusterer.c (line colours) and
 * camera_geocode_pod.c (the resolved street address). The visible symptom was
 * a geocoded camera whose address shows in the detail pane and is permanently
 * unfindable by search. Any future writer that changes title, body, summary,
 * link, author, tags or properties outside emit() must call this after its
 * UPDATE commits — it is the same code path emit() uses, deliberately, so the
 * two cannot drift.
 *
 * Cost is one indexed SELECT plus the segmentation of that row; it is a
 * per-row repair, not a bulk one. `keywords` is reset exactly as the ingest
 * path resets it (translate.c's rowid watermark is what restores it). */
int intel_fts_remirror(db_handle *db, const char *uid);

#endif
