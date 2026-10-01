/* test_resolve_near.c — the name-miss resolver in core/osint_dispatch.c.
 *
 * Before it existed, a service name the registry did not have produced
 * `not_implemented` and nothing else: a model that wrote DOMAIN_WHOIS_LOOKUP,
 * or a client that wrote domain-whois, got silence for a service that is
 * registered and working. The resolver closes that, under two rules that this
 * test is here to hold:
 *
 *   - it resolves only when there is ONE obvious candidate, and
 *   - a resolution is never silent (osint_result.resolved_from; the pipeline
 *     puts it in the per-service result next to what actually ran).
 *
 * The interesting assertions are the refusals. A resolver that always answers
 * is worse than none: it turns "we do not have that" into a confident answer
 * about a different service.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "source.h"
#include "core/osint_dispatch.h"

static int resolves_to(const char *asked, const char *want) {
  char canon[64], out[128];
  if (!osint_canon(asked, canon, sizeof canon)) return 0;
  if (registry_get(canon)) return 0;          /* not a miss at all */
  if (!osint_resolve_near(canon, out, sizeof out)) return 0;
  return !strcmp(out, want);
}

static int refuses(const char *asked) {
  char canon[64], out[128];
  if (!osint_canon(asked, canon, sizeof canon)) return 1;
  if (registry_get(canon)) return 0;          /* it exists; not a refusal case */
  return osint_resolve_near(canon, out, sizeof out) == 0;
}

int main(void) {
  assert(registry_count() > 0 && "no sources registered — link line is wrong");

  /* The registered spellings this test is anchored on really are registered;
   * if one is ever retired the assertions below would pass for the wrong
   * reason, so check first. */
  assert(registry_get("DOMAIN_WHOIS"));
  assert(registry_get("DNS_RECORDS"));
  assert(registry_get("IP_GEOLOCATION"));

  /* 1. Spelling: separators and a decorative suffix are not a different name. */
  assert(resolves_to("domain-whois", "DOMAIN_WHOIS"));
  assert(resolves_to("DOMAIN_WHOIS_LOOKUP", "DOMAIN_WHOIS"));
  assert(resolves_to("domain whois", "DOMAIN_WHOIS"));
  assert(resolves_to("dns-records", "DNS_RECORDS"));
  assert(resolves_to("IP_GEOLOCATION_API", "IP_GEOLOCATION"));
  printf("  separators and _LOOKUP/_API suffixes resolve: ok\n");

  /* 2. A typo within edit distance 2, when nothing else is that close. */
  assert(resolves_to("DOMAIN_WHOIZ", "DOMAIN_WHOIS"));
  assert(resolves_to("IP_GEOLOCATON", "IP_GEOLOCATION"));
  printf("  single-character typos resolve: ok\n");

  /* 3. REFUSALS — the half that matters.
   *    Nothing near: answering would be invention. */
  assert(refuses("ASK_THE_ORACLE"));
  assert(refuses("PLEASE_FIND_EVERYTHING_ABOUT_THIS_PERSON"));
  assert(refuses(""));
  printf("  a name with no near match is refused: ok\n");

  /* A name equidistant from two registered ids is a coin flip, and a coin
   * flip presented as a result is worse than not_implemented. The registry
   * carries families that differ by one or two characters (the OPENPAY_
   * OWNERSHIP_20NN series is one), so this is a real shape, not a contrived
   * one: a name 1 edit from two of them must resolve to neither. */
  {
    int years = 0;
    char id[64];
    for (int y = 2019; y <= 2025; y++) {
      snprintf(id, sizeof id, "OPENPAY_OWNERSHIP_%d", y);
      if (registry_get(id)) years++;
    }
    if (years >= 2) {
      /* "OPENPAY_OWNERSHIP_202" is 1 edit from every _202N member. */
      assert(refuses("OPENPAY_OWNERSHIP_202"));
      printf("  a name equally near %d siblings is refused: ok\n", years);
    } else {
      printf("  (skipped the tie case: the OPENPAY family is gone)\n");
    }
  }

  /* 4. A registered name is never "resolved" — it is just found. */
  {
    char out[128];
    assert(osint_resolve_near("DOMAIN_WHOIS", out, sizeof out) == 0 ||
           !strcmp(out, "DOMAIN_WHOIS"));
  }
  printf("test_resolve_near: ok\n");
  return 0;
}
