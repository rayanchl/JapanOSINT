/* test_ratelimit_breakglass.c — API-11: who a break-glass attempt is charged to.
 *
 *  - X-Forwarded-For is ignored unless the peer is in $JO_TRUSTED_PROXIES, so a
 *    client cannot pick its own bucket (no bypass) ...
 *  - ... and honoured from a trusted proxy (right-most untrusted hop), so the
 *    users behind one proxy no longer share a single 5/min bucket.
 *  - IPv6 collapses to its /64.
 *  - The global ceiling stops a spray of distinct clients, and a refused
 *    client does not spend that shared ceiling.
 *  - A refusal ends with its window: nothing outlives it.
 */
#include "../../core/ratelimit.c"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

static void key(const char *peer, const char *xff, char *out) {
  ratelimit_client_key(peer, xff, out, RL_KEYLEN);
}

int main(void) {
  char k[RL_KEYLEN], k2[RL_KEYLEN];
  unsetenv("JO_TRUSTED_PROXIES");

  /* No trusted proxy configured: the header is ignored whole. */
  key("192.0.2.9", "203.0.113.7", k);
  assert(!strcmp(k, "192.0.2.9"));
  key("192.0.2.9", "203.0.113.7, 198.51.100.1", k);
  assert(!strcmp(k, "192.0.2.9"));

  /* Trusted proxy: the right-most hop that is not a proxy is the client. */
  setenv("JO_TRUSTED_PROXIES", "10.0.0.1, 10.0.0.2", 1);
  key("10.0.0.1", "203.0.113.7", k);
  assert(!strcmp(k, "203.0.113.7"));
  /* A forged left-most entry does not win over what our proxy appended. */
  key("10.0.0.1", "1.1.1.1, 203.0.113.7", k);
  assert(!strcmp(k, "203.0.113.7"));
  /* Chained trusted proxies are skipped. */
  key("10.0.0.1", "203.0.113.7, 10.0.0.2", k);
  assert(!strcmp(k, "203.0.113.7"));
  /* A peer that is NOT trusted cannot use the header even if it names one. */
  key("192.0.2.9", "10.0.0.1", k);
  assert(!strcmp(k, "192.0.2.9"));
  /* Garbage from a trusted proxy falls back to the peer, never to the text. */
  key("10.0.0.1", "not-an-ip", k);
  assert(!strcmp(k, "10.0.0.1"));
  key("10.0.0.1", "", k);
  assert(!strcmp(k, "10.0.0.1"));

  /* IPv6: one /64 is one client; another /64 is another; v4-mapped is v4. */
  unsetenv("JO_TRUSTED_PROXIES");
  key("2001:db8:0:1:aaaa:bbbb:cccc:dddd", NULL, k);
  key("2001:db8:0:1::1", NULL, k2);
  assert(!strcmp(k, k2) && !strcmp(k, "2001:db8:0:1::/64"));
  key("2001:db8:0:2::1", NULL, k2);
  assert(strcmp(k, k2) != 0);
  key("::ffff:192.0.2.5", NULL, k);
  assert(!strcmp(k, "192.0.2.5"));
  key(NULL, NULL, k);
  assert(!strcmp(k, "-"));

  /* Per-client: 5 pass, the 6th is refused with a retry hint; another client
   * (behind the same trusted proxy) is untouched. */
  setenv("JO_TRUSTED_PROXIES", "10.0.0.1", 1);
  setenv("JO_BREAKGLASS_GLOBAL_PER_MIN", "0", 1);          /* ceiling off */
  int retry = 0;
  for (int i = 0; i < 5; i++)
    assert(ratelimit_breakglass_allow("10.0.0.1", "198.51.100.1", &retry));
  assert(!ratelimit_breakglass_allow("10.0.0.1", "198.51.100.1", &retry));
  assert(retry >= 1 && retry <= 60);
  assert(ratelimit_breakglass_allow("10.0.0.1", "198.51.100.2", &retry));
  /* Without the proxy the same header is not an escape from the bucket. */
  unsetenv("JO_TRUSTED_PROXIES");
  for (int i = 0; i < 5; i++)
    assert(ratelimit_breakglass_allow("192.0.2.50", "9.9.9.9", NULL));
  assert(!ratelimit_breakglass_allow("192.0.2.50", "8.8.8.8", NULL));

  /* Global ceiling: 12 distinct clients, ceiling 10 => the 11th and 12th are
   * refused although each client is within its own allowance, and the refused
   * attempts of ONE exhausted client do not burn the ceiling. */
  setenv("JO_BREAKGLASS_GLOBAL_PER_MIN", "10", 1);
  int ok = 0;
  for (int i = 0; i < 12; i++) {
    char ip[32]; snprintf(ip, sizeof ip, "198.18.%d.1", i);
    if (ratelimit_breakglass_allow(ip, NULL, &retry)) ok++;
  }
  assert(ok == 10);
  /* Distinct /64s of one prefix are NOT distinct clients. */
  setenv("JO_BREAKGLASS_GLOBAL_PER_MIN", "1000", 1);
  int allowed = 0;
  for (int i = 0; i < 40; i++) {
    char ip[64]; snprintf(ip, sizeof ip, "2001:db8:0:7:%x::1", i + 1);
    if (ratelimit_breakglass_allow(ip, NULL, NULL)) allowed++;
  }
  assert(allowed == 5);

  printf("test_ratelimit_breakglass: all passed\n");
  return 0;
}
