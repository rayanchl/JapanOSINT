/* test_host_agents.c — core/host_agents.def, the one per-host User-Agent table.
 *
 * Until 2026-10-21 there were three mechanisms (two tables in httpclient.c and
 * 471 per-row headers) with three precedences. Holds: each kind resolves to
 * its agent, a lookalike host inherits nothing, :port and case do not matter,
 * the browser profile is reported for HA_BROWSER only, the squatted domain
 * dropped in the move stays out, and the dump has one line per entry. */
#include "core/httpclient.h"
#include "core/keysapi.h"
#include "core/tenantapi.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void) {
  const char *ua;

  ua = http_ua_override("https://servicios.ine.es/wstempus/js/ES/CLASIFICACIONES");
  assert(ua && strstr(ua, "JapanOSINT/1.0") && !strstr(ua, "collector"));
  ua = http_ua_override("https://pib.gov.in/RssMain.aspx?ModId=6");
  assert(ua && !strcmp(ua, "JapanOSINT/1.0 (contact via repo issues)"));
  ua = http_ua_override("https://data.humdata.org/api/3/action/package_search");
  assert(ua && !strncmp(ua, "RCorp-feeds/1.0", 15));
  ua = http_ua_override("https://www.bls.gov/feed/cpi.rss");
  assert(ua && strstr(ua, "Chrome/"));
  assert(http_browser_host("https://www.bls.gov/feed/cpi.rss"));
  assert(!http_browser_host("https://pib.gov.in/x"));
  printf("  each kind resolves to its agent; only HA_BROWSER is a browser host: ok\n");

  assert(http_ua_override("https://data.humdata.org.evil.example/") == NULL);
  assert(http_ua_override("https://example.org/data.humdata.org/x") == NULL);
  assert(http_ua_override("https://DATA.HUMDATA.ORG:443/x") != NULL);
  assert(http_ua_override("https://www.example.org/") == NULL);
  printf("  exact host match: lookalikes and paths inherit nothing; case and :port ignored: ok\n");

  assert(http_ua_override("https://edinburgnd.com/feed") != NULL);
  assert(http_ua_override("https://midjourneyvideo.net/feed/") == NULL);
  printf("  the squatted midjourneyvideo.net stays out of the table: ok\n");

  /* SEC refuses a client that declares no contact mailbox, so its agent is
   * built from a CREDENTIAL (SEC_CONTACT, resolved like an API key) and, with
   * none configured, no request is sent at all (a 403 is certain). */
  const char *sec = "https://data.sec.gov/api/xbrl/frames/us-gaap/Assets/USD/CY2024Q1I.json";
  const char *sec2 = "https://www.sec.gov/files/company_tickers.json";
  const char *sec3 = "https://efts.sec.gov/LATEST/search-index?q=x";
  unsetenv("SEC_CONTACT_UA");
  unsetenv("SEC_CONTACT");

  /* unset: resolves to "needs SEC_CONTACT", the override is NULL, and
   * http_request refuses WITHOUT network and says why. */
  assert(http_host_needs_credential(sec) && !strcmp(http_host_needs_credential(sec), "SEC_CONTACT"));
  assert(http_host_needs_credential(sec2) && http_host_needs_credential(sec3));
  assert(http_ua_override(sec) == NULL);
  assert(http_host_needs_credential("https://pib.gov.in/RssMain.aspx") == NULL);
  assert(http_host_needs_credential("https://www.example.org/") == NULL);
  {
    http_client *hc = http_client_new();
    http_response r;
    http_credential_refusal_reset();
    assert(http_credential_refusal() == NULL);
    int rc = http_request(hc, "GET", sec, NULL, NULL, 0, 2000, 0, &r);
    assert(rc != 0 && r.status == 0 && r.body == NULL);
    assert(http_credential_refusal() && !strcmp(http_credential_refusal(), "SEC_CONTACT"));
    assert(http_client_request_count(hc) == 0 && http_client_host_count(hc) == 0);
    http_credential_refusal_reset();
    assert(http_credential_refusal() == NULL);
    http_client_free(hc);
  }
  printf("  SEC_CONTACT unset: SEC hosts need it, and a request is refused with no network: ok\n");

  /* set via the environment: bare address -> the research agent; a full agent
   * string is used as written; other hosts are untouched. */
  setenv("SEC_CONTACT", "ops@acme.example", 1);
  assert(http_host_needs_credential(sec) == NULL);
  ua = http_ua_override(sec);
  assert(ua && !strcmp(ua, "JapanOSINT/1.0 research (ops@acme.example)"));
  assert(http_ua_override(sec2) && http_ua_override(sec3));
  assert(!http_browser_host(sec2));
  setenv("SEC_CONTACT", "  ops@acme.example  ", 1);              /* trimmed */
  ua = http_ua_override(sec);
  assert(ua && !strcmp(ua, "JapanOSINT/1.0 research (ops@acme.example)"));
  setenv("SEC_CONTACT", "AcmeResearch/2.0 (ops@acme.example)", 1);
  ua = http_ua_override(sec);
  assert(ua && !strcmp(ua, "AcmeResearch/2.0 (ops@acme.example)"));
  ua = http_ua_override("https://pib.gov.in/RssMain.aspx?ModId=6");
  assert(ua && !strcmp(ua, "JapanOSINT/1.0 (contact via repo issues)"));
  printf("  SEC_CONTACT from the environment builds the agent; other hosts unchanged: ok\n");

  /* $SEC_CONTACT_UA, the original full-string override, still wins. */
  setenv("SEC_CONTACT", "ops@acme.example", 1);
  setenv("SEC_CONTACT_UA", "FullString/3.0 (legal@acme.example)", 1);
  ua = http_ua_override(sec);
  assert(ua && !strcmp(ua, "FullString/3.0 (legal@acme.example)"));
  unsetenv("SEC_CONTACT_UA");
  unsetenv("SEC_CONTACT");
  /* ...and alone it is enough, as before the credential existed. */
  setenv("SEC_CONTACT_UA", "AcmeResearch/2.0 (ops@acme.example)", 1);
  ua = http_ua_override(sec2);
  assert(ua && !strcmp(ua, "AcmeResearch/2.0 (ops@acme.example)"));
  unsetenv("SEC_CONTACT_UA");
  printf("  $SEC_CONTACT_UA still replaces the whole string: ok\n");

  /* validation: an address SEC accepts, no CR/LF (header injection). A bad
   * value in the environment counts as unset and is never sent. */
  const char *why = NULL;
  assert(http_contact_check("ops@acme.example", &why) == 0);
  assert(http_contact_check("Acme/2.0 (ops@acme.example)", &why) == 0);
  assert(http_contact_check("no-address-here", &why) != 0 && why && strstr(why, "e-mail"));
  assert(http_contact_check("@acme.example", &why) != 0);
  assert(http_contact_check("ops@localhost", &why) != 0);
  assert(http_contact_check("ops@acme.example\r\nX-Evil: 1", &why) != 0 && strstr(why, "control"));
  assert(http_contact_check("ops@acme.example\nX", &why) != 0);
  assert(http_contact_check("", &why) != 0);
  setenv("SEC_CONTACT", "ops@acme.example\r\nX-Evil: 1", 1);
  assert(http_host_needs_credential(sec) != NULL && http_ua_override(sec) == NULL);
  setenv("SEC_CONTACT", "not an address", 1);
  assert(http_host_needs_credential(sec) != NULL);
  unsetenv("SEC_CONTACT");
  printf("  contact validation: address required, CR/LF refused, bad env value = unset: ok\n");

  /* set via the OVERLAY, the way the Keys screen does: PUT /api/keys/SEC_CONTACT
   * validates, writes the overlay and the process environment; a bad value is
   * a 400 and changes nothing; clearing unsets it again. */
  {
    char tmpl[] = "/tmp/jo_apikeys_XXXXXX";
    int fd = mkstemp(tmpl); assert(fd >= 0); close(fd);
    unlink(tmpl);
    setenv("JO_API_KEYS_FILE", tmpl, 1);
    tenant_ctx t; memset(&t, 0, sizeof t);
    snprintf(t.role, sizeof t.role, "admin");
    int st = 0; char *o;

    o = keysapi_platform(NULL, &t, "PUT", "SEC_CONTACT", "{\"value\":\"ops@acme.example\\r\\nX: y\"}", &st);
    assert(st == 400 && strstr(o, "SEC_CONTACT") && strstr(o, "control")); free(o);
    o = keysapi_platform(NULL, &t, "PUT", "SEC_CONTACT", "{\"value\":\"nobody\"}", &st);
    assert(st == 400 && strstr(o, "e-mail")); free(o);
    assert(http_host_needs_credential(sec) != NULL);          /* nothing changed */

    o = keysapi_platform(NULL, &t, "GET", "", "", &st);
    assert(st == 200 && strstr(o, "\"SEC_CONTACT\"")); free(o);  /* listed in /api/keys */

    o = keysapi_platform(NULL, &t, "PUT", "SEC_CONTACT", "{\"value\":\"keys@acme.example\"}", &st);
    assert(st == 200 && strstr(o, "\"set\":true") && strstr(o, "\"hasOverlay\":true")); free(o);
    ua = http_ua_override(sec);
    assert(ua && !strcmp(ua, "JapanOSINT/1.0 research (keys@acme.example)"));

    /* a fresh process reads the overlay file at boot */
    unsetenv("SEC_CONTACT");
    assert(http_host_needs_credential(sec) != NULL);
    keysapi_apply_overlay_env();
    ua = http_ua_override(sec);
    assert(ua && !strcmp(ua, "JapanOSINT/1.0 research (keys@acme.example)"));

    o = keysapi_platform(NULL, &t, "PUT", "SEC_CONTACT", "{\"value\":\"\"}", &st);
    assert(st == 200 && strstr(o, "\"set\":false")); free(o);
    assert(http_host_needs_credential(sec) != NULL);
    unlink(tmpl);
    unsetenv("JO_API_KEYS_FILE");
  }
  printf("  SEC_CONTACT through the Keys API: validated, listed, applied, clearable: ok\n");

  char *buf = NULL; size_t len = 0;
  FILE *f = open_memstream(&buf, &len);
  http_host_agents_dump(f);
  fclose(f);
  int lines = 0, compat = 0;
  for (char *p = buf; *p; p++) if (*p == '\n') lines++;
  for (char *p = buf; (p = strstr(p, "\tHA_COMPAT\t")); p++) compat++;
  assert(lines > 300 && compat >= 300);
  /* unset SEC_CONTACT shows as (unset: ...) and never as a string */
  assert(strstr(buf, "data.sec.gov\tHA_SEC\t(unset: SEC_CONTACT)\t"));
  assert(!strstr(buf, "%CONTACT%"));
  assert(strstr(buf, "www.bls.gov\tHA_BROWSER\t") && strstr(buf, "Sec-Fetch-Mode: navigate"));
  printf("  dump: %d entries, %d HA_COMPAT, browser extras listed: ok\n", lines - 1, compat);
  free(buf);
  printf("test_host_agents: ok\n");
  return 0;
}
