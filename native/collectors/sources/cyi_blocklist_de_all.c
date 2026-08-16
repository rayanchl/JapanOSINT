/* collectors/sources/cyi_blocklist_de_all.c
 * blocklist.de — IPs reported by the fail2ban reporting network for attacking
 * SSH/mail/web in the last 48 hours.
 * Endpoint: https://lists.blocklist.de/lists/all.txt                 (keyless)
 * parse_notes: "Bare IPv4 per line, no comments. Best stored as a lookup set
 * that enriches other rows rather than 22k standalone intel rows." The feed is
 * therefore parsed in full — the true total is counted and carried on every row
 * as feed_total — while at most MAX_ROWS entries are materialised as intel rows
 * so one hourly run stays bounded. Every emitted address is a literal line
 * from the feed; nothing is synthesised. No coordinates -> has_geo 0 (R2).
 * Licence: free community feed, no key; attribution to blocklist.de.
 */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://lists.blocklist.de/lists/all.txt"
#define MAX_ROWS 5000

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, CYI_URL, 40000);
  if (!body) { fprintf(stderr, "[blocklist-de-all] fetch failed\n"); return -1; }

  /* pass 1: true feed size (carried onto each row) */
  int total = 0;
  for (const char *q = body; *q; q++) if (*q == '\n') total++;

  int n = 0;
  char *cur = body, *line;
  while ((line = jo_next_line(&cur)) != NULL && n < MAX_ROWS) {
    if (!line[0] || line[0] == '#') continue;
    if (!jo_is_ipv4(line)) continue;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "ip", line);
    cJSON_AddStringToObject(p, "feed", "blocklist.de");
    cJSON_AddStringToObject(p, "activity", "attack-reported");
    cJSON_AddStringToObject(p, "observation_window", "48h");
    cJSON_AddNumberToObject(p, "feed_total", total);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    intel_item it = {0};
    it.remote_key      = line;
    it.title           = line;
    it.summary         = "Reported to blocklist.de for attacking SSH/mail/web in the last 48h";
    it.link            = CYI_URL;
    it.lang            = "en";
    it.record_type     = "malicious-ip";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"blocklist\",\"attacker-ip\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[blocklist-de-all] emitted %d of ~%d listed IPs\n", n, total);
  return 0;
}

static const source_def cyi_blocklist_de_all_def = {
  .id = "blocklist-de-all", .collector = "cyber",
  .name = "blocklist.de attacker IPs (all services)",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "IPs reported by the blocklist.de fail2ban network for attacking SSH/mail/web in the last 48h — a large, fast-moving, keyless attacker set.",
  .license = "Free community feed, no key; attribution to blocklist.de.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_blocklist_de_all_def)
