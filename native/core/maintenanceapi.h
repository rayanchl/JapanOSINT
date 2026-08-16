/* core/maintenanceapi.h — GET /api/admin/maintenance?hours= digest
 * (self-healing/repair report; split from w4api). */
#ifndef JO_MAINTENANCEAPI_H
#define JO_MAINTENANCEAPI_H
#include "db.h"
char *maintenance_digest(db_handle *db, int hours);

/* GET /api/admin/maintenance/source/:id — full per-source pipeline (source
 * meta + quarantine, recent fetch_log runs, anomalies with triage, repairs
 * with patch). Returns malloc'd JSON, or NULL if the source id is unknown
 * (caller replies 404). */
char *maintenance_source_detail(db_handle *db, const char *source_id);

/* Operator actions. Each returns malloc'd JSON and sets *status to the HTTP
 * code (200 ok, 404 not-found, 409 wrong-state, 422 bad-data). Never returns
 * NULL on a handled path. */
char *maintenance_repair_action(db_handle *db, long repair_id, int approve,
                                int *status);

/* DELETE /api/admin/repairs/:id — undo an approved url_swap: drop the runtime
 * override (url_override_remove + url_override_reload, so the in-memory map
 * and collector_url_overrides agree again) and put the repair row back to
 * 'verified' so it can be re-approved. 409 "not_revertable" when the repair is
 * not a merged url_swap — there is nothing live to undo, and reporting success
 * would claim a rewrite was removed that never existed. Without this a
 * misapplied repair was irreversible short of editing the table and
 * restarting the process. */
char *maintenance_repair_revert(db_handle *db, long repair_id, int *status);
char *maintenance_unquarantine(db_handle *db, const char *source_id,
                               int *status);
char *maintenance_requeue_anomaly(db_handle *db, long anomaly_id, int *status);
#endif
