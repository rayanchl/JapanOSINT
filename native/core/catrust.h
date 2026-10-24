/* catrust — the intermediate CA certificates our upstreams forget to send.
 *
 * A server is supposed to present its whole chain bar the root. Nine hosts
 * carrying 24 probed source candidates present only their leaf, so both
 * OpenSSL and libcurl answer "unable to get local issuer certificate" and the
 * row was written off as a dead endpoint. The missing link is public: the
 * leaf's own AIA "CA Issuers" extension names it at the issuing CA's URL.
 *
 * This loads those fetched intermediates into the verification store ON TOP OF
 * the system roots. It cannot widen trust, and that is a structural property
 * rather than a promise: no certificate here is self-signed, and OpenSSL
 * accepts a chain only when it terminates in a self-signed certificate that
 * was ALREADY trusted (X509_V_FLAG_PARTIAL_CHAIN is not set). So every chain
 * these certificates complete still has to be vouched for by a root the system
 * trusted before this file existed. `tests/unit/test_catrust.c` fails the build
 * if any certificate in the bundle is self-signed, expired, or cannot be
 * chained to the system store.
 *
 * Verification itself is never touched: CURLOPT_SSL_VERIFYPEER and
 * CURLOPT_SSL_VERIFYHOST keep libcurl's defaults, and this file contains no
 * code that could change them.
 */
#ifndef JO_CATRUST_H
#define JO_CATRUST_H

#include <stddef.h>

/* The bundle path: $JO_CA_EXTRA, else <repo root>/native/data/tls/extra-intermediates.pem
 * ($JO_REPO_ROOT when set, else the root the binary was built from). Never a
 * path written into the source — the same rule as the breach corpus root. */
const char *catrust_extra_path(void);

/* Install the bundle on one libcurl easy handle. Returns the number of
 * certificates the store will receive, 0 when there is no bundle to install,
 * and -1 when this libcurl cannot take an SSL_CTX callback (a non-OpenSSL
 * backend), which is not an error: the handle then behaves exactly as it did
 * before this file existed. */
int catrust_install(void *curl_easy);

/* How many certificates the bundle parsed to, for /api/status and the tests.
 * Parses on first use; -1 when the bundle exists but could not be read. */
int catrust_count(void);

#endif
