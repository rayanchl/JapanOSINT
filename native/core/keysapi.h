/* core/keysapi.h — P7 Wave 3c: /api/keys (platform overlay, operator-admin),
 * /api/tenant-keys (per-tenant BYOK, HKDF+AES-256-GCM tenant_secrets), and
 * /admin/break-glass/login (TOTP → HS256 JWT). Ports apiKeysStore.js +
 * credentials.js crypto + keyAccess.js gates + breakGlass.js. */
#ifndef JO_KEYSAPI_H
#define JO_KEYSAPI_H
#include "db.h"
#include "tenantapi.h"

/* /api/keys subtree. name="" for the collection. Sets *status; malloc'd. */
/* Overlay data/api-keys.json into the process environment. MUST be called at
 * startup before load_dotenv(), and it is what makes a key entered through
 * PUT /api/keys visible to collectors at all — they resolve through plain
 * getenv(). Without it the API reports a key as configured while every
 * collector reports itself gated. See the definition for the precedence rules. */
void keysapi_apply_overlay_env(void);

char *keysapi_platform(db_handle *db, const tenant_ctx *t, const char *method,
                       const char *name, const char *body, int *status);

/* /api/tenant-keys subtree. seg = "" | "policy" | "<VARNAME>". */
char *keysapi_tenant(db_handle *db, const tenant_ctx *t, const char *method,
                     const char *seg, const char *body, int *status);

/* POST /admin/break-glass/login (pre-auth, mounted outside /api). */
char *keysapi_breakglass(db_handle *db, const char *body,
                         const char *ip, const char *ua, int *status);

#endif
