/* core/authcookie.h — the web client's refresh token as an httpOnly cookie
 * (audit WEB-13).
 *
 * HOW REFRESH WORKS HERE. This server issues no tokens of its own: sessions
 * come from Supabase GoTrue (core/auth.c only VERIFIES the access JWT). The
 * web client used to talk to GoTrue directly and keep the refresh token in
 * sessionStorage/localStorage, readable by any script on the page. These
 * routes put a small token-exchange in front of GoTrue so the refresh token is
 * never visible to page script:
 *
 *   POST /api/auth/login    {email,password,keep}          password grant
 *   POST /api/auth/signup   {email,password,keep}          (a session only when
 *                                                           email confirmation is off)
 *   POST /api/auth/oauth    {auth_code,code_verifier,keep} PKCE grant
 *   POST /api/auth/refresh  {keep}  + cookie   | {refresh_token} with NO cookie
 *                           ({refresh_token,cookie:true}: one-time web migration)
 *   POST /api/auth/logout                                   revoke + clear cookie
 *
 * The refresh token travels only in `Set-Cookie: jo_rt=...; HttpOnly;
 * SameSite=Strict; Path=/api/auth/refresh` (+ Secure behind TLS, + Max-Age
 * only for "keep me signed in"). The JSON answer carries the access token and
 * never the refresh token. Native clients (iOS keeps its token in the
 * Keychain and talks to GoTrue itself) are unaffected: a refresh that presents
 * NO cookie and a body refresh_token is answered with the token in the JSON
 * body and no cookie, exactly what a non-browser client needs.
 *
 * A cookie is ambient, so every route here that can act on one (and login,
 * signup, oauth, logout, which set or clear it) is guarded (ac_guard):
 *   - the custom header X-JO-Refresh: 1 must be present (a cross-site form or
 *     <img> cannot send it, and a cross-site fetch() that adds it needs a CORS
 *     preflight this server answers only for allow-listed origins);
 *   - Origin, when present, must be this server's own origin or on the
 *     JO_CORS_ORIGINS allow-list ("null" never is); an absent Origin is a
 *     non-browser caller or an ancient browser, which the header still gates;
 *   - Sec-Fetch-Site, when present, must not be cross-site.
 * The no-cookie native refresh is not ambient and is not guarded.
 *
 * CONFIG. SUPABASE_URL (already used for JWKS) and SUPABASE_ANON_KEY (the
 * project's publishable key). Either unset: 501 auth_exchange_not_configured.
 * JO_REFRESH_COOKIE_MAX_AGE_SEC: lifetime when "keep" was asked (default 30 d). */
#ifndef JO_AUTHCOOKIE_H
#define JO_AUTHCOOKIE_H
#include <stddef.h>

#define AC_COOKIE_NAME   "jo_rt"
#define AC_REFRESH_PATH  "/api/auth/refresh"
#define AC_CSRF_HEADER   "X-JO-Refresh"
#define AC_COOKIE_MAX    640     /* a Set-Cookie value we are willing to emit */
#define AC_TOKEN_MAX     320

typedef enum { AC_LOGIN, AC_SIGNUP, AC_OAUTH, AC_REFRESH, AC_LOGOUT, AC_NONE } ac_kind;

/* The route a path names, AC_NONE when it is not one of ours. */
ac_kind ac_kind_of(const char *path, size_t len);

typedef struct {
  const char *origin;          /* Origin header, NULL = absent                */
  const char *host;            /* Host header                                 */
  const char *sec_fetch_site;  /* Sec-Fetch-Site or NULL                      */
  const char *csrf;            /* X-JO-Refresh value or NULL                  */
} ac_req;

/* 0 = pass. Else 1 and *why names the rule ("csrf_header_missing",
 * "origin_not_allowed", "cross_site"). Reads the CORS allow-list. */
int ac_guard(const ac_req *r, const char **why);

/* The jo_rt value in a Cookie header; 1 when found and non-empty. */
int ac_cookie_get(const char *cookie_hdr, char *out, size_t cap);

/* Secure flag: the connection is TLS, or the peer is a $JO_TRUSTED_PROXIES
 * address and its (right-most) X-Forwarded-Proto is https. */
int ac_secure(const char *peer, const char *xfp, int conn_is_tls);

/* "jo_rt=...; Path=...; HttpOnly; SameSite=Strict[; Secure][; Max-Age=N]".
 * Returns 0, or -1 when the token is empty/too long/has a byte a cookie
 * cannot carry (nothing is written then). */
int ac_set_cookie(char *out, size_t cap, const char *token, int keep, int secure);
/* The same cookie, expired. */
void ac_clear_cookie(char *out, size_t cap, int secure);
long ac_keep_max_age_sec(void);

/* 1 when a JSON body asks {"cookie":true}: a refresh whose token arrives in the
 * body but must still be answered with the cookie (web migration of a token an
 * older build stored). Such a request is ambient-credential-shaped and guarded. */
int ac_wants_cookie(const char *body_json);

typedef struct {
  int   status;
  char *body;                  /* malloc'd JSON */
  char  set_cookie[AC_COOKIE_MAX];   /* "" = send none */
} ac_result;

/* Perform one exchange against GoTrue. BLOCKS on the network: call it from a
 * worker thread, never the event loop. `cookie_token` is the jo_rt value or
 * NULL; `bearer` the caller's access token (logout revoke) or NULL. */
void ac_run(ac_kind k, const char *body_json, const char *cookie_token,
            const char *bearer, int secure, ac_result *out);
#endif
