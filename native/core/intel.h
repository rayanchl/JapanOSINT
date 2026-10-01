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
