/* core/tenantapi.h — P7 Wave 3a: tenant resolution (middleware/tenant.js) +
 * /api/me + /api/audit (auditChain.js verify). Pure SQLite over the shared
 * japanmap.db. Single C backend (faithful behaviour, not Node parity). */
#ifndef JO_TENANTAPI_H
#define JO_TENANTAPI_H
#include "db.h"
#include "auth.h"

typedef struct {
  char user_id[64];                 /* local users.id (uuid) */
  char email[256];
  char display_name[256]; int has_display;
  char tenant_id[64], slug[160], tname[320], plan[32];
  int  require_sso;
  char role[32];
} tenant_ctx;

/* resolveTenant: first-seen provisioning (users + personal tenant + owner
 * membership), X-Tenant-Id pick. Returns 0 on success (out filled), or a
 * negative auth/500 marker: -401 (no user), -500 (db failure), or
 * TENANT_NOT_MEMBER when a NON-EMPTY x_tenant_id names a workspace the caller
 * is not a member of (2026-11-07: it used to fall back silently to the first
 * workspace). NULL or "" picks the caller's first workspace. */
#define TENANT_NOT_MEMBER (-403)
int tenant_resolve(db_handle *db, const auth_user *u,
                   const char *x_tenant_id, tenant_ctx *out);

/* The same, except a non-member x_tenant_id falls back to the first workspace
 * instead of failing. ONLY for GET /api/me, which reports the workspace it
 * actually used, so a client holding a stale id can recover. */
int tenant_resolve_lenient(db_handle *db, const auth_user *u,
                           const char *x_tenant_id, tenant_ctx *out);

/* GET /api/me — identity + active tenant + memberships. malloc'd. */
char *tenantapi_me(db_handle *db, const tenant_ctx *t);

/* GET /api/audit?limit&offset — recent events for the active tenant. */
char *tenantapi_audit_list(db_handle *db, const char *tenant_id,
                           int limit, int offset);

/* GET /api/audit/verify — re-walk the tenant's hash chain. malloc'd. */
char *tenantapi_audit_verify(db_handle *db, const char *tenant_id);

/* Which chain GET /api/audit[/verify] reads, from its `?scope=` parameter
 * (2026-11-12). Operator actions — break-glass logins, cadence and schedule
 * changes, threshold edits, collector runs, raw evidence reads — are written
 * under the reserved tenant "platform" (TENANTAPI_PLATFORM_CHAIN), which no
 * workspace route could read, so the one chain recording what operators did
 * had no reader at all. `scope=platform` reads it, for a PLATFORM OPERATOR
 * only (opgate_check() == 0, the gate /api/admin/thresholds sits behind);
 * workspace owner/admin is not enough, because that chain is every
 * workspace's business and none of theirs alone.
 *
 *   scope NULL / "" / "workspace"  -> *chain = tenant_id, returns 0
 *   scope "platform", operator     -> *chain = "platform", returns 0
 *   scope "platform", not operator -> 403
 *   anything else                  -> 400 (an unknown scope is not silently
 *                                     the workspace: a typo would read as the
 *                                     wrong chain) */
#define TENANTAPI_PLATFORM_CHAIN "platform"
int tenantapi_audit_chain(const char *scope, int is_operator,
                          const char *tenant_id, const char **chain);

/* /api/members[...] — member roster + pending-invite management. seg/act are
 * the two path segments after /api/members/ (parsed in httpd.c): GET ""
 * lists; POST "invite"; PATCH ":userId"/"role"; DELETE ":userId"; DELETE
 * "invite"/":id". Writes require owner/admin role. malloc'd; sets *status. */
char *tenantapi_members(db_handle *db, const tenant_ctx *t,
                        const char *method, const char *seg,
                        const char *act, const char *body, int *status);

#endif
