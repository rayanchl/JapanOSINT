/* test_host_agents.c — core/host_agents.def, the one per-host User-Agent table.
 *
 * Until 2026-10-21 there were three mechanisms (two tables in httpclient.c and
 * 471 per-row headers) with three precedences. Holds: each kind resolves to
 * its agent, a lookalike host inherits nothing, :port and case do not matter,
 * the browser profile is reported for HA_BROWSER only, the squatted domain
 * dropped in the move stays out, and the dump has one line per entry. */
#include "core/httpclient.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

  char *buf = NULL; size_t len = 0;
  FILE *f = open_memstream(&buf, &len);
  http_host_agents_dump(f);
  fclose(f);
  int lines = 0, compat = 0;
  for (char *p = buf; *p; p++) if (*p == '\n') lines++;
  for (char *p = buf; (p = strstr(p, "\tHA_COMPAT\t")); p++) compat++;
  assert(lines > 300 && compat >= 300);
  assert(strstr(buf, "www.bls.gov\tHA_BROWSER\t") && strstr(buf, "Sec-Fetch-Mode: navigate"));
  printf("  dump: %d entries, %d HA_COMPAT, browser extras listed: ok\n", lines - 1, compat);
  free(buf);
  printf("test_host_agents: ok\n");
  return 0;
}
