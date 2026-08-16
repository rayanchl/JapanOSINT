/* collectors/sources/cyi_phishing_army_blocklist.c
 * Phishing Army blocklist (extended) — large aggregated phishing-domain set
 * built from Phishtank/OpenPhish/PhishFindR and others, deduplicated.
 * Endpoint: https://phishing.army/download/phishing_army_blocklist.txt (keyless)
 * parse_notes: "Bare domains after a '#' header block. 154k entries: index it,
 * do not emit rows one-per-domain." The full file is therefore parsed and
 * counted — the real total and the '# Last Update' header are carried on every
 * row — but only the first MAX_ROWS domains are materialised as intel rows, so
 * this stays the breadth layer behind the smaller high-precision feeds rather
 * than a 154k-row dump. Every emitted domain is a literal line from the file.
 * No coordinates -> has_geo 0 (R2).
 * Licence: CC BY-NC-SA style community feed — NON-COMMERCIAL; check the
 * project page terms before commercial redistribution.
 */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://phishing.army/download/phishing_army_blocklist.txt"
#define MAX_ROWS 5000

static int looks_like_host(const char *s) {
  size_t n = strlen(s);
  if (n < 4 || n > 253) return 0;
  if (!strchr(s, '.')) return 0;
  for (const char *q = s; *q; q++)
    if (!(isalnum((unsigned char)*q) || *q == '-' || *q == '.' ||
          (unsigned char)*q >= 0x80)) return 0;
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, CYI_URL, 60000);
  if (!body) { fprintf(stderr, "[phishing-army-blocklist] fetch failed\n"); return -1; }

  int total = 0;
  for (const char *q = body; *q; q++) if (*q == '\n') total++;

  char updated[128] = "";
  int n = 0;
  char *cur = body, *line;
  while ((line = jo_next_line(&cur)) != NULL) {
    if (!line[0]) continue;
    if (line[0] == '#') {
      const char *u = strstr(line, "Last Update:");
      if (u && !updated[0]) {
        u += 12;
        while (*u == ' ') u++;
        snprintf(updated, sizeof updated, "%s", u);
      }
      continue;
    }
    if (!looks_like_host(line)) continue;
    if (n >= MAX_ROWS) break;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "domain", line);
    cJSON_AddStringToObject(p, "feed", "phishing.army-extended");
    cJSON_AddNumberToObject(p, "feed_total", total);
    if (updated[0]) cJSON_AddStringToObject(p, "feed_updated", updated);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    intel_item it = {0};
    it.remote_key      = line;
    it.title           = line;
    it.summary         = "Listed on the Phishing Army extended blocklist";
    it.link            = CYI_URL;
    it.lang            = "en";
    it.record_type     = "phishing-domain";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"phishing\",\"blocklist\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[phishing-army-blocklist] emitted %d of ~%d domains (updated %s)\n",
          n, total, updated[0] ? updated : "n/a");
  return 0;
}

static const source_def cyi_phishing_army_blocklist_def = {
  .id = "phishing-army-blocklist", .collector = "cyber",
  .name = "Phishing Army blocklist (extended)",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "Large aggregated phishing-domain set from Phishtank/OpenPhish/PhishFindR and others — the breadth layer behind the high-precision phishing feeds.",
  .license = "CC BY-NC-SA style community feed — NON-COMMERCIAL; check phishing.army terms before commercial redistribution.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_phishing_army_blocklist_def)
