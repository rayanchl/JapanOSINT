/* tests/unit/test_clientip.c — the client address behind a reverse proxy
 * (core/clientip.c; httpd.c's req_client_ip() is its only caller).
 *
 * WHAT IS BEING PROTECTED.
 *   1. With JO_TRUSTED_PROXIES unset, X-Forwarded-For is IGNORED: the peer is
 *      the client, as before. A server that believed the header from anyone
 *      would let every request pick its own rate-limit bucket and audit IP.
 *   2. From a peer that is NOT listed, the header is ignored too.
 *   3. From a listed proxy, the chain is read from the RIGHT: the first
 *      untrusted entry is the client, so an address the client wrote at the
 *      left end is never taken. Trusted hops (a CIDR, IPv6) are skipped.
 *   4. A malformed entry stops the walk at the last trusted hop — the old
 *      shared-bucket behaviour, never an address of the caller's choosing.
 *   5. Bad configuration entries are skipped, not widened. */

#include "../../core/clientip.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *R(const char *peer, const char *xff) {
  static char out[64];
  clientip_resolve(peer, xff, out, sizeof out);
  return out;
}

int main(void) {
  unsetenv("JO_TRUSTED_PROXIES");
  assert(clientip_reload() == 0);
  assert(!strcmp(R("127.0.0.1", "203.0.113.7"), "127.0.0.1"));
  printf("  unset: the header is ignored: ok\n");

  setenv("JO_TRUSTED_PROXIES", "127.0.0.1, 10.0.0.0/8,::1", 1);
  assert(clientip_reload() == 3);
  assert(!strcmp(R("198.51.100.4", "203.0.113.7"), "198.51.100.4"));
  printf("  an unlisted peer's header is ignored: ok\n");

  assert(!strcmp(R("127.0.0.1", "203.0.113.7"), "203.0.113.7"));
  assert(!strcmp(R("127.0.0.1", "6.6.6.6, 203.0.113.7"), "203.0.113.7")
         && "the client's own left-hand entry is not believed");
  assert(!strcmp(R("127.0.0.1", "6.6.6.6, 203.0.113.7, 10.2.3.4"), "203.0.113.7")
         && "a trusted inner hop is skipped");
  assert(!strcmp(R("::1", "2001:db8::5"), "2001:db8::5"));
  assert(!strcmp(R("[::1]", " 203.0.113.9 "), "203.0.113.9"));
  assert(!strcmp(R("::ffff:127.0.0.1", "203.0.113.8"), "203.0.113.8")
         && "an IPv4-mapped peer is the IPv4 proxy");
  assert(!strcmp(R("127.0.0.1", "10.0.0.9"), "10.0.0.9") && "all trusted: the leftmost");
  printf("  a listed proxy: the chain is read from the right: ok\n");

  assert(!strcmp(R("127.0.0.1", "6.6.6.6, not-an-ip"), "127.0.0.1"));
  assert(!strcmp(R("127.0.0.1", "6.6.6.6, garbage, 10.1.1.1"), "10.1.1.1"));
  assert(!strcmp(R("127.0.0.1", ""), "127.0.0.1"));
  assert(!strcmp(R("127.0.0.1", NULL), "127.0.0.1"));
  printf("  a malformed entry stops at the last trusted hop: ok\n");

  setenv("JO_TRUSTED_PROXIES", "nonsense,10.0.0.0/99,10.0.0.0/,0.0.0.0/0", 1);
  assert(clientip_reload() == 1 && "only the well-formed /0 is accepted");
  setenv("JO_TRUSTED_PROXIES", "nonsense,10.0.0.0/99", 1);
  assert(clientip_reload() == 0);
  assert(!strcmp(R("10.0.0.1", "203.0.113.7"), "10.0.0.1"));
  printf("  bad entries are skipped, never widened: ok\n");

  printf("test_clientip: all passed\n");
  return 0;
}
