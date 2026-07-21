/* core/alertsapi.h — P7 Wave 3b: alert-rule CRUD (routes/alerts.js),
 * tenant-scoped over alert_rules / alert_events. Single C backend.
 *
 * alertEngine dispatch (email/webhook) is a side-effecting subsystem shared
 * with the collector pipeline — its port lands with P5/P6. /:id/test is
 * therefore contract-correct (404 vs {ok,fired}) but does not dispatch yet. */
#ifndef JO_ALERTSAPI_H
#define JO_ALERTSAPI_H
#include "db.h"

/* One entrypoint for the whole /api/alerts subtree. `id`/`action` are the
 * parsed path params ("" when absent), `body` the raw request body (may be
 * NULL), `method` the HTTP verb. Sets *status; returns a malloc'd JSON body
 * or NULL (NULL + status 204 = empty success; NULL + other = caller emits a
 * generic error of that status). */
char *alertsapi(db_handle *db, const char *tenant_id, const char *user_id,
                const char *method, const char *id, const char *action,
                const char *body, int ev_limit, int *status);

#endif
