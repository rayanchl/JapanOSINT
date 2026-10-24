#include "catrust.h"

#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef JO_REPO_ROOT
#define JO_REPO_ROOT "/Users/rayan/OSINTsaas"
#endif

/* Under native/, not the repo's /data/, which .gitignore holds for relocated
 * RUNTIME data (models, the breach corpus). This bundle is SOURCE: it is
 * reviewed, committed and shipped with the code that loads it. */
#define CATRUST_REL "/native/data/tls/extra-intermediates.pem"

const char *catrust_extra_path(void) {
  const char *e = getenv("JO_CA_EXTRA");
  if (e && *e) return e;
  const char *r = getenv("JO_REPO_ROOT");
  if (r && *r) {
    static __thread char buf[1024];
    snprintf(buf, sizeof buf, "%s" CATRUST_REL, r);
    return buf;
  }
  return JO_REPO_ROOT CATRUST_REL;
}

/* OpenSSL is already a link dependency of this tree (the Makefile's
 * pkg-config line, and core/breach_index.c's SHA1), so the headers are always
 * there. What is NOT guaranteed is that the libcurl we link was built against
 * it: only an OpenSSL-backed libcurl takes an SSL_CTX callback, and one built
 * against another backend answers CURLE_NOT_BUILT_IN at setopt time. That is
 * reported as -1 and the handle behaves exactly as it did before this file
 * existed — never as a weaker handshake. */
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

/* The parsed bundle, read once. X509_STORE_add_cert up-refs, so one parsed
 * copy serves every connection and the file is not re-read per request. */
static pthread_once_t g_once = PTHREAD_ONCE_INIT;
static X509 **g_certs = NULL;
static int    g_n = 0;          /* >=0 parsed, -1 unreadable bundle */

/* A self-signed certificate is a ROOT: subject and issuer are the same name.
 * OpenSSL accepts a chain only when it ends at a self-signed certificate that
 * was already trusted (X509_V_FLAG_PARTIAL_CHAIN is not set), so admitting one
 * here would be adding a trust anchor — the one thing this file must not do. */
static int catrust_is_self_signed(X509 *x) {
  return X509_NAME_cmp(X509_get_subject_name(x), X509_get_issuer_name(x)) == 0;
}

static void catrust_load(void) {
  const char *path = catrust_extra_path();
  FILE *f = fopen(path, "rb");
  if (!f) {
    /* No bundle is the ordinary case for a deployment that does not need one;
     * say nothing, behave as before. */
    g_n = 0;
    return;
  }
  int cap = 16, n = 0, refused = 0;
  X509 **v = calloc((size_t)cap, sizeof *v);
  if (!v) { fclose(f); g_n = -1; return; }
  X509 *x;
  while ((x = PEM_read_X509(f, NULL, NULL, NULL)) != NULL) {
    /* THE SELF-SIGNED CHECK IS HERE, at load, not at every handshake: a
     * deployment that edits this file cannot introduce a trust anchor, and the
     * refusal is said once rather than once per connection per certificate.
     * A root is what this file must never carry; an intermediate is all it is
     * for, and tests/unit/test_catrust.c fails the build over one. */
    if (catrust_is_self_signed(x)) {
      fprintf(stderr, "[catrust] refusing a SELF-SIGNED certificate in %s — "
                      "this bundle may hold only intermediates\n", path);
      X509_free(x);
      refused++;
      continue;
    }
    if (n == cap) {
      X509 **nv = realloc(v, (size_t)cap * 2 * sizeof *v);
      if (!nv) { X509_free(x); break; }
      v = nv; cap *= 2;
    }
    v[n++] = x;
  }
  ERR_clear_error();                 /* the loop ends on a parse error by design */
  fclose(f);
  if (!n) {
    free(v);
    fprintf(stderr, "[catrust] %s holds no usable certificate%s\n", path,
            refused ? " (every entry was self-signed)" : "");
    g_n = -1;
    return;
  }
  g_certs = v;
  g_n = n;
}

static CURLcode on_ssl_ctx(CURL *e, void *sslctx, void *parm) {
  (void)e; (void)parm;
  SSL_CTX *ctx = (SSL_CTX *)sslctx;
  X509_STORE *st = SSL_CTX_get_cert_store(ctx);
  if (!st) return CURLE_OK;
  for (int i = 0; i < g_n; i++) {
    /* Already present is the common answer on a reused store; it is not a
     * failure, and leaving it in the error queue would confuse the next
     * OpenSSL call into reporting it. */
    if (!X509_STORE_add_cert(st, g_certs[i])) ERR_clear_error();
  }
  return CURLE_OK;
}

int catrust_install(void *curl_easy) {
  CURL *e = (CURL *)curl_easy;
  if (!e) return 0;
  pthread_once(&g_once, catrust_load);
  if (g_n <= 0) return g_n < 0 ? -1 : 0;
  if (curl_easy_setopt(e, CURLOPT_SSL_CTX_FUNCTION, on_ssl_ctx) != CURLE_OK)
    return -1;
  return g_n;
}

int catrust_count(void) {
  pthread_once(&g_once, catrust_load);
  return g_n;
}
