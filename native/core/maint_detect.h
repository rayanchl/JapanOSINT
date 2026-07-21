/* core/maint_detect.h — maintenance pod Stage 0 (fetch logging) + Stage 1
 * (rule-based anomaly detection), run inline by the scheduler when a collector
 * run finishes (the run result is in hand there — detection is not a polling
 * worker). Ports the actionable subset of the deleted Node detection module:
 * the LLM payload-sanity verdict (sanity_failed) is deferred; the three
 * deterministic verdicts below cover the repairable classes. */
#ifndef JO_MAINT_DETECT_H
#define JO_MAINT_DETECT_H

#include "db.h"

/* Stage 0: one INSERT into fetch_log. status is "ok" | "error". `error` may be
 * NULL. Returns the new fetch_log rowid, or -1 on failure. */
long fetch_log_write(db_handle *db, const char *source_id, const char *status,
                     int records, long duration_ms, const char *error);

/* Stage 1: rule-based detection. Opens at most one anomaly per source (deduped
 * against any currently-open anomaly via idx_anomaly_open):
 *   - status_bad       when the run errored
 *   - records_drop     when records < baseline_mean*(1-DETECT_DROP_FRAC) and a
 *                      baseline exists (mean of recent healthy fetch_log rows)
 *   - duration_outlier when duration_ms > DETECT_DURATION_OUTLIER_MS
 * status_bad takes precedence; otherwise records_drop, then duration_outlier. */
void anomaly_detect(db_handle *db, const char *source_id, long fetch_log_id,
                    const char *status, int records, long duration_ms);

#endif
