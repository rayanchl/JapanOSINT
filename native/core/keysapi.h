/* core/keysapi.h — P7 Wave 3c: /api/keys (platform overlay, operator-admin),
 * /api/tenant-keys (per-tenant BYOK, HKDF+AES-256-GCM tenant_secrets), and
 * /admin/break-glass/login (TOTP → HS256 JWT). Ports apiKeysStore.js +
 * credentials.js crypto + keyAccess.js gates + breakGlass.js. */
#ifndef JO_KEYSAPI_H
#define JO_KEYSAPI_H
#include "db.h"
#include "tenantapi.h"

/* /api/keys subtree. name="" for the collection. Sets *status; malloc'd. */
char *keysapi_platform(db_handle *db, const tenant_ctx *t, const char *method,
                       const char *name, const char *body, int *status);

/* /api/tenant-keys subtree. seg = "" | "policy" | "<VARNAME>". */
char *keysapi_tenant(db_handle *db, const tenant_ctx *t, const char *method,
                     const char *seg, const char *body, int *status);

/* POST /admin/break-glass/login (pre-auth, mounted outside /api). */
char *keysapi_breakglass(db_handle *db, const char *body,
                         const char *ip, const char *ua, int *status);

#endif
