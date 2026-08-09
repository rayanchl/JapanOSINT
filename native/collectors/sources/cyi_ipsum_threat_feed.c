/* collectors/sources/cyi_ipsum_threat_feed.c
 * IPsum — union of ~30 public blocklists where each IP carries the number of
 * lists that agree on it. That count is the whole value of the feed: it is a
 * ready-made confidence score.
 * Endpoint: https://raw.githubusercontent.com/stamparm/ipsum/master/ipsum.txt
 * Keyless.
 * parse_notes: "Tab-separated ip<TAB>count after a '#' header. Filter on
 * count>=3 to get a defensible high-confidence subset instead of 110k rows."
 * That filter is applied here, plus a hard row cap so one daily run is bounded;
 * the '# Last update' header line is parsed and carried onto every row.
 * No coordinates -> has_geo 0 (R2).
 * Licence: stamparm/ipsum — Unlicense / public domain repo.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://raw.githubusercontent.com/stamparm/ipsum/master/ipsum.txt"
#define MIN_LISTS 3
#define MAX_ROWS 5000

static char *next_line(char **p) {
  char *s = *p;
  if (!s || !*s) return NULL;
  char *e = strchr(s, '\n');
  if (e) { *e = '\0'; *p = e + 1; } else { *p = s + strlen(s); }
  size_t n = strlen(s);
  while (n && (s[n - 1] == '\r' || s[n - 1] == ' ')) s[--n] = '\0';
  return s;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, CYI_URL, 60000);
  if (!body) { fprintf(stderr, "[ipsum-threat-feed] fetch failed\n"); return -1; }

  char updated[160] = "";
  int n = 0, qualified = 0;
  char *cur = body, *line;
  while ((line = next_line(&cur)) != NULL) {
    if (!line[0]) continue;
    if (line[0] == '#') {
      if (!updated[0] && strstr(line, "update"))
        snprintf(updated, sizeof updated, "%s", line + 1);
      continue;
    }
    char *tab = strchr(line, '\t');
    if (!tab) continue;
    *tab = '\0';
    const char *ip = line;
    char *end = NULL;
    long lists = strtol(tab + 1, &end, 10);
    if (end == tab + 1) continue;               /* no numeric count -> skip */
    if (!jo_is_ipv4(ip)) continue;
    if (lists < MIN_LISTS) continue;
    qualified++;
    if (n >= MAX_ROWS) continue;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "ip", ip);
    cJSON_AddNumberToObject(p, "blacklist_hits", (double)lists);
    cJSON_AddStringToObject(p, "feed", "ipsum");
    if (updated[0]) cJSON_AddStringToObject(p, "feed_updated", updated);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char summary[128];
    snprintf(summary, sizeof summary, "listed on %ld public blocklists", lists);

    intel_item it = {0};
    it.remote_key      = ip;
    it.title           = ip;
    it.summary         = summary;
    it.link            = CYI_URL;
    it.lang            = "en";
    it.record_type     = "malicious-ip";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"blocklist\",\"aggregated\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[ipsum-threat-feed] emitted %d of %d IPs with >=%d list hits\n",
          n, qualified, MIN_LISTS);
  return 0;
}

static const source_def cyi_ipsum_threat_feed_def = {
  .id = "ipsum-threat-feed", .collector = "cyber",
  .name = "IPsum aggregated threat feed (with list-hit count)",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "Union of ~30 public blocklists where each IP carries how many lists agree on it — a ready-made confidence score a raw blocklist cannot give.",
  .license = "stamparm/ipsum — Unlicense / public domain.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_ipsum_threat_feed_def)
