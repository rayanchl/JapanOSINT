/* tests/unit/test_catrust.c — the extra-intermediates bundle cannot widen
 * trust, and the property is checked rather than promised. No network.
 *
 * WHAT IS BEING PROTECTED.
 *   1. Every certificate in native/data/tls/extra-intermediates.pem is an
 *      INTERMEDIATE — not self-signed. OpenSSL accepts a chain only when it
 *      ends at a self-signed certificate that was already trusted
 *      (X509_V_FLAG_PARTIAL_CHAIN is not set), so a non-self-signed entry can
 *      complete a chain and can never anchor one. This is the whole safety
 *      argument for the file existing, so it is a build gate.
 *   2. The self-signed discriminator is not vacuous: a ROOT out of the system
 *      store is identified as self-signed by the same function. A new check
 *      that reports nothing deserves one suspicious look — CLAUDE.md records
 *      `loop-cap`'s first regex missing the very line it was written for.
 *   3. Each intermediate verifies against the SYSTEM root store on its own.
 *      That is what makes it a link in a public chain rather than a stranger:
 *      the root at the top of it was trusted before this file existed.
 *   4. None is expired. An expired intermediate cannot complete anything and
 *      would leave the row reading as a dead endpoint for a different reason.
 *   5. catrust_extra_path() resolves $JO_CA_EXTRA, then $JO_REPO_ROOT, then
 *      the root the binary was built from — never a path written into the
 *      source, the same rule as the breach corpus root.
 *   6. The engine's own parser (catrust_count) agrees with this test's count,
 *      so checks 1-4 are made about the certificates the engine will install
 *      and not about a second reading of the file.
 *
 * A path given on the command line is checked INSTEAD of the shipped bundle,
 * which is how 1-4 were confirmed to fail rather than assumed to (the engine's
 * parse is cached per process, so one run can only ever load one bundle):
 *
 *   ./test_catrust /tmp/bad.pem   # a system root appended -> SELF-SIGNED, rc=1
 *   ./test_catrust /tmp/own.pem   # an intermediate off our own CA -> rc=1,
 *                                 #   "unable to get local issuer certificate"
 *
 * Includes catrust.c to reach the file-static parse and the discriminator. */

#include "../../core/catrust.c"

#include <assert.h>

/* The default store, as OpenSSL would assemble it for a verify. Returns NULL
 * when this build has no usable system store (a Homebrew OpenSSL with no
 * SSL_CERT_FILE, for instance), in which case the chain check cannot be made
 * and says so rather than passing silently. */
static X509_STORE *system_store(void) {
  X509_STORE *st = X509_STORE_new();
  if (!st) return NULL;
  if (!X509_STORE_set_default_paths(st)) { X509_STORE_free(st); return NULL; }
  return st;
}

static char *subject_of(X509 *x) {
  static __thread char buf[512];
  X509_NAME_oneline(X509_get_subject_name(x), buf, (int)sizeof buf);
  return buf;
}

/* Verify one certificate as the target, with the rest of the bundle available
 * as untrusted links (a cross-signed intermediate can need its sibling). */
static int chains_to_system_root(X509_STORE *st, X509 *target,
                                 STACK_OF(X509) *untrusted, int *depth_out) {
  X509_STORE_CTX *c = X509_STORE_CTX_new();
  assert(c);
  assert(X509_STORE_CTX_init(c, st, target, untrusted));
  int ok = X509_verify_cert(c);
  if (!ok) {
    fprintf(stderr, "    verify says: %s\n",
            X509_verify_cert_error_string(X509_STORE_CTX_get_error(c)));
  } else if (depth_out) {
    STACK_OF(X509) *ch = X509_STORE_CTX_get0_chain(c);
    *depth_out = ch ? sk_X509_num(ch) : 0;
  }
  X509_STORE_CTX_free(c);
  return ok;
}

/* Checks 1, 3 and 4 over one bundle, read independently of catrust's cache.
 * Returns the number of certificates, or -1 on a finding. */
static int check_bundle(const char *path, X509_STORE *st) {
  FILE *f = fopen(path, "rb");
  if (!f) { fprintf(stderr, "FAILED: no bundle at %s\n", path); return -1; }
  STACK_OF(X509) *all = sk_X509_new_null();
  assert(all);
  X509 *x;
  while ((x = PEM_read_X509(f, NULL, NULL, NULL)) != NULL) assert(sk_X509_push(all, x));
  ERR_clear_error();
  fclose(f);

  int n = sk_X509_num(all);
  if (n <= 0) {
    fprintf(stderr, "FAILED: %s holds no certificate\n", path);
    return -1;
  }
  printf("  %s: %d certificates\n", path, n);

  int rc = n;
  for (int i = 0; i < n && rc > 0; i++) {
    X509 *c = sk_X509_value(all, i);

    /* 1 — an intermediate, never a root. */
    if (catrust_is_self_signed(c)) {
      fprintf(stderr, "FAILED: %s is SELF-SIGNED. This bundle may hold only\n"
                      "        intermediates; a self-signed certificate here\n"
                      "        would be a new trust anchor.\n", subject_of(c));
      rc = -1; break;
    }

    /* 4 — not expired. */
    if (X509_cmp_current_time(X509_get0_notAfter(c)) <= 0) {
      fprintf(stderr, "FAILED: %s has EXPIRED\n", subject_of(c));
      rc = -1; break;
    }

    /* 3 — the root above it was already trusted. */
    if (st) {
      int depth = 0;
      if (!chains_to_system_root(st, c, all, &depth)) {
        fprintf(stderr, "FAILED: %s does not chain to a system root. An entry\n"
                        "        here must be a link in a chain the system\n"
                        "        already trusts, fetched from its CA's own AIA\n"
                        "        URL — not a certificate from anywhere else.\n",
                subject_of(c));
        rc = -1; break;
      }
      printf("    chains to a system root in %d certs: %s\n", depth, subject_of(c));
    }
  }
  sk_X509_pop_free(all, X509_free);
  return rc;
}

/* 5 — path resolution. Runs before anything loads a bundle. */
static void test_path_resolution(void) {
  setenv("JO_CA_EXTRA", "/srv/jo/custom.pem", 1);
  assert(!strcmp(catrust_extra_path(), "/srv/jo/custom.pem"));
  unsetenv("JO_CA_EXTRA");

  setenv("JO_REPO_ROOT", "/srv/jo", 1);
  assert(!strcmp(catrust_extra_path(), "/srv/jo" CATRUST_REL));
  unsetenv("JO_REPO_ROOT");

  assert(!strcmp(catrust_extra_path(), JO_REPO_ROOT CATRUST_REL));
  printf("  path resolution: JO_CA_EXTRA > JO_REPO_ROOT > built-from root  OK\n");
}

/* 2 — the discriminator is not vacuous: real roots read as self-signed.
 * Sampled out of the system store so the test need not carry one. */
static void test_discriminator_sees_a_root(X509_STORE *st) {
  int roots = 0, self = 0;
  STACK_OF(X509_OBJECT) *objs = X509_STORE_get0_objects(st);
  for (int i = 0; objs && i < sk_X509_OBJECT_num(objs) && roots < 50; i++) {
    X509 *r = X509_OBJECT_get0_X509(sk_X509_OBJECT_value(objs, i));
    if (!r) continue;
    roots++;
    if (catrust_is_self_signed(r)) self++;
  }
  if (!roots) {
    /* A hashed-directory store lists nothing until a lookup, so there is
     * nothing to sample. Say so rather than letting check 2 pass empty. */
    printf("  NOTE: the system store lists no certificates (hashed dir), so\n"
           "        the self-signed discriminator could not be sampled.\n");
    return;
  }
  printf("  discriminator on %d system roots: %d read as self-signed\n",
         roots, self);
  assert(self == roots);
}

int main(int argc, char **argv) {
  test_path_resolution();

  X509_STORE *st = system_store();
  if (!st) {
    printf("  NOTE: this build has no usable system root store, so the\n"
           "        chain-to-a-public-root check (3) cannot be made. The\n"
           "        self-signed and expiry checks still run.\n");
  }

  /* The bundle under test: an argument when given (see the header), else the
   * one this tree ships, found the way the engine finds it. */
  if (argc > 1) setenv("JO_CA_EXTRA", argv[1], 1);
  else setenv("JO_REPO_ROOT", JO_REPO_ROOT, 1);
  const char *path = catrust_extra_path();

  int n = check_bundle(path, st);
  if (n < 0) return 1;

  /* 6 — the engine's parser sees the same certificates. */
  int engine = catrust_count();
  if (engine != n) {
    fprintf(stderr, "FAILED: catrust_count()=%d, this test read %d\n", engine, n);
    return 1;
  }
  printf("  catrust_count() agrees: %d\n", engine);

  if (st) { test_discriminator_sees_a_root(st); X509_STORE_free(st); }
  printf("test_catrust: OK\n");
  return 0;
}
