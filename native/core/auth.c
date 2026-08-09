#include "auth.h"
#include "httpclient.h"
#include "../third_party/cJSON.h"
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/ec.h>
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

static int verify_hs256(const char *signing_input, size_t si_len,
                        const char *sig_b64, size_t sig_len) {
  if (!g_secret) return 0;
  unsigned char mac[32]; unsigned int mlen = 0;
  HMAC(EVP_sha256(), g_secret, (int)strlen(g_secret),
       (const unsigned char *)signing_input, si_len, mac, &mlen);
  size_t glen = 0;
  unsigned char *given = b64url_decode(sig_b64, sig_len, &glen);
  int ok = (given && glen == 32 && mlen == 32 &&
            CRYPTO_memcmp(mac, given, 32) == 0);
  free(given);
  return ok;
}

/* ── Asymmetric (RS256/ES256) via the project JWKS — port of jose
 * createRemoteJWKSet + jwtVerify(algorithms:['ES256','RS256']). JWKS doc
 * cached 10 min; on unknown kid we refetch once (jose semantics). ── */
static pthread_mutex_t g_jwks_lock = PTHREAD_MUTEX_INITIALIZER;
static char  *g_jwks_doc = NULL;
static time_t g_jwks_at  = 0;
#define JWKS_TTL 600

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

/* cJSON of the cached JWKS doc (caller cJSON_Delete); force=1 refetches. */
static cJSON *jwks_doc(int force) {
  pthread_mutex_lock(&g_jwks_lock);
  time_t now = time(NULL);
  if (force || !g_jwks_doc || now - g_jwks_at > JWKS_TTL) {
    char *d = http_get_jwks();
    if (d) { free(g_jwks_doc); g_jwks_doc = d; g_jwks_at = now; }
  }
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
      RSA *rsa = RSA_new();
      RSA_set0_key(rsa, BN_bin2bn(nb, nl, NULL), BN_bin2bn(eb, el, NULL), NULL);
      pk = EVP_PKEY_new();
      if (EVP_PKEY_assign_RSA(pk, rsa) != 1) { RSA_free(rsa); EVP_PKEY_free(pk); pk = NULL; }
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
    if (xb && yb) {
      EC_KEY *ec = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
      BIGNUM *bx = BN_bin2bn(xb, xl, NULL), *by = BN_bin2bn(yb, yl, NULL);
      if (ec && bx && by &&
          EC_KEY_set_public_key_affine_coordinates(ec, bx, by) == 1) {
        pk = EVP_PKEY_new();
        if (EVP_PKEY_assign_EC_KEY(pk, ec) != 1) { EC_KEY_free(ec); EVP_PKEY_free(pk); pk = NULL; }
      } else if (ec) EC_KEY_free(ec);
      BN_free(bx); BN_free(by);
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
 * The refetch-once-on-unknown-kid rule below is jose's semantics and is right
 * for a real key rotation. It is also an amplifier: an UNAUTHENTICATED request
 * carrying a made-up kid costs us one outbound HTTPS round-trip to the IdP,
 * and the same kid replayed costs one each time. Remembering the kids we have
 * already looked up and failed to find turns a replay into a memcmp, and
 * ratelimit.c caps the rate at which genuinely-new unknown kids can force a
 * fetch. Both are needed: the cache alone is defeated by varying the kid, the
 * limiter alone would delay a real rotation. */
#define KIDNEG_SLOTS 64
#define KIDNEG_TTL   300                     /* < JWKS_TTL, so a rotation that
                                              * lands mid-window still recovers
                                              * within one refresh cycle */
static struct { char kid[256]; time_t at; } g_kidneg[KIDNEG_SLOTS];
static int g_kidneg_next;                    /* round-robin victim */

/* Caller holds g_jwks_lock. */
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

/* Try RS256/ES256 over the JWKS (refetch once on unknown kid). */
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

  int ok = 0, seen = 0;
  for (int attempt = 0; attempt < 2 && !ok; attempt++) {
    /* Only the FORCED refetch is throttled — attempt 0 is served from the
     * 10-minute cache and costs nothing. Keyed on the kid so one abusive kid
     * cannot starve a genuine rotation of a different one. */
    if (attempt == 1 &&
        !ratelimit_allow(RL_JWKS, kid[0] ? kid : "-", 3, 60, NULL))
      break;
    cJSON *doc = jwks_doc(attempt);          /* attempt 1 forces refetch */
    cJSON *jwk = find_jwk(doc, kid[0] ? kid : NULL);
    if (jwk) {
      seen = 1;
      EVP_PKEY *pk = jwk_to_pkey(jwk);
      if (pk) { ok = verify_asym(si, si_len, sig, sig_len, pk, alg);
                EVP_PKEY_free(pk); }
    }
    if (doc) cJSON_Delete(doc);
    if (jwk) break;                          /* kid found: don't refetch */
  }
  if (!seen && kid[0]) {
    time_t now = time(NULL);
    pthread_mutex_lock(&g_jwks_lock);
    kid_mark_bad(kid, now);
    pthread_mutex_unlock(&g_jwks_lock);
  }
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

auth_result auth_check(const char *hdr, auth_user *out) {
  g_email_verified = 0;
  if (!g_secret && !g_url) return AUTH_503_UNCONFIG;

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
  if (!verified && g_secret) verified = verify_hs256(tok, si_len, sig, sig_len);
  if (!verified) return AUTH_401_INVALID;

  size_t plen = 0;
  unsigned char *pl = b64url_decode(d1 + 1, (size_t)(d2 - d1 - 1), &plen);
  if (!pl) return AUTH_401_INVALID;
  char *pjson = malloc(plen + 1);
  memcpy(pjson, pl, plen); pjson[plen] = 0; free(pl);
  cJSON *j = cJSON_Parse(pjson); free(pjson);
  if (!j) return AUTH_401_INVALID;

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
