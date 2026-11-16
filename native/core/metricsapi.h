/* core/metricsapi.h — GET /api/metrics (OPS-14): a Prometheus text exposition
 * of a few cheap gauges/counters that already exist as accessible state.
 *
 * Only measured values are exposed; a number with no cheap source is left out
 * rather than guessed (the scheduler's queue depth and the HTTP worker count
 * are not exported by scheduler.h / httpd.c, so they are not here).
 *
 *   japanosint_sources_registered       gauge  registered sources
 *   japanosint_sources_scheduled        gauge  rows with an interval, not pods
 *   japanosint_sources_pivots           gauge  entity pivots
 *   japanosint_sources_pods             gauge  maintenance pods (collector _*)
 *   japanosint_collector_runs_1h        gauge  fetch_log rows in the last hour
 *   japanosint_collector_failures_1h    gauge  of those, status='error'
 *
 * The two run figures read fetch_log through idx_log_ts (an indexed timestamp
 * predicate); each is a COUNT over one hour of rows, not over the table.
 * Gated by the caller (platform operator). */
#ifndef JO_METRICSAPI_H
#define JO_METRICSAPI_H
#include "db.h"

/* malloc'd exposition text (text/plain; version=0.0.4), caller frees; NULL on
 * allocation failure. A failed fetch_log read omits those two gauges. */
char *metricsapi_render(db_handle *db);

#endif
