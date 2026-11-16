/* core/station_merges.{c,h} — the WRITER and the review queue for
 * llm_station_merges.
 *
 * WHY THIS EXISTS. core/station_clusterer.c's pass 3 has read
 * llm_station_merges since the port ("trusted LLM merges"), and nothing in
 * the C tree wrote it — the Node judge that once did is gone (found
 * 2026-11-12; the owner's decision is "wire writers"). Two things were
 * missing, and both are here:
 *
 *   1. A JUDGE. The clusterer hands over the pairs its own name rule could not
 *      decide — two stations within its 150 m radius whose fingerprints are
 *      near but not near enough (station_clusterer.c, near_miss()). Each is
 *      put to the generation server (core/llm.c, the background lane), the
 *      station records fenced as untrusted text because their names are
 *      third-party data. The answer is RECORDED, never applied: every row is
 *      written status='pending' with the model that gave it and its reason.
 *
 *   2. A REVIEW. GET /api/admin/station-merges?status=pending lists them, and
 *      POST /api/admin/station-merges/:id/approve | reject decides, behind the
 *      platform-operator gate and audited ("station_merge.approve" /
 *      ".reject", old and new status).
 *
 * WHAT THE READER APPLIES. Only same = 1 AND status = 'approved'. The old
 * reader trusted `same = 1 AND confidence >= 0.7` from an unreviewed model;
 * that confidence is now advice to the reviewer, not a gate, because an
 * approval is a person's decision and a merge rewrites which stations the map
 * draws as one. An approved `same = 0` row is the operator agreeing the two
 * are distinct and merges nothing; a rejected row merges nothing either. A
 * decision takes effect on the clusterer's next run (it rebuilds from scratch
 * each run), and may be reversed by deciding the other way.
 *
 * ROWS FROM BEFORE THIS. The migration adds status with DEFAULT 'pending', so
 * any row a past writer left (the Node judge) stops being applied until an
 * operator approves it. Those rows were never reviewed, which is exactly what
 * the queue is for; the count is in the list's meta.
 *
 * HONEST EMPTY. With LLM_BASE_URL unset (llm.c's own variable; llm_init would
 * otherwise fall back to localhost) the judge does not run and writes nothing.
 * When the server cannot say which model it serves (GET /v1/models) the batch
 * is skipped — a verdict with no model identity cannot be weighed — and an
 * outage mid-batch stops the batch, leaving the remaining pairs unasked for
 * the next run (core/llm_fault.h's split). An answer that does not parse as a
 * verdict is counted and not stored: nothing is invented for it. */
#ifndef JO_STATION_MERGES_H
#define JO_STATION_MERGES_H

#include <stddef.h>
#include "db.h"

/* Idempotent: the table if absent, then the columns this module adds (id,
 * status, model, reviewed_by, reviewed_at, name_a, name_b, dist_m) and the
 * index the review list reads. The clusterer calls it before its reader. */
void station_merges_migrate(db_handle *db);

/* One ambiguous pair, as the clusterer saw it. Pointers are borrowed. */
typedef struct {
  const char *uid_a, *uid_b;
  const char *name_a, *name_b;
  const char *name_ja_a, *name_ja_b;
  const char *mode_a, *mode_b;
  const char *operator_a, *operator_b;
  const char *line_a, *line_b;
  double dist_m;
} station_merge_pair;

/* 1 when the judge may run: LLM_BASE_URL set and the per-run budget > 0. */
int station_merges_judge_enabled(void);
/* Pairs asked per clusterer run: $JO_STATION_JUDGE_MAX, default 20, 0 = off,
 * at most 500 (an unbounded value would hold the clusterer for hours). */
int station_merges_judge_budget(void);

/* 1 when a row already exists for the pair (in either order). */
int station_merges_known(db_handle *db, const char *uid_a, const char *uid_b);

/* INSERT OR IGNORE one verdict as 'pending' — an existing row (and whatever
 * review it carries) is never overwritten. 1 inserted, 0 existed, -1 error. */
int station_merges_record(db_handle *db, const station_merge_pair *p, int same,
                          double confidence, const char *reason,
                          const char *model);

/* Ask the judge about pairs[0..n), at most station_merges_judge_budget() of
 * them, skipping pairs already recorded. Returns the number recorded, or -1
 * when it did not run (disabled / no model identity). *asked and *unparsed
 * (may be NULL) report the calls made and the answers that were not a
 * verdict. */
int station_merges_judge(db_handle *db, const station_merge_pair *pairs, int n,
                         int *asked, int *unparsed);

/* GET /api/admin/station-merges?status=&limit=&offset=. `status` is pending
 * (default), approved, rejected or all. */
char *station_merges_api_list(db_handle *db, const char *status, int limit,
                              int offset, int *http_status);
/* POST /api/admin/station-merges/:id/approve|reject. Body optional:
 * {"note": "..."} goes to the audit row. */
char *station_merges_api_review(db_handle *db, const char *id, int approve,
                                const char *body, size_t body_len,
                                const char *user_id, int *http_status);

#endif
