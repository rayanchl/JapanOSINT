#include "auth.h"
#include "httpclient.h"
#include "../third_party/cJSON.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/param_build.h>
#include <openssl/core_names.h>
#include <openssl/bn.h>
#include <openssl/ecdsa.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include "ratelimit.h"

static void jwks_start(void);

static const char *g_secret = NULL;   /* SUPABASE_JWT_SECRET */
static const char *g_url    = NULL;   /* SUPABASE_URL (JWKS present) */
static const char *g_aud    = "authenticated"; /* Node AUDIENCE default */
static char        g_iss[512];        /* expected `iss`; empty = unchecked */

void auth_init(void) {
  const char *s = getenv("SUPABASE_JWT_SECRET");
  const char *u = getenv("SUPABASE_URL");
  g_secret = (s && *s) ? s : NULL;
  g_url    = (u && *u) ? u : NULL;
  const char *a = getenv("SUPABASE_AUD");
  if (a && *a) g_aud = a;

  /* Expected issuer. JO_JWT_ISS wins when set (self-hosted GoTrue, or a
   * migration where the issuer is not SUPABASE_URL-derived); otherwise it is
   * Supabase's "<project-url>/auth/v1". Deliberately NOT hard-failing when
   * neither is derivable: an existing deployment that only ever set
   * SUPABASE_JWT_SECRET would lose every session on upgrade, which is a worse
   * outage than the check is worth. Logged once so the gap is visible. */
  const char *iss = getenv("JO_JWT_ISS");
  g_iss[0] = 0;
  if (iss && *iss) {
    snprintf(g_iss, sizeof g_iss, "%s", iss);
  } else if (g_url) {
    size_t n = strlen(g_url);
    while (n && g_url[n - 1] == '/') n--;          /* tolerate a trailing / */
    snprintf(g_iss, sizeof g_iss, "%.*s/auth/v1", (int)n, g_url);
  } else {
    fprintf(stderr, "[auth] neither JO_JWT_ISS nor SUPABASE_URL is set: "
                    "token issuer will NOT be validated\n");
  }
  /* Fetch the JWKS now, off the loop, so the first asymmetric token after
   * boot finds a document rather than a 401. */
  if (g_url) jwks_start();
}

/* base64url -> bytes. Returns malloc'd buffer, sets *out_len. NULL on error. */
static unsigned char *b64url_decode(const char *in, size_t in_len, size_t *out_len) {
  static const char *T =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  int rev[256]; memset(rev, -1, sizeof rev);
  for (int i = 0; i < 64; i++) rev[(unsigned char)T[i]] = i;
  size_t cap = (in_len / 4 + 1) * 3 + 3;
  unsigned char *out = malloc(cap);
  if (!out) return NULL;
  /* `val` is UNSIGNED and masked to the 16 bits the loop can still need.
   * As a signed int it overflowed on the third accumulated sextet — UBSan:
   * "left shift of 805815650 by 6 places cannot be represented in type
   * 'int'" — and this runs on the Authorization header of every request,
   * before any authentication, so the input is fully attacker-chosen. It
   * wraps benignly today; signed overflow is UB and need not. */
  size_t o = 0; unsigned val = 0; int bits = -8;
  for (size_t i = 0; i < in_len; i++) {
    int c = rev[(unsigned char)in[i]];
    if (c < 0) continue;
    val = ((val << 6) | (unsigned)c) & 0xFFFFu; bits += 6;
    if (bits >= 0) { out[o++] = (unsigned char)((val >> bits) & 0xFF); bits -= 8; }
  }
  *out_len = o; return out;
}

static int verify_hs256(const char *secret, const char *signing_input,
                        size_t si_len, const char *sig_b64, size_t sig_len) {
  if (!secret || !*secret) return 0;
  unsigned char mac[32]; unsigned int mlen = 0;
  HMAC(EVP_sha256(), secret, (int)strlen(secret),
       (const unsigned char *)signing_input, si_len, mac, &mlen);
  size_t glen = 0;
  unsigned char *given = b64url_decode(sig_b64, sig_len, &glen);
  int ok = (given && glen == 32 && mlen == 32 &&
            CRYPTO_memcmp(mac, given, 32) == 0);
  free(given);
  return ok;
}

/* ── Asymmetric (RS256/ES256) via the project JWKS — port of jose
 * createRemoteJWKSet + jwtVerify(algorithms:['ES256','RS256']).
 *
 * THE EVENT LOOP NEVER FETCHES. auth_check() runs on the mongoose loop for
 * every /api request, before anything is authenticated, and it used to call
 * http_request() (8 s timeout) inline whenever the cache was older than
 * JWKS_TTL or a token named a kid the cache lacked. One request with a
 * made-up kid froze every connection for up to 8 s, and the throttle meant to
 * stop it was keyed on the kid — a fresh random kid per request was a fresh
 * allowance per request, and the 64-slot negative cache was a ring the same
 * spray rotated straight through.
 *
 * Now one refresher thread owns the network: it fetches at boot, every
 * JWKS_TTL, and when asked. The loop only reads the cached document. An
 * unknown kid answers 401 at once and ASKS for a refresh — jose's "refetch
 * once on unknown kid", made asynchronous — and asks are single-flight and
 * charged against ONE global budget (RL_JWKS, key "global": at most one forced
 * refetch per JWKS_REFETCH_MIN_SEC however many kids are sprayed, and the
 * class fails closed under limiter-table pressure, see ratelimit.h). The cost
 * of a genuine key rotation is that the first token signed with the new key
 * gets a 401 and its retry, one round trip later, verifies. */
static pthread_mutex_t g_jwks_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_jwks_cv   = PTHREAD_COND_INITIALIZER;
static pthread_once_t  g_jwks_once = PTHREAD_ONCE_INIT;
static char  *g_jwks_doc     = NULL;
static time_t g_jwks_at      = 0;   /* last successful fetch            */
static time_t g_jwks_tried   = 0;   /* last attempt, success or not     */
static int    g_jwks_want    = 0;   /* refresh requested, not yet taken */
static long   g_jwks_fetches = 0;   /* attempts since boot              */
#define JWKS_TTL              600
#define JWKS_REFETCH_MIN_SEC  30    /* global forced-refetch budget     */
#define JWKS_RETRY_SEC        30    /* after a failed fetch             */

static char *http_get_jwks(void) {
  if (!g_url) return NULL;
  char url[1024];
  snprintf(url, sizeof url, "%s/auth/v1/.well-known/jwks.json", g_url);
  http_client *c = http_client_new();
  if (!c) return NULL;
  http_response r = {0};
  int rc = http_request(c, "GET", url, NULL, NULL, 0, 8000, 1, &r);
  char *doc = NULL;
  if (rc == 0 && r.status >= 200 && r.status < 300 && r.body)
    doc = strdup(r.body);
  http_response_free(&r);
  http_client_free(c);
  return doc;
}
/* The one network call. A pointer so tests/unit/test_auth_jwks.c can count
 * fetches, and make one slow, without a server. */
static char *(*g_jwks_fetch)(void) = http_get_jwks;

static void kidneg_clear(void);

static void *jwks_refresher(void *unused) {
  (void)unused;
  int failed = 0;
  pthread_mutex_lock(&g_jwks_lock);
  for (;;) {
    while (!g_jwks_want) {
      time_t now = time(NULL);
      time_t due = !g_jwks_fetches ? now
                 : failed          ? g_jwks_tried + JWKS_RETRY_SEC
                                   : g_jwks_at + JWKS_TTL;
      if (now >= due) break;
      struct timespec ts = { .tv_sec = due, .tv_nsec = 0 };  /* CLOCK_REALTIME */
      pthread_cond_timedwait(&g_jwks_cv, &g_jwks_lock, &ts);
    }
    g_jwks_want = 0;
    g_jwks_fetches++;
    g_jwks_tried = time(NULL);
    pthread_mutex_unlock(&g_jwks_lock);

    char *d = g_jwks_fetch();                  /* no lock held: may take 8 s */

    pthread_mutex_lock(&g_jwks_lock);
    if (d) {
      /* A changed document may hold a kid already written off — the rotation
       * case the negative cache must not outlive. */
      if (!g_jwks_doc || strcmp(g_jwks_doc, d) != 0) kidneg_clear();
      free(g_jwks_doc); g_jwks_doc = d; g_jwks_at = time(NULL);
      failed = 0;
    } else {
      /* Stale-while-error: the previous document keeps verifying. */
      failed = 1;
      fprintf(stderr, "[auth] JWKS fetch failed (%s); retry in %ds\n",
              g_jwks_doc ? "serving the cached document" : "no document yet",
              JWKS_RETRY_SEC);
    }
  }
  return NULL;                                 /* not reached */
}

static void jwks_thread_start(void) {
  pthread_t th;
  if (pthread_create(&th, NULL, jwks_refresher, NULL) == 0)
    pthread_detach(th);
  else
    fprintf(stderr, "[auth] could not start the JWKS refresher: asymmetric "
                    "tokens will not verify\n");
}

static void jwks_start(void) { pthread_once(&g_jwks_once, jwks_thread_start); }

/* Ask the refresher for an early fetch. Never touches the network. Returns 1
 * if this call scheduled one, 0 if one was already pending or the global
 * budget is spent. */
static int jwks_request_refresh(void) {
  if (!g_url) return 0;
  jwks_start();
  pthread_mutex_lock(&g_jwks_lock);
  int sched = 0;
  if (!g_jwks_want &&
      ratelimit_allow(RL_JWKS, "global", 1, JWKS_REFETCH_MIN_SEC, NULL)) {
    g_jwks_want = 1;
    pthread_cond_signal(&g_jwks_cv);
    sched = 1;
  }
  pthread_mutex_unlock(&g_jwks_lock);
  return sched;
}

/* Parsed copy of the cached JWKS doc (caller cJSON_Delete), or NULL if none
 * has been fetched yet. Memory only. */
static cJSON *jwks_doc(void) {
  pthread_mutex_lock(&g_jwks_lock);
  cJSON *j = g_jwks_doc ? cJSON_Parse(g_jwks_doc) : NULL;
  pthread_mutex_unlock(&g_jwks_lock);
  return j;
}

static cJSON *find_jwk(cJSON *doc, const char *kid) {
  if (!doc) return NULL;
  cJSON *keys = cJSON_GetObjectItem(doc, "keys");
  if (!cJSON_IsArray(keys)) return NULL;
  cJSON *k;
  cJSON_ArrayForEach(k, keys) {
    cJSON *kk = cJSON_GetObjectItem(k, "kid");
    if (!kid || (kk && cJSON_IsString(kk) && !strcmp(kk->valuestring, kid)))
      return k;
  }
  return NULL;
}

/* Build an EVP_PKEY from the JWK parameters via the OpenSSL 3.0 provider path
 * (EVP_PKEY_fromdata), rather than RSA_new/RSA_set0_key/EC_KEY_* + the
 * EVP_PKEY_assign_* handoff, all of which are deprecated in 3.0.
 *
 * Same key, same verify result — only the construction route changed. The one
 * genuine difference is where the group lives: EC_KEY_new_by_curve_name took
 * an NID, fromdata takes the group by name, so P-256 is spelled "P-256" here.
 * Still P-256 only, which is what ES256 means and all verify_asym() accepts.
 *
 * fromdata wants the EC public key as a single uncompressed SEC1 point
 * (0x04 || X || Y) with X and Y at the curve's fixed 32-byte width, whereas
 * BN_bin2bn + affine coordinates accepted whatever width the JWK carried. A
 * few JWKS producers strip leading zero bytes from x/y, so the halves are
 * LEFT-PADDED into place rather than memcpy'd at offset 1 — dropping that
 * padding would shift a stripped coordinate left and reject every token from
 * such a key. Anything wider than 32 bytes is not a P-256 coordinate and is
 * refused instead of being silently truncated. */
static EVP_PKEY *pkey_fromdata(const char *type, OSSL_PARAM *params) {
  EVP_PKEY *pk = NULL;
  EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_from_name(NULL, type, NULL);
  if (ctx) {
    if (EVP_PKEY_fromdata_init(ctx) != 1 ||
        EVP_PKEY_fromdata(ctx, &pk, EVP_PKEY_PUBLIC_KEY, params) != 1)
      pk = NULL;
    EVP_PKEY_CTX_free(ctx);
  }
  return pk;
}

static EVP_PKEY *jwk_to_pkey(cJSON *jwk) {
  cJSON *kty = cJSON_GetObjectItem(jwk, "kty");
  if (!cJSON_IsString(kty)) return NULL;
  if (!strcmp(kty->valuestring, "RSA")) {
    cJSON *N = cJSON_GetObjectItem(jwk, "n"), *E = cJSON_GetObjectItem(jwk, "e");
    if (!cJSON_IsString(N) || !cJSON_IsString(E)) return NULL;
    size_t nl = 0, el = 0;
    unsigned char *nb = b64url_decode(N->valuestring, strlen(N->valuestring), &nl);
    unsigned char *eb = b64url_decode(E->valuestring, strlen(E->valuestring), &el);
    EVP_PKEY *pk = NULL;
    if (nb && eb) {
      BIGNUM *bn = BN_bin2bn(nb, nl, NULL), *be = BN_bin2bn(eb, el, NULL);
      OSSL_PARAM_BLD *bld = OSSL_PARAM_BLD_new();
      OSSL_PARAM *params = NULL;
      if (bn && be && bld &&
          OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_N, bn) == 1 &&
          OSSL_PARAM_BLD_push_BN(bld, OSSL_PKEY_PARAM_RSA_E, be) == 1 &&
          (params = OSSL_PARAM_BLD_to_param(bld)) != NULL)
        pk = pkey_fromdata("RSA", params);
      OSSL_PARAM_free(params);
      OSSL_PARAM_BLD_free(bld);
      BN_free(bn); BN_free(be);
    }
    free(nb); free(eb);
    return pk;
  }
  if (!strcmp(kty->valuestring, "EC")) {
    cJSON *X = cJSON_GetObjectItem(jwk, "x"), *Y = cJSON_GetObjectItem(jwk, "y");
    if (!cJSON_IsString(X) || !cJSON_IsString(Y)) return NULL;
    size_t xl = 0, yl = 0;
    unsigned char *xb = b64url_decode(X->valuestring, strlen(X->valuestring), &xl);
    unsigned char *yb = b64url_decode(Y->valuestring, strlen(Y->valuestring), &yl);
    EVP_PKEY *pk = NULL;
    if (xb && yb && xl && yl && xl <= 32 && yl <= 32) {
      unsigned char pt[65] = {0};             /* 0x04 || X(32) || Y(32) */
      pt[0] = 0x04;
      memcpy(pt + 1  + (32 - xl), xb, xl);    /* left-pad, see above */
      memcpy(pt + 33 + (32 - yl), yb, yl);
      OSSL_PARAM_BLD *bld = OSSL_PARAM_BLD_new();
      OSSL_PARAM *params = NULL;
      if (bld &&
          OSSL_PARAM_BLD_push_utf8_string(bld, OSSL_PKEY_PARAM_GROUP_NAME,
                                          "P-256", 0) == 1 &&
          OSSL_PARAM_BLD_push_octet_string(bld, OSSL_PKEY_PARAM_PUB_KEY,
                                           pt, sizeof pt) == 1 &&
          (params = OSSL_PARAM_BLD_to_param(bld)) != NULL)
        pk = pkey_fromdata("EC", params);
      OSSL_PARAM_free(params);
      OSSL_PARAM_BLD_free(bld);
    }
    free(xb); free(yb);
    return pk;
  }
  return NULL;
}

static int verify_asym(const char *si, size_t si_len, const char *sig_b64,
                       size_t sig_b64_len, EVP_PKEY *pk, const char *alg) {
  size_t sl = 0;
  unsigned char *sg = b64url_decode(sig_b64, sig_b64_len, &sl);
  if (!sg) return 0;
  int ok = 0;
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (ctx && !strcmp(alg, "RS256")) {
    if (EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, pk) == 1 &&
        EVP_DigestVerify(ctx, sg, sl, (const unsigned char *)si, si_len) == 1)
      ok = 1;
  } else if (ctx && !strcmp(alg, "ES256") && sl == 64) {
    /* JWT ES256 signature is raw r||s (P-256) — wrap as DER ECDSA_SIG. */
    ECDSA_SIG *es = ECDSA_SIG_new();
    if (es && ECDSA_SIG_set0(es, BN_bin2bn(sg, 32, NULL),
                                 BN_bin2bn(sg + 32, 32, NULL)) == 1) {
      unsigned char *der = NULL;
      int dl = i2d_ECDSA_SIG(es, &der);
      if (dl > 0 &&
          EVP_DigestVerifyInit(ctx, NULL, EVP_sha256(), NULL, pk) == 1 &&
          EVP_DigestVerify(ctx, der, dl, (const unsigned char *)si, si_len) == 1)
        ok = 1;
      if (der) OPENSSL_free(der);
    }
    if (es) ECDSA_SIG_free(es);
  }
  if (ctx) EVP_MD_CTX_free(ctx);
  free(sg);
  return ok;
}

/* JWT header (first segment) -> alg/kid copies. Returns 1 on parse. */
static int jwt_header(const char *tok, const char *d1, char *alg, size_t an,
                      char *kid, size_t kn) {
  alg[0] = 0; kid[0] = 0;
  size_t hl = 0;
  unsigned char *hb = b64url_decode(tok, (size_t)(d1 - tok), &hl);
  if (!hb) return 0;
  char *hj = malloc(hl + 1);
  if (!hj) { free(hb); return 0; }
  memcpy(hj, hb, hl); hj[hl] = 0; free(hb);
  cJSON *h = cJSON_Parse(hj); free(hj);
  if (!h) return 0;
  cJSON *a = cJSON_GetObjectItem(h, "alg");
  cJSON *k = cJSON_GetObjectItem(h, "kid");
  if (a && cJSON_IsString(a)) snprintf(alg, an, "%s", a->valuestring);
  if (k && cJSON_IsString(k)) snprintf(kid, kn, "%s", k->valuestring);
  cJSON_Delete(h);
  return alg[0] != 0;
}

/* ── unknown-kid negative cache ────────────────────────────────────────────
 * No longer the network guard — the loop cannot fetch at all now, and forced
 * refetches are globally budgeted. What it still buys is that a replayed bogus
 * kid is a memcmp instead of a JWKS re-parse plus a refresh request. Cleared
 * whenever the refresher installs a CHANGED document. */
#define KIDNEG_SLOTS 64
#define KIDNEG_TTL   300
static struct { char kid[256]; time_t at; } g_kidneg[KIDNEG_SLOTS];
static int g_kidneg_next;                    /* round-robin victim */

/* Callers hold g_jwks_lock. */
static void kidneg_clear(void) {
  memset(g_kidneg, 0, sizeof g_kidneg);
  g_kidneg_next = 0;
}
static int kid_known_bad(const char *kid, time_t now) {
  for (int i = 0; i < KIDNEG_SLOTS; i++)
    if (g_kidneg[i].kid[0] && strcmp(g_kidneg[i].kid, kid) == 0)
      return (now - g_kidneg[i].at) < KIDNEG_TTL;
  return 0;
}
static void kid_mark_bad(const char *kid, time_t now) {
  for (int i = 0; i < KIDNEG_SLOTS; i++)
    if (g_kidneg[i].kid[0] && strcmp(g_kidneg[i].kid, kid) == 0) {
      g_kidneg[i].at = now; return;
    }
  int v = g_kidneg_next++ % KIDNEG_SLOTS;
  snprintf(g_kidneg[v].kid, sizeof g_kidneg[v].kid, "%s", kid);
  g_kidneg[v].at = now;
}

/* RS256/ES256 over the CACHED JWKS. A kid the cache lacks (or no cache yet)
 * fails now and schedules a refresh; it never waits for one. */
static int verify_jwks(const char *tok, const char *d1, const char *si,
                       size_t si_len, const char *sig, size_t sig_len) {
  if (!g_url) return 0;
  char alg[16], kid[256];
  if (!jwt_header(tok, d1, alg, sizeof alg, kid, sizeof kid)) return 0;
  if (strcmp(alg, "RS256") != 0 && strcmp(alg, "ES256") != 0) return 0;

  if (kid[0]) {
    time_t now = time(NULL);
    pthread_mutex_lock(&g_jwks_lock);
    int bad = kid_known_bad(kid, now);
    pthread_mutex_unlock(&g_jwks_lock);
    if (bad) return 0;                       /* already looked up, not there */
  }

  cJSON *doc = jwks_doc();
  if (!doc) { jwks_request_refresh(); return 0; }
  int ok = 0;
  cJSON *jwk = find_jwk(doc, kid[0] ? kid : NULL);
  if (jwk) {
    EVP_PKEY *pk = jwk_to_pkey(jwk);
    if (pk) { ok = verify_asym(si, si_len, sig, sig_len, pk, alg);
              EVP_PKEY_free(pk); }
  } else {
    if (kid[0]) {
      time_t now = time(NULL);
      pthread_mutex_lock(&g_jwks_lock);
      kid_mark_bad(kid, now);
      pthread_mutex_unlock(&g_jwks_lock);
    }
    jwks_request_refresh();
  }
  cJSON_Delete(doc);
  return ok;
}

/* ── verified-email signal ─────────────────────────────────────────────────
 * tenantapi.c claims pending invites on the token's `email` claim, which turns
 * "I can sign up as alice@corp.example" into "I am a member of corp's
 * workspace" whenever the IdP hands out a token before the address is
 * confirmed. It needs to know whether the address was actually verified.
 *
 * Thread-local rather than a field on auth_user: auth.h is a published header
 * with other consumers, and auth_check() → tenant_resolve() run back-to-back
 * on the same request thread, so the value is never read across a boundary
 * where a struct field would have been safer. Reset on every auth_check so a
 * failed verification can never leave a stale `true` behind. */
static __thread int g_email_verified = 0;

/* Supabase puts email_verified at the top level on newer projects and under
 * user_metadata on older ones; GoTrue also emits `email_confirmed_at`. Accept
 * any of them, treat anything else as unverified. */
static int email_verified_claim(cJSON *j) {
  cJSON *v = cJSON_GetObjectItem(j, "email_verified");
  if (v && cJSON_IsBool(v))   return cJSON_IsTrue(v) ? 1 : 0;
  if (v && cJSON_IsString(v)) return strcmp(v->valuestring, "true") == 0;
  cJSON *ca = cJSON_GetObjectItem(j, "email_confirmed_at");
  if (ca && cJSON_IsString(ca) && ca->valuestring[0]) return 1;
  cJSON *um = cJSON_GetObjectItem(j, "user_metadata");
  if (um && cJSON_IsObject(um)) {
    cJSON *uv = cJSON_GetObjectItem(um, "email_verified");
    if (uv && cJSON_IsBool(uv))   return cJSON_IsTrue(uv) ? 1 : 0;
    if (uv && cJSON_IsString(uv)) return strcmp(uv->valuestring, "true") == 0;
  }
  return 0;
}

int auth_email_verified(void) { return g_email_verified; }

/* ── break-glass tokens ─────────────────────────────────────────────────────
 * keysapi_breakglass() mints an HS256 token with BREAK_GLASS_JWT_SECRET after
 * a TOTP login, for when Supabase auth is down — and nothing here ever
 * verified that secret, so every break-glass token was a 401: the emergency
 * path had never worked. It is verified now, under the issuer's own rules and
 * nothing looser: the same three env gates the issuer applies (enabled, set,
 * not reused as SUPABASE_JWT_SECRET), alg HS256, the claims the issuer writes
 * (break_glass:true, sub "break-glass-admin"), and a lifetime no longer than
 * the issuer grants. Read per call, like the issuer, so the two agree on
 * whether break-glass is on. The ALLOW carries out->break_glass, which
 * opgate_check() maps to operator and httpd.c audits per request. */
static const char *bg_secret(void) {
  const char *en = getenv("BREAK_GLASS_ENABLED");
  if (!en || strcmp(en, "1") != 0) return NULL;
  const char *s = getenv("BREAK_GLASS_JWT_SECRET");
  if (!s || !*s) return NULL;
  const char *sup = getenv("SUPABASE_JWT_SECRET");
  if (sup && *sup && strcmp(s, sup) == 0) return NULL;   /* reuse guard */
  return s;
}

/* Claims check for a token whose signature verified under bg_secret(). */
static int bg_claims_ok(cJSON *j, time_t now) {
  cJSON *bg  = cJSON_GetObjectItem(j, "break_glass");
  cJSON *sub = cJSON_GetObjectItem(j, "sub");
  cJSON *iat = cJSON_GetObjectItem(j, "iat");
  cJSON *exp = cJSON_GetObjectItem(j, "exp");
  if (!cJSON_IsTrue(bg)) return 0;
  if (!cJSON_IsString(sub) || strcmp(sub->valuestring, "break-glass-admin") != 0)
    return 0;
  if (!cJSON_IsNumber(iat) || !cJSON_IsNumber(exp)) return 0;
  double t = (double)now;
  if (t >= exp->valuedouble) return 0;                    /* expired      */
  if (iat->valuedouble > t + 60) return 0;                /* minted ahead */
  if (exp->valuedouble - iat->valuedouble > BREAK_GLASS_TTL_SEC) return 0;
  if (exp->valuedouble - t > BREAK_GLASS_TTL_SEC + 60) return 0;
  return 1;
}

auth_result auth_check(const char *hdr, auth_user *out) {
  g_email_verified = 0;
  const char *bgs = bg_secret();
  if (!g_secret && !g_url && !bgs) return AUTH_503_UNCONFIG;

  if (!hdr) return AUTH_401_MISSING;
  while (*hdr == ' ') hdr++;
  if (strncasecmp(hdr, "Bearer", 6) != 0 || !isspace((unsigned char)hdr[6]))
    return AUTH_401_MISSING;
  const char *tok = hdr + 6;
  while (*tok == ' ' || *tok == '\t') tok++;
  if (!*tok) return AUTH_401_MISSING;

  /* split header.payload.signature */
  const char *d1 = strchr(tok, '.');
  if (!d1) return AUTH_401_INVALID;
  const char *d2 = strchr(d1 + 1, '.');
  if (!d2) return AUTH_401_INVALID;
  size_t si_len = (size_t)(d2 - tok);          /* header.payload */
  const char *sig = d2 + 1;
  size_t sig_len = strlen(sig);

  /* (1) Asymmetric ES256/RS256 via the project JWKS (auth.js step 1), then
   * (2) legacy symmetric HS256 (step 2). Either verifying is sufficient. */
  int verified = verify_jwks(tok, d1, tok, si_len, sig, sig_len);
  if (!verified && g_secret)
    verified = verify_hs256(g_secret, tok, si_len, sig, sig_len);
  /* (3) break-glass, last, and only for a header that says HS256. */
  int bg = 0;
  if (!verified && bgs) {
    char alg[16], kid[256];
    if (jwt_header(tok, d1, alg, sizeof alg, kid, sizeof kid) &&
        strcmp(alg, "HS256") == 0)
      bg = verified = verify_hs256(bgs, tok, si_len, sig, sig_len);
  }
  if (!verified) return AUTH_401_INVALID;

  size_t plen = 0;
  unsigned char *pl = b64url_decode(d1 + 1, (size_t)(d2 - d1 - 1), &plen);
  if (!pl) return AUTH_401_INVALID;
  char *pjson = malloc(plen + 1);
  /* Unchecked malloc here was a NULL-pointer memcpy: this runs on the
   * Authorization header of every request, before authentication succeeds, so
   * plen is attacker-influenced (bounded only by the token they send). An
   * allocation failure must degrade to "reject this token", not crash the
   * server that every other request is also relying on. */
  if (!pjson) { free(pl); return AUTH_401_INVALID; }
  memcpy(pjson, pl, plen); pjson[plen] = 0; free(pl);
  cJSON *j = cJSON_Parse(pjson); free(pjson);
  if (!j) return AUTH_401_INVALID;

  if (bg) {
    /* The issuer sets neither aud nor iss, so the Supabase checks below do
     * not apply; bg_claims_ok() is the whole of what a break-glass token must
     * satisfy, and it is stricter than they are. */
    int okc = bg_claims_ok(j, time(NULL));
    cJSON_Delete(j);
    if (!okc) return AUTH_401_INVALID;
    if (out) {
      snprintf(out->id, sizeof out->id, "%s", "break-glass-admin");
      snprintf(out->email, sizeof out->email, "%s", "break-glass@local");
      snprintf(out->role, sizeof out->role, "%s", "service_role");
      out->break_glass = 1;
    }
    return AUTH_ALLOW;
  }

  /* `exp` is MANDATORY and must be a number. The old form ("check it if it is
   * present and numeric") meant a token with no exp — or with exp as the
   * string "9999999999" — never expired: forever-valid credentials, issued by
   * anyone who can mint one token. jose treats a malformed exp as a failure,
   * and so must we. */
  cJSON *exp = cJSON_GetObjectItem(j, "exp");
  if (!exp || !cJSON_IsNumber(exp)) { cJSON_Delete(j); return AUTH_401_INVALID; }
  if ((double)time(NULL) >= exp->valuedouble) {
    cJSON_Delete(j); return AUTH_401_INVALID;
  }
  cJSON *nbf = cJSON_GetObjectItem(j, "nbf");   /* jose enforces nbf too */
  if (nbf && cJSON_IsNumber(nbf) && (double)time(NULL) < nbf->valuedouble) {
    cJSON_Delete(j); return AUTH_401_INVALID;
  }
  /* RFC 7519 §4.1.3: `aud` is a string OR an array of strings. Skipping the
   * array case (as this did) meant an array-audience token was accepted with
   * NO audience check at all — exactly the shape a token minted for a
   * different service has. Present-but-neither-form is a reject, not a skip. */
  cJSON *aud = cJSON_GetObjectItem(j, "aud");
  if (g_aud && aud && !cJSON_IsNull(aud)) {
    int match = 0;
    if (cJSON_IsString(aud)) {
      match = strcmp(aud->valuestring, g_aud) == 0;
    } else if (cJSON_IsArray(aud)) {
      cJSON *a;
      cJSON_ArrayForEach(a, aud)
        if (cJSON_IsString(a) && strcmp(a->valuestring, g_aud) == 0) { match = 1; break; }
    }
    if (!match) { cJSON_Delete(j); return AUTH_401_INVALID; }
  }
  /* Issuer. Only enforced when we know what to expect (see auth_init). */
  if (g_iss[0]) {
    cJSON *iss = cJSON_GetObjectItem(j, "iss");
    if (!iss || !cJSON_IsString(iss) || strcmp(iss->valuestring, g_iss) != 0) {
      cJSON_Delete(j); return AUTH_401_INVALID;
    }
  }
  cJSON *sub = cJSON_GetObjectItem(j, "sub");
  if (!sub || !cJSON_IsString(sub) || !sub->valuestring[0]) {
    cJSON_Delete(j); return AUTH_401_NO_SUB;
  }
  if (out) {
    snprintf(out->id, sizeof out->id, "%s", sub->valuestring);
    cJSON *em = cJSON_GetObjectItem(j, "email");
    snprintf(out->email, sizeof out->email, "%s",
             (em && cJSON_IsString(em)) ? em->valuestring : "");
    cJSON *ro = cJSON_GetObjectItem(j, "role");
    snprintf(out->role, sizeof out->role, "%s",
             (ro && cJSON_IsString(ro)) ? ro->valuestring : "authenticated");
    out->break_glass = 0;    /* a break_glass CLAIM under the IdP's key is inert */
  }
  g_email_verified = email_verified_claim(j);
  cJSON_Delete(j);
  return AUTH_ALLOW;
}

int auth_status(auth_result r) {
  switch (r) {
    case AUTH_ALLOW:        return 200;
    case AUTH_503_UNCONFIG: return 503;
    default:                return 401;
  }
}
const char *auth_body(auth_result r) {
  switch (r) {
    case AUTH_401_MISSING:  return "{\"error\":\"Missing bearer token\"}";
    case AUTH_401_INVALID:  return "{\"error\":\"Invalid token\"}";
    case AUTH_401_NO_SUB:   return "{\"error\":\"Token missing sub\"}";
    case AUTH_503_UNCONFIG: return "{\"error\":\"Auth not configured\"}";
    default:                return "{}";
  }
}
