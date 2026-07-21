/* core/auth.h — port of middleware/auth.js requireSupabaseAuth gate.
 *
 * Parity contract (verified against the running Node server):
 *   - neither SUPABASE_URL nor SUPABASE_JWT_SECRET set -> 503 {"error":"Auth not configured"}
 *   - no/!Bearer header                                -> 401 {"error":"Missing bearer token"}
 *   - bad token                                        -> 401 {"error":"Invalid token"}
 *   - ok                                               -> ALLOW, fills auth_user
 *
 * HS256 (SUPABASE_JWT_SECRET) verified here via OpenSSL HMAC + audience.
 * RS256/ES256 via the project JWKS is the asymmetric path Supabase uses by
 * default; JWKS fetch+verify is wired in P7 (a real user token is needed to
 * exercise it). Until then the configured-but-asymmetric case falls through
 * to the HS256 secret like Node's step 2. */
#ifndef JO_AUTH_H
#define JO_AUTH_H

typedef enum {
  AUTH_ALLOW = 0,
  AUTH_401_MISSING,    /* {"error":"Missing bearer token"}    */
  AUTH_401_INVALID,    /* {"error":"Invalid token"}           */
  AUTH_401_NO_SUB,     /* {"error":"Token missing sub"}       */
  AUTH_503_UNCONFIG    /* {"error":"Auth not configured"}     */
} auth_result;

typedef struct {
  char id[128];
  char email[256];
  char role[64];
} auth_user;

void        auth_init(void);                 /* read env once */
auth_result auth_check(const char *authorization_header, auth_user *out);
/* Map result -> (status, json body) for uniform replies. */
int         auth_status(auth_result r);
const char *auth_body(auth_result r);

#endif
