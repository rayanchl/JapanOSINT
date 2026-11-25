/* core/authcookie.c — see authcookie.h. */
#include "authcookie.h"
#include "cors.h"
#include "httpclient.h"
#include "ratelimit.h"
#include "../lib/joenv.h"
#include "../third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

ac_kind ac_kind_of(const char *p, size_t n) {
#define IS(s) (n == sizeof(s) - 1 && !memcmp(p, s, n))
  if (IS("/api/auth/login"))   return AC_LOGIN;
  if (IS("/api/auth/signup"))  return AC_SIGNUP;
  if (IS("/api/auth/oauth"))   return AC_OAUTH;
  if (IS("/api/auth/refresh")) return AC_REFRESH;
  if (IS("/api/auth/logout"))  return AC_LOGOUT;
#undef IS
  return AC_NONE;
}

/* "scheme://authority" -> authority, or NULL when it is not http(s). */
static const char *authority_of(const char *origin) {
  if (!strncasecmp(origin, "https://", 8)) return origin + 8;
  if (!strncasecmp(origin, "http://", 7))  return origin + 7;
  return NULL;
}

int ac_guard(const ac_req *r, const char **why) {
  const char *w = NULL;
  const int has_origin = r->origin && *r->origin;
  int listed = has_origin && cors_verdict(r->origin, strlen(r->origin)) >= 2;
  if (!r->csrf || strcmp(r->csrf, "1") != 0) {
    w = "csrf_header_missing";
  } else if (has_origin) {
    const char *auth = authority_of(r->origin);
    int same = auth && r->host && *r->host && !strcasecmp(auth, r->host);
    if (!same && !listed) w = "origin_not_allowed";
  }
  if (!w && r->sec_fetch_site && *r->sec_fetch_site) {
    const char *s = r->sec_fetch_site;
    int own = !strcasecmp(s, "same-origin") || !strcasecmp(s, "none");
    /* same-site / cross-site: only an allow-listed Origin may cross. */
    if (!own && !listed) w = "cross_site";
  }
  if (why) *why = w;
  return w != NULL;
}

int ac_cookie_get(const char *h, char *out, size_t cap) {
  if (!h || !cap) return 0;
  const size_t nl = sizeof(AC_COOKIE_NAME) - 1;
  const char *p = h;
  while (*p) {
    while (*p == ' ' || *p == ';' || *p == '\t') p++;
    const char *e = p;
    while (*e && *e != ';') e++;
    if ((size_t)(e - p) > nl && !strncmp(p, AC_COOKIE_NAME, nl) && p[nl] == '=') {
      size_t vl = (size_t)(e - (p + nl + 1));
      while (vl && (p[nl + vl] == ' ' || p[nl + vl] == '\t')) vl--;
      if (vl == 0 || vl >= cap) return 0;
      memcpy(out, p + nl + 1, vl); out[vl] = 0;
      return 1;
    }
    p = e;
  }
  return 0;
}

int ac_secure(const char *peer, const char *xfp, int conn_is_tls) {
  if (conn_is_tls) return 1;
  if (!peer || !xfp || !*xfp || !ratelimit_peer_trusted(peer)) return 0;
  const char *last = strrchr(xfp, ',');
  last = last ? last + 1 : xfp;
  while (*last == ' ' || *last == '\t') last++;
  return !strncasecmp(last, "https", 5) &&
         (last[5] == 0 || last[5] == ' ' || last[5] == '\t');
}

long ac_keep_max_age_sec(void) {
  return jo_env_long("JO_REFRESH_COOKIE_MAX_AGE_SEC", 30L * 24 * 3600, 60, 400L * 24 * 3600);
}

static int token_ok(const char *t) {
  size_t n = t ? strlen(t) : 0;
  if (!n || n > AC_TOKEN_MAX) return 0;
  for (; *t; t++)
    if (!(isalnum((unsigned char)*t) || strchr("-._~+/=", *t))) return 0;
  return 1;
}

int ac_set_cookie(char *out, size_t cap, const char *token, int keep, int secure) {
  if (cap) out[0] = 0;
  if (!token_ok(token)) return -1;
  char age[48] = "";
  if (keep) snprintf(age, sizeof age, "; Max-Age=%ld", ac_keep_max_age_sec());
  int n = snprintf(out, cap, "%s=%s; Path=%s; HttpOnly; SameSite=Strict%s%s",
                   AC_COOKIE_NAME, token, AC_REFRESH_PATH, secure ? "; Secure" : "", age);
  if (n < 0 || (size_t)n >= cap) { if (cap) out[0] = 0; return -1; }
  return 0;
}

void ac_clear_cookie(char *out, size_t cap, int secure) {
  snprintf(out, cap, "%s=; Path=%s; HttpOnly; SameSite=Strict%s; Max-Age=0; "
           "Expires=Thu, 01 Jan 1970 00:00:00 GMT",
           AC_COOKIE_NAME, AC_REFRESH_PATH, secure ? "; Secure" : "");
}

/* ---- the exchange ------------------------------------------------------- */

static void fail(ac_result *o, int status, const char *code, const char *msg) {
  cJSON *j = cJSON_CreateObject();
  cJSON_AddStringToObject(j, "error", code);
  if (msg && *msg) cJSON_AddStringToObject(j, "message", msg);
  o->status = status;
  free(o->body);
  o->body = cJSON_PrintUnformatted(j);
  cJSON_Delete(j);
}

int ac_wants_cookie(const char *body_json) {
  cJSON *in = (body_json && *body_json) ? cJSON_Parse(body_json) : NULL;
  int w = cJSON_IsTrue(in ? cJSON_GetObjectItemCaseSensitive(in, "cookie") : NULL);
  cJSON_Delete(in);
  return w;
}

static const char *sget(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItemCaseSensitive(o, k) : NULL;
  return cJSON_IsString(v) ? v->valuestring : NULL;
}

/* One POST to GoTrue. 0 = an HTTP exchange completed (status in r). */
static int upstream_post(const char *path, const char *anon, const char *bearer,
                         const char *body, http_response *r) {
  const char *base = getenv("SUPABASE_URL");
  char url[512];
  size_t bl = strlen(base);
  while (bl && base[bl - 1] == '/') bl--;
  snprintf(url, sizeof url, "%.*s/auth/v1/%s", (int)bl, base, path);
  char h_key[400], h_auth[AC_TOKEN_MAX * 4 + 32];
  snprintf(h_key, sizeof h_key, "apikey: %s", anon);
  const char *hdrs[4]; int nh = 0;
  hdrs[nh++] = h_key;
  hdrs[nh++] = "Content-Type: application/json";
  if (bearer && *bearer) {
    snprintf(h_auth, sizeof h_auth, "Authorization: Bearer %s", bearer);
    hdrs[nh++] = h_auth;
  }
  hdrs[nh] = NULL;
  http_client *hc = http_client_new();
  if (!hc) return 1;
  int rc = http_request(hc, "POST", url, hdrs, body ? body : "", body ? strlen(body) : 0,
                        15000, 0, r);
  http_client_free(hc);
  return rc;
}

static void run_logout(const char *bearer, const char *anon, int secure, ac_result *out) {
  int ok = 0;
  if (bearer && *bearer) {
    http_response r = {0};
    int rc = upstream_post("logout?scope=local", anon, bearer, NULL, &r);
    ok = rc == 0 && ((r.status >= 200 && r.status < 300) ||
                     r.status == 401 || r.status == 403 || r.status == 404);
    http_response_free(&r);
  }
  out->status = 200;
  out->body = strdup(ok ? "{\"ok\":true,\"revoked\":true}" : "{\"ok\":true,\"revoked\":false}");
  ac_clear_cookie(out->set_cookie, sizeof out->set_cookie, secure);   /* always */
}

void ac_run(ac_kind k, const char *body_json, const char *cookie_token,
            const char *bearer, int secure, ac_result *out) {
  memset(out, 0, sizeof *out);
  const char *base = getenv("SUPABASE_URL"), *anon = getenv("SUPABASE_ANON_KEY");
  if (!base || !*base || !anon || !*anon) {
    fail(out, 501, "auth_exchange_not_configured",
         "SUPABASE_URL and SUPABASE_ANON_KEY must both be set on the server");
    return;
  }
  if (k == AC_LOGOUT) { run_logout(bearer, anon, secure, out); return; }

  cJSON *in = (body_json && *body_json) ? cJSON_Parse(body_json) : NULL;
  if (body_json && *body_json && !in) { fail(out, 400, "bad_json", NULL); return; }
  cJSON *kp = in ? cJSON_GetObjectItemCaseSensitive(in, "keep") : NULL;
  const int keep = cJSON_IsTrue(kp);

  const char *path = NULL;
  cJSON *up = cJSON_CreateObject();
  int via_cookie = 1;            /* tokens go to a cookie, not the JSON body */
  cJSON *uj = NULL, *oj = NULL;
  char *req = NULL;
  http_response r = {0};

  switch (k) {
    case AC_LOGIN: case AC_SIGNUP: {
      const char *e = sget(in, "email"), *p = sget(in, "password");
      if (!e || !p || !*e || !*p) { fail(out, 400, "email_and_password_required", NULL); goto done; }
      path = k == AC_LOGIN ? "token?grant_type=password" : "signup";
      cJSON_AddStringToObject(up, "email", e);
      cJSON_AddStringToObject(up, "password", p);
      break;
    }
    case AC_OAUTH: {
      const char *c = sget(in, "auth_code"), *v = sget(in, "code_verifier");
      if (!c || !v || !*c || !*v) { fail(out, 400, "auth_code_and_verifier_required", NULL); goto done; }
      path = "token?grant_type=pkce";
      cJSON_AddStringToObject(up, "auth_code", c);
      cJSON_AddStringToObject(up, "code_verifier", v);
      break;
    }
    case AC_REFRESH: {
      const char *rt = cookie_token;
      if (!rt || !*rt) {
        rt = sget(in, "refresh_token");
        /* {"cookie":true}: the web client moving a token an older build kept in
         * storage into the cookie. Otherwise a native body refresh. */
        via_cookie = ac_wants_cookie(body_json);
      }
      if (!rt || !*rt) { fail(out, 401, "no_refresh_token", NULL); goto done; }
      path = "token?grant_type=refresh_token";
      cJSON_AddStringToObject(up, "refresh_token", rt);
      break;
    }
    default: fail(out, 404, "not_found", NULL); goto done;
  }

  req = cJSON_PrintUnformatted(up);
  int rc = upstream_post(path, anon, NULL, req, &r);
  if (rc != 0 || r.status == 0) { fail(out, 502, "identity_provider_unreachable", NULL); goto done; }
  uj = r.body ? cJSON_Parse(r.body) : NULL;

  if (r.status < 200 || r.status >= 300) {
    const char *msg = sget(uj, "error_description");
    if (!msg) msg = sget(uj, "msg");
    if (!msg) msg = sget(uj, "message");
    const char *code = sget(uj, "error_code");
    if (!code) code = sget(uj, "error");
    int st = r.status == 400 || r.status == 401 || r.status == 422 || r.status == 429
             ? (int)r.status : (r.status == 403 ? 401 : 502);
    fail(out, st, code ? code : "upstream_error", msg);
    /* A refused cookie token is dead: stop the browser sending it. */
    if (k == AC_REFRESH && via_cookie && (r.status == 400 || r.status == 401 || r.status == 403))
      ac_clear_cookie(out->set_cookie, sizeof out->set_cookie, secure);
    goto done;
  }
  const char *at = sget(uj, "access_token"), *nrt = sget(uj, "refresh_token");
  if (!at || !*at) {
    if (k == AC_SIGNUP) {                      /* email confirmation required */
      out->status = 200;
      out->body = strdup("{\"confirmation_required\":true}");
    } else fail(out, 502, "unreadable_upstream_response", NULL);
    goto done;
  }
  oj = cJSON_CreateObject();
  cJSON_AddStringToObject(oj, "access_token", at);
  cJSON_AddStringToObject(oj, "token_type", "bearer");
  cJSON *ei = cJSON_GetObjectItemCaseSensitive(uj, "expires_in");
  if (cJSON_IsNumber(ei)) cJSON_AddNumberToObject(oj, "expires_in", ei->valuedouble);
  cJSON *us = cJSON_GetObjectItemCaseSensitive(uj, "user");
  if (cJSON_IsObject(us)) {
    cJSON *uo = cJSON_AddObjectToObject(oj, "user");
    const char *id = sget(us, "id"), *em = sget(us, "email");
    cJSON_AddStringToObject(uo, "id", id ? id : "");
    if (em) cJSON_AddStringToObject(uo, "email", em); else cJSON_AddNullToObject(uo, "email");
  }
  if (!via_cookie) {
    /* Native body path: no cookie was presented, so the rotated token goes
     * back in the body, as GoTrue itself would have answered. */
    if (nrt) cJSON_AddStringToObject(oj, "refresh_token", nrt);
  } else if (nrt && ac_set_cookie(out->set_cookie, sizeof out->set_cookie, nrt, keep, secure) != 0) {
    fail(out, 502, "refresh_token_not_cookie_safe", NULL);
    goto done;
  }
  out->status = 200;
  out->body = cJSON_PrintUnformatted(oj);
done:
  http_response_free(&r);
  free(req);
  cJSON_Delete(oj);
  cJSON_Delete(uj);
  cJSON_Delete(up);
  cJSON_Delete(in);
  if (!out->body) { out->status = 500; out->body = strdup("{\"error\":\"server_error\"}"); }
}
