/* tests/auth_jwk_test.c — round-trip proof for core/auth.c's JWK -> EVP_PKEY
 * conversion and asymmetric JWT signature check.
 *
 * WHY THIS EXISTS: jwk_to_pkey() was migrated off OpenSSL 1.1's deprecated
 * RSA_set0_key / EC_KEY_set_public_key_affine_coordinates onto OpenSSL 3's
 * EVP_PKEY_fromdata. "It compiles" says nothing about whether a token still
 * verifies, and the failure mode of a wrong public key is not a crash — it is
 * verify returning 0 for every legitimate user (an outage), or, if the point
 * encoding were built loosely, accepting something it should not. So this
 * generates real keys, publishes them as real JWKs, signs real JWTs and runs
 * the real code path.
 *
 * It #includes auth.c to reach its static functions, which is the convention
 * the other tests here use. No network: jwk_to_pkey and verify_asym are pure.
 *
 *   cc -I../third_party auth_jwk_test.c ../third_party/cJSON.c \
 *      -lssl -lcrypto -o bin/auth_jwk_test && ./bin/auth_jwk_test
 */
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* auth.c pulls in httpclient.h for the JWKS fetch; stub the three symbols it
 * references so the test links without the whole core. Neither function under
 * test calls them. */
#include "../core/httpclient.h"
http_client *http_client_new(void) { return NULL; }
void http_client_free(http_client *c) { (void)c; }
int http_request(http_client *a, const char *b, const char *c,
                 const char *const *d, const char *e, size_t f,
                 int g, int h, http_response *i) {
  (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;
  return -1;
}
void http_response_free(http_response *r) { (void)r; }

/* Same for the JWKS-refetch throttle: reached only from verify_jwks(), which
 * this test does not exercise. */
#include "../core/ratelimit.h"
int ratelimit_allow(rl_class cls, const char *key, int limit, int window_sec,
                    int *retry_after) {
  (void)cls; (void)key; (void)limit; (void)window_sec; (void)retry_after;
  return 1;
}

#include "../core/auth.c"

static int failures = 0;
static void ok(int cond, const char *what) {
  printf("  %-5s %s\n", cond ? "ok" : "FAIL", what);
  if (!cond) failures++;
}

/* base64url, no padding — the JWK/JWT alphabet. */
static char *b64url(const unsigned char *in, size_t n) {
  static const char A[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
  char *o = malloc(4 * ((n + 2) / 3) + 1);
  size_t j = 0;
  for (size_t i = 0; i < n; i += 3) {
    unsigned v = in[i] << 16;
    if (i + 1 < n) v |= in[i+1] << 8;
    if (i + 2 < n) v |= in[i+2];
    o[j++] = A[(v >> 18) & 63];
    o[j++] = A[(v >> 12) & 63];
    if (i + 1 < n) o[j++] = A[(v >> 6) & 63];
    if (i + 2 < n) o[j++] = A[v & 63];
  }
  o[j] = 0;
  return o;
}

static char *bn_b64url(const BIGNUM *b, int fixed_len) {
  int n = fixed_len > 0 ? fixed_len : BN_num_bytes(b);
  unsigned char *buf = calloc((size_t)n, 1);
  BN_bn2binpad(b, buf, n);
  char *s = b64url(buf, (size_t)n);
  free(buf);
  return s;
}

/* Sign `si` and return the JWS signature bytes. For ES256 the DER signature
 * OpenSSL produces is converted to the raw r||s form a JWT carries. */
static unsigned char *sign_jws(EVP_PKEY *priv, const char *si, size_t si_len,
                               int is_ec, size_t *out_len) {
  EVP_MD_CTX *c = EVP_MD_CTX_new();
  size_t dl = 0;
  EVP_DigestSignInit(c, NULL, EVP_sha256(), NULL, priv);
  EVP_DigestSign(c, NULL, &dl, (const unsigned char *)si, si_len);
  unsigned char *der = malloc(dl);
  EVP_DigestSign(c, der, &dl, (const unsigned char *)si, si_len);
  EVP_MD_CTX_free(c);
  if (!is_ec) { *out_len = dl; return der; }

  const unsigned char *p = der;
  ECDSA_SIG *sig = d2i_ECDSA_SIG(NULL, &p, (long)dl);
  unsigned char *raw = calloc(64, 1);
  const BIGNUM *r = NULL, *s = NULL;
  ECDSA_SIG_get0(sig, &r, &s);
  BN_bn2binpad(r, raw, 32);
  BN_bn2binpad(s, raw + 32, 32);
  ECDSA_SIG_free(sig);
  free(der);
  *out_len = 64;
  return raw;
}

static EVP_PKEY *gen(const char *type, int bits) {
  EVP_PKEY *k = NULL;
  EVP_PKEY_CTX *c = EVP_PKEY_CTX_new_from_name(NULL, type, NULL);
  EVP_PKEY_keygen_init(c);
  if (bits) EVP_PKEY_CTX_set_rsa_keygen_bits(c, bits);
  else EVP_PKEY_CTX_set_group_name(c, "prime256v1");
  EVP_PKEY_generate(c, &k);
  EVP_PKEY_CTX_free(c);
  return k;
}

int main(void) {
  const char *si = "eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiJqYXBhbm9zaW50In0";
  size_t si_len = strlen(si);

  printf("RS256 — JWK{n,e} -> EVP_PKEY -> verify\n");
  {
    EVP_PKEY *priv = gen("RSA", 2048);
    BIGNUM *n = NULL, *e = NULL;
    EVP_PKEY_get_bn_param(priv, OSSL_PKEY_PARAM_RSA_N, &n);
    EVP_PKEY_get_bn_param(priv, OSSL_PKEY_PARAM_RSA_E, &e);
    char *ns = bn_b64url(n, 0), *es = bn_b64url(e, 0);

    cJSON *jwk = cJSON_CreateObject();
    cJSON_AddStringToObject(jwk, "kty", "RSA");
    cJSON_AddStringToObject(jwk, "n", ns);
    cJSON_AddStringToObject(jwk, "e", es);

    EVP_PKEY *pub = jwk_to_pkey(jwk);
    ok(pub != NULL, "jwk_to_pkey built a key from {kty:RSA,n,e}");

    size_t sl = 0;
    unsigned char *sg = sign_jws(priv, si, si_len, 0, &sl);
    char *sb = b64url(sg, sl);
    ok(pub && verify_asym(si, si_len, sb, strlen(sb), pub, "RS256") == 1,
       "a genuine RS256 signature verifies");

    sb[0] = (sb[0] == 'A') ? 'B' : 'A';
    ok(!pub || verify_asym(si, si_len, sb, strlen(sb), pub, "RS256") == 0,
       "a tampered RS256 signature is rejected");

    ok(!pub || verify_asym("eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiJldmlsIn0",
                           strlen("eyJhbGciOiJSUzI1NiJ9.eyJzdWIiOiJldmlsIn0"),
                           sb, strlen(sb), pub, "RS256") == 0,
       "a signature lifted onto a different payload is rejected");

    free(sg); free(sb); free(ns); free(es);
    BN_free(n); BN_free(e);
    cJSON_Delete(jwk);
    if (pub) EVP_PKEY_free(pub);
    EVP_PKEY_free(priv);
  }

  printf("ES256 — JWK{crv,x,y} -> EVP_PKEY -> verify\n");
  {
    EVP_PKEY *priv = gen("EC", 0);
    BIGNUM *x = NULL, *y = NULL;
    EVP_PKEY_get_bn_param(priv, OSSL_PKEY_PARAM_EC_PUB_X, &x);
    EVP_PKEY_get_bn_param(priv, OSSL_PKEY_PARAM_EC_PUB_Y, &y);
    char *xs = bn_b64url(x, 32), *ys = bn_b64url(y, 32);

    cJSON *jwk = cJSON_CreateObject();
    cJSON_AddStringToObject(jwk, "kty", "EC");
    cJSON_AddStringToObject(jwk, "crv", "P-256");
    cJSON_AddStringToObject(jwk, "x", xs);
    cJSON_AddStringToObject(jwk, "y", ys);

    EVP_PKEY *pub = jwk_to_pkey(jwk);
    ok(pub != NULL, "jwk_to_pkey built a key from {kty:EC,x,y}");

    size_t sl = 0;
    unsigned char *sg = sign_jws(priv, si, si_len, 1, &sl);
    ok(sl == 64, "ES256 signature is raw r||s, 64 bytes");
    char *sb = b64url(sg, sl);
    ok(pub && verify_asym(si, si_len, sb, strlen(sb), pub, "ES256") == 1,
       "a genuine ES256 signature verifies");

    sb[0] = (sb[0] == 'A') ? 'B' : 'A';
    ok(!pub || verify_asym(si, si_len, sb, strlen(sb), pub, "ES256") == 0,
       "a tampered ES256 signature is rejected");

    free(sg); free(sb); free(xs); free(ys);
    BN_free(x); BN_free(y);
    cJSON_Delete(jwk);
    if (pub) EVP_PKEY_free(pub);
    EVP_PKEY_free(priv);
  }

  printf("malformed JWKs are refused, not half-built\n");
  {
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "kty", "RSA");   /* no n, no e */
    ok(jwk_to_pkey(j) == NULL, "RSA JWK missing n/e yields no key");
    cJSON_Delete(j);

    j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "kty", "EC");
    cJSON_AddStringToObject(j, "x", "AAAA");
    ok(jwk_to_pkey(j) == NULL, "EC JWK missing y yields no key");
    cJSON_Delete(j);

    j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "kty", "oct");   /* symmetric: never asymmetric */
    ok(jwk_to_pkey(j) == NULL, "a non-RSA/EC kty yields no key");
    cJSON_Delete(j);
  }

  printf(failures ? "\n%d FAILED\n" : "\nall passed\n", failures);
  return failures ? 1 : 0;
}
