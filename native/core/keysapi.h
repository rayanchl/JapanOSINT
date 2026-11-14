/* core/keysapi.h — P7 Wave 3c: /api/keys (platform overlay, operator-admin),
 * /api/tenant-keys (per-tenant BYOK, HKDF+AES-256-GCM tenant_secrets), and
 * /admin/break-glass/login (TOTP → HS256 JWT). Ports apiKeysStore.js +
 * credentials.js crypto + keyAccess.js gates + breakGlass.js. */
#ifndef JO_KEYSAPI_H
#define JO_KEYSAPI_H
#include "db.h"
#include "tenantapi.h"
#include "../source.h"     /* intel_sink, intel_item */

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

/* ── a WORKSPACE's own key for its on-demand pivots (2026-11-12) ─────────
 * The repository owner's decision ("use them for pivots"): when a workspace
 * member's search dispatches an entity pivot, the row's key_env resolves from
 * THAT workspace's saved key (tenant_secrets, decrypted here) before the
 * process environment. Scheduled shared-corpus runs never call this and keep
 * the environment only. core/osint_dispatch.c is the one caller.
 *
 * The decrypted plaintext of `name` for `tenant_id`, malloc'd, or NULL when
 * the workspace stores none (or it cannot be decrypted, or it is shorter than
 * KEYSAPI_PIVOT_KEY_MIN — see keysapi.c). Stamps last_used_at. The caller
 * OPENSSL_cleanse()s and frees it when the run ends. */
#define KEYSAPI_PIVOT_KEY_MIN 8
char *keysapi_tenant_secret(db_handle *db, const char *tenant_id,
                            const char *name);

/* An intel_sink that replaces every occurrence of `secret` with "[redacted]"
 * in each string field of an item before handing it to `inner`. core/intel.c
 * redacts the VALUES of credential-named ENVIRONMENT variables; a workspace
 * key is not in the environment, so without this a fetch-failure URL carrying
 * `api_key=<the workspace's key>` would be stored in the shared corpus and
 * returned in the run's response. `secret` must outlive the sink; NULL or ""
 * makes it a pass-through. */
typedef struct keysapi_scrub_sink {
  intel_sink   base;            /* pass &w->base to the collector */
  intel_sink  *inner;
  const char  *secret;
  size_t       secret_len;
} keysapi_scrub_sink;
void keysapi_scrub_sink_init(keysapi_scrub_sink *w, intel_sink *inner,
                             const char *secret);

/* POST /admin/break-glass/login (pre-auth, mounted outside /api). */
char *keysapi_breakglass(db_handle *db, const char *body,
                         const char *ip, const char *ua, int *status);

#endif
