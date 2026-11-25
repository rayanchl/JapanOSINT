/* test_authcookie.c — WEB-13: the refresh token as an httpOnly cookie.
 *
 * Pins core/authcookie.c:
 *   - the CSRF defence: accepted shapes (same origin, allow-listed origin,
 *     absent Origin with the header) and EACH rejection (header missing or
 *     wrong, foreign origin, "null" origin, look-alike host, cross-site fetch
 *     metadata);
 *   - the cookie: HttpOnly; SameSite=Strict; Path = the refresh route only;
 *     Secure only behind TLS or a trusted proxy that says https; Max-Age only
 *     for "keep me signed in"; refused for a token a cookie cannot carry; the
 *     clearing cookie; the reader picks jo_rt out of a jar;
 *   - the route names;
 *   - an unconfigured server answers 501 and a malformed body 400, without
 *     touching the network. */
#include "../../core/authcookie.c"

#include <assert.h>

static int guard(const char *origin, const char *host, const char *sfs,
                 const char *csrf, const char **why) {
  ac_req r = { origin, host, sfs, csrf };
  return ac_guard(&r, why);
}

int main(void) {
  setenv("JO_CORS_ORIGINS", "https://app.example.com", 1);
  cors_init();
  const char *why = NULL;

  /* ---- accepted ------------------------------------------------------- */
  assert(!guard("https://api.example.com", "api.example.com", "same-origin", "1", &why));
  assert(!guard("http://localhost:5173", "localhost:5173", NULL, "1", &why));
  assert(!guard("https://app.example.com", "api.example.com", "cross-site", "1", &why) &&
         "an allow-listed origin may cross");
  assert(!guard(NULL, "api.example.com", NULL, "1", &why) && "no Origin: a non-browser caller, header present");
  assert(!guard(NULL, "api.example.com", "none", "1", &why));
  printf("  accepted: same origin, allow-listed origin, absent origin + header: ok\n");

  /* ---- each rejection ------------------------------------------------- */
  assert(guard("https://api.example.com", "api.example.com", "same-origin", NULL, &why) &&
         !strcmp(why, "csrf_header_missing"));
  assert(guard("https://api.example.com", "api.example.com", NULL, "0", &why) &&
         !strcmp(why, "csrf_header_missing"));
  assert(guard("https://api.example.com", "api.example.com", NULL, "", &why) &&
         !strcmp(why, "csrf_header_missing"));
  assert(guard(NULL, "api.example.com", NULL, NULL, &why) && !strcmp(why, "csrf_header_missing"));
  assert(guard("https://evil.example", "api.example.com", "cross-site", "1", &why) &&
         !strcmp(why, "origin_not_allowed"));
  assert(guard("https://api.example.com.evil.example", "api.example.com", NULL, "1", &why) &&
         !strcmp(why, "origin_not_allowed") && "a look-alike host is not this host");
  assert(guard("null", "api.example.com", NULL, "1", &why) && !strcmp(why, "origin_not_allowed"));
  assert(guard("ftp://api.example.com", "api.example.com", NULL, "1", &why));
  assert(guard(NULL, "api.example.com", "cross-site", "1", &why) && !strcmp(why, "cross_site") &&
         "a browser naming a cross-site fetch with no Origin");
  assert(guard("https://api.example.com", "api.example.com", "same-site", "1", &why) &&
         !strcmp(why, "cross_site") && "a sibling site is not this origin");
  assert(guard("https://api.example.com", "api.example.com", "teleported", "1", &why) &&
         "an unknown Sec-Fetch-Site fails closed");
  printf("  rejected: header missing/wrong, foreign/null/look-alike origin, cross-site: ok\n");

  unsetenv("JO_CORS_ORIGINS");
  cors_init();
  assert(guard("https://app.example.com", "api.example.com", NULL, "1", &why) &&
         !strcmp(why, "origin_not_allowed") && "no allow-list: only same origin");
  printf("  CORS off: only the server's own origin passes: ok\n");

  /* ---- cookie ----------------------------------------------------------- */
  char ck[AC_COOKIE_MAX];
  assert(ac_set_cookie(ck, sizeof ck, "abc123XYZ", 0, 0) == 0);
  assert(!strcmp(ck, "jo_rt=abc123XYZ; Path=/api/auth/refresh; HttpOnly; SameSite=Strict"));
  assert(!strstr(ck, "Max-Age") && !strstr(ck, "Secure"));
  assert(ac_set_cookie(ck, sizeof ck, "abc123XYZ", 1, 1) == 0);
  assert(strstr(ck, "HttpOnly") && strstr(ck, "SameSite=Strict") && strstr(ck, "; Secure") &&
         strstr(ck, "Path=/api/auth/refresh;") && strstr(ck, "; Max-Age=2592000"));
  setenv("JO_REFRESH_COOKIE_MAX_AGE_SEC", "3600", 1);
  assert(ac_set_cookie(ck, sizeof ck, "t", 1, 0) == 0 && strstr(ck, "Max-Age=3600"));
  unsetenv("JO_REFRESH_COOKIE_MAX_AGE_SEC");
  assert(ac_set_cookie(ck, sizeof ck, "", 0, 0) == -1 && !ck[0]);
  assert(ac_set_cookie(ck, sizeof ck, "a;b", 0, 0) == -1 && "a ';' would end the cookie");
  assert(ac_set_cookie(ck, sizeof ck, "a b", 0, 0) == -1);
  assert(ac_set_cookie(ck, sizeof ck, "a\r\nSet-Cookie: x=1", 0, 0) == -1 && "no header injection");
  { char big[AC_TOKEN_MAX + 8]; memset(big, 'a', sizeof big - 1); big[sizeof big - 1] = 0;
    assert(ac_set_cookie(ck, sizeof ck, big, 0, 0) == -1); }
  ac_clear_cookie(ck, sizeof ck, 1);
  assert(strstr(ck, "jo_rt=;") && strstr(ck, "Max-Age=0") && strstr(ck, "Path=/api/auth/refresh;") &&
         strstr(ck, "Secure") && strstr(ck, "HttpOnly"));
  printf("  cookie: HttpOnly, Strict, refresh path only, Secure/Max-Age only when due: ok\n");

  char tok[AC_TOKEN_MAX + 1];
  assert(ac_cookie_get("a=1; jo_rt=tok-9; b=2", tok, sizeof tok) && !strcmp(tok, "tok-9"));
  assert(ac_cookie_get("jo_rt=first", tok, sizeof tok) && !strcmp(tok, "first"));
  assert(!ac_cookie_get("xjo_rt=nope; jo_rt2=nope", tok, sizeof tok));
  assert(!ac_cookie_get("jo_rt=", tok, sizeof tok));
  assert(!ac_cookie_get(NULL, tok, sizeof tok) && !ac_cookie_get("", tok, sizeof tok));
  printf("  cookie jar: jo_rt found exactly, look-alike names ignored: ok\n");

  /* ---- Secure -------------------------------------------------------- */
  unsetenv("JO_TRUSTED_PROXIES");
  assert(!ac_secure("10.0.0.5", "https", 0) && "an untrusted peer cannot claim TLS");
  assert(ac_secure("10.0.0.5", NULL, 1) && "a TLS connection is Secure");
  setenv("JO_TRUSTED_PROXIES", "10.0.0.5", 1);
  assert(ac_secure("10.0.0.5", "https", 0));
  assert(ac_secure("10.0.0.5", "http, https", 0) && "the proxy's own hop is right-most");
  assert(!ac_secure("10.0.0.5", "https, http", 0));
  assert(!ac_secure("10.0.0.5", "http", 0) && !ac_secure("10.0.0.5", NULL, 0));
  assert(!ac_secure("10.0.0.6", "https", 0) && "a different peer is not the proxy");
  unsetenv("JO_TRUSTED_PROXIES");
  printf("  Secure: TLS or a trusted proxy saying https, never a client's claim: ok\n");

  /* ---- route names ------------------------------------------------------ */
  assert(ac_kind_of("/api/auth/login", 15) == AC_LOGIN);
  assert(ac_kind_of("/api/auth/refresh", 17) == AC_REFRESH);
  assert(ac_kind_of("/api/auth/logout", 16) == AC_LOGOUT);
  assert(ac_kind_of("/api/auth/oauth", 15) == AC_OAUTH);
  assert(ac_kind_of("/api/auth/signup", 16) == AC_SIGNUP);
  assert(ac_kind_of("/api/auth/refresh/x", 19) == AC_NONE && ac_kind_of("/api/auth", 9) == AC_NONE);

  assert(ac_wants_cookie("{\"refresh_token\":\"x\",\"cookie\":true}"));
  assert(!ac_wants_cookie("{\"refresh_token\":\"x\"}") && !ac_wants_cookie("{\"cookie\":\"yes\"}"));
  assert(!ac_wants_cookie(NULL) && !ac_wants_cookie("") && !ac_wants_cookie("{bad"));

  /* ---- exchange: no network when it cannot be served ------------------- */
  ac_result r;
  unsetenv("SUPABASE_URL"); unsetenv("SUPABASE_ANON_KEY");
  ac_run(AC_REFRESH, "{}", "tok", NULL, 0, &r);
  assert(r.status == 501 && strstr(r.body, "auth_exchange_not_configured") && !r.set_cookie[0]);
  free(r.body);
  setenv("SUPABASE_URL", "https://x.invalid", 1);
  ac_run(AC_REFRESH, "{}", "tok", NULL, 0, &r);
  assert(r.status == 501); free(r.body);
  setenv("SUPABASE_ANON_KEY", "k", 1);
  ac_run(AC_LOGIN, "{not json", NULL, NULL, 0, &r);
  assert(r.status == 400 && strstr(r.body, "bad_json")); free(r.body);
  ac_run(AC_LOGIN, "{\"email\":\"a@b.c\"}", NULL, NULL, 0, &r);
  assert(r.status == 400 && strstr(r.body, "email_and_password_required")); free(r.body);
  ac_run(AC_OAUTH, "{\"auth_code\":\"c\"}", NULL, NULL, 0, &r);
  assert(r.status == 400); free(r.body);
  ac_run(AC_REFRESH, "{}", NULL, NULL, 0, &r);
  assert(r.status == 401 && strstr(r.body, "no_refresh_token") && !r.set_cookie[0]); free(r.body);
  ac_run(AC_LOGOUT, "", NULL, NULL, 1, &r);
  assert(r.status == 200 && strstr(r.set_cookie, "Max-Age=0") && strstr(r.set_cookie, "Secure") &&
         strstr(r.body, "\"revoked\":false")); free(r.body);
  printf("  exchange: 501 unconfigured, 400 bad body, 401 nothing to refresh, logout clears: ok\n");

  printf("test_authcookie: all passed\n");
  return 0;
}
