/* core/alertsapi.h — P7 Wave 3b: alert-rule CRUD (routes/alerts.js),
 * tenant-scoped over alert_rules / alert_events. Single C backend.
 *
 * Dispatch is no longer a stub: matching is core/alert_eval.c (hooked into the
 * intel ingest chokepoint) and delivery is core/alert_deliver.c (background
 * worker, webhook + SMTP). /:id/test now really fires through the rule's
 * channels via alert_deliver_test(). */
#ifndef JO_ALERTSAPI_H
#define JO_ALERTSAPI_H
#include "db.h"

/* One entrypoint for the whole /api/alerts subtree. `id`/`action` are the
 * parsed path params ("" when absent), `body` the raw request body (may be
 * NULL), `method` the HTTP verb. Sets *status; returns a malloc'd JSON body
 * or NULL (NULL + status 204 = empty success; NULL + other = caller emits a
 * generic error of that status).
 *
 * AUTHORIZATION is enforced INSIDE, not by the caller: (tenant_id,user_id) is
 * looked up in `memberships` and a read needs any role, while create / edit /
 * delete / mute / unmute / test need analyst or better — the savedsearchapi.c
 * rule. A non-member gets 403. The role is derived rather than passed because
 * savedsearchapi.c's /to-alert re-enters this same entrypoint and the
 * signature is a published contract.
 *
 * Webhook channel targets are validated with hostgate_url_check_strict(), so a
 * rule cannot aim this server's outbound connection at loopback, RFC1918 or
 * the 169.254.169.254 metadata address. Rule create/update/delete/test write
 * audit_events rows. */
char *alertsapi(db_handle *db, const char *tenant_id, const char *user_id,
                const char *method, const char *id, const char *action,
                const char *body, int ev_limit, int *status);

/* /api/alert-events[...] — the cross-rule notification inbox (roadmap item
 * 11). /api/alerts/:id/events answers "what did THIS rule match"; the inbox
 * answers "what is waiting for me", tenant-wide and stateful.
 *
 *   GET  /api/alert-events?unread=1&limit=N   list (suppressed rows excluded)
 *   GET  /api/alert-events/unread-count       badge count
 *   POST /api/alert-events/:id/read           mark one read
 *   POST /api/alert-events/read-all           mark all read
 *
 * `seg` is the single path segment after /api/alert-events/ ("" for the
 * collection; "unread-count" / "read-all" are reserved words, anything else
 * is treated as an event id). `qs` is the raw query string (may be NULL).
 * Read state lives in alert_events.read_at, added by db.c's ensure_column()
 * boot migration — it is NOT in the generated schema.sql, by design. */
char *alerteventsapi(db_handle *db, const char *tenant_id, const char *user_id,
                     const char *method, const char *seg,
                     const char *qs, int *status);

#endif
