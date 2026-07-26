/* core/source_trust.h — per-source reliability scoring (roadmap item 30).
 *
 * Every input already exists; nothing new is collected. We fold four signals
 * that the platform has always recorded but never combined into a number an
 * analyst can act on:
 *
 *   reliability = 0.40 · success_rate_30d        (fetch_log.status)
 *               + 0.30 · freshness               (sources.last_success vs interval)
 *               + 0.20 · (1 − anomaly_rate_30d)  (collector_anomaly)
 *               + 0.10 · volume_stability        (fetch_log.records_fetched cv)
 *
 * A quarantined source scores a hard 0 regardless of history — quarantine is
 * an explicit operator/maintenance verdict and must not be averaged away by a
 * good 30-day record.
 *
 * DELIBERATE: a source with no fetch_log rows in the window scores NULL, not
 * 0 and not 0.5. "We have no evidence" and "we have evidence it is bad" are
 * different claims, and a fabricated middling grade on 415 sources would make
 * the whole badge untrustworthy. Callers render NULL as "unrated".
 *
 * No new tables. Computed on read; the aggregate is two GROUP BY queries over
 * indexed columns (idx_log_ts, idx_anomaly_unresolved), loaded once per
 * /api/status build the same way statusapi.c's load_aggs() works. */
#ifndef JO_SOURCE_TRUST_H
#define JO_SOURCE_TRUST_H
#include "db.h"

typedef struct {
  char   source_id[256];
  int    rated;             /* 0 → no evidence in window; score/grade are N/A */
  double reliability;       /* 0..1 */
  char   grade[2];          /* "A".."F" */
  /* components, exposed so the UI can explain the score rather than assert it */
  double success_rate;      /* 0..1 */
  double freshness;         /* 0..1 */
  double anomaly_rate;      /* 0..1 (higher is worse) */
  double volume_stability;  /* 0..1 */
  long   runs_30d;
  long   anomalies_30d;
  int    quarantined;
} source_trust;

/* Load the trust table for every source with fetch history in the window.
 * Returns a malloc'd array (caller frees) and writes its length to *out_n.
 * NULL on SQL failure (caller degrades to unrated). */
source_trust *source_trust_load(db_handle *db, int *out_n);

/* Linear find by source id; NULL when the source has no entry (unrated). */
const source_trust *source_trust_find(const source_trust *tbl, int n,
                                      const char *source_id);

#endif
