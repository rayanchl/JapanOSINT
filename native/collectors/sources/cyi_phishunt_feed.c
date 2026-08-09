/* collectors/sources/cyi_phishunt_feed.c
 * Phishunt live phishing URL feed — currently-live phishing URLs with full
 * paths, independent of OpenPhish/PhishTank.
 * Endpoint: https://phishunt.io/feed.txt                              (keyless)
 * parse_notes: "One URL per line, no header. Full URL including path — keep the
 * path, it carries the kit identifier." The path is preserved verbatim; the
 * host is additionally split out so the impersonated brand is queryable.
 * No coordinates -> has_geo 0 (R2).
 * Licence: phishunt.io free feed; attribution requested.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://phishunt.io/feed.txt"

/* scheme://[user@]host[:port]/… -> host. Returns out or NULL. */
static const char *url_host(const char *u, char *out, size_t n) {
  const char *p = strstr(u, "://");
  if (!p) return NULL;
  p += 3;
  const char *at = NULL;
  for (const char *q = p; *q && *q != '/' && *q != '?' && *q != '#'; q++)
    if (*q == '@') at = q;
  if (at) p = at + 1;
  size_t o = 0;
  while (*p && *p != '/' && *p != '?' && *p != '#' && *p != ':' && o + 1 < n)
    out[o++] = *p++;
  if (o == 0) return NULL;
  out[o] = '\0';
  return out;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, CYI_URL, 25000);
  if (!body) { fprintf(stderr, "[phishunt-feed] fetch failed\n"); return -1; }

  int n = 0;
  char *cur = body, *line;
  while ((line = jo_next_line(&cur)) != NULL) {
    if (!line[0] || line[0] == '#') continue;
    if (strncmp(line, "http", 4) != 0) continue;

    char host[300];
    const char *h = url_host(line, host, sizeof host);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "url", line);
    if (h) cJSON_AddStringToObject(p, "host", h);
    cJSON_AddStringToObject(p, "feed", "phishunt");
    cJSON_AddStringToObject(p, "status", "live");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    intel_item it = {0};
    it.remote_key      = line;
    it.title           = h ? h : line;
    it.summary         = "Live phishing URL (Phishunt)";
    it.body            = line;               /* full URL incl. kit path */
    it.link            = line;
    it.lang            = "en";
    it.record_type     = "phishing-url";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"phishing\",\"phishunt\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[phishunt-feed] emitted %d\n", n);
  return 0;
}

static const source_def cyi_phishunt_feed_def = {
  .id = "phishunt-feed", .collector = "cyber",
  .name = "Phishunt live phishing URL feed",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "Currently-live phishing URLs with full paths, independent of OpenPhish/PhishTank; the path carries the phishing-kit identifier.",
  .license = "phishunt.io free feed; attribution requested.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_phishunt_feed_def)
