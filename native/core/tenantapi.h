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
 * negative auth/500 marker: -401 (no user), -403 (X-Tenant-Id names a
 * workspace the caller is not a member of), -500 (db failure). */
int tenant_resolve(db_handle *db, const auth_user *u,
                   const char *x_tenant_id, tenant_ctx *out);

/* Per-request memo for tenant_resolve(), this thread only. httpd.c calls
 * _begin at the start of each request; until the next _begin a successful
 * resolution of the same (user, X-Tenant-Id) is returned from memory instead
 * of being redone by every handler after the central gate already did it.
 * Never active on a worker thread or in a test that does not call it. */
void tenant_resolve_scope_begin(void);
void tenant_resolve_scope_end(void);

/* 1 when `tenant_id` is an auto-provisioned personal workspace that still has
 * a single member — the kind every first-time user is made `owner` of, and so
 * a role that proves nothing about platform-level resources. Fails closed:
 * an unknown tenant or a database error reads as personal. */
int tenant_is_personal(db_handle *db, const char *tenant_id);

/* GET /api/me — identity + active tenant + memberships. malloc'd. */
char *tenantapi_me(db_handle *db, const tenant_ctx *t);

/* GET /api/audit?limit&offset — recent events for the active tenant. */
char *tenantapi_audit_list(db_handle *db, const char *tenant_id,
                           int limit, int offset);

/* GET /api/audit/verify — re-walk the tenant's hash chain. malloc'd. */
char *tenantapi_audit_verify(db_handle *db, const char *tenant_id);

/* /api/members[...] — member roster + pending-invite management. seg/act are
 * the two path segments after /api/members/ (parsed in httpd.c): GET ""
 * lists; POST "invite"; PATCH ":userId"/"role"; DELETE ":userId"; DELETE
 * "invite"/":id". Writes require owner/admin role. malloc'd; sets *status. */
char *tenantapi_members(db_handle *db, const tenant_ctx *t,
                        const char *method, const char *seg,
                        const char *act, const char *body, int *status);

#endif
