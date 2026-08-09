/* collectors/sources/cyi_cins_army_badguys.c
 * CINS Army list (Sentinel IPS) — the CINS score's worst 15,000 IPs, derived
 * from Sentinel IPS sensor telemetry. Independent of blocklist.de/DShield, so
 * an overlap between them is a genuine confidence signal.
 * Endpoint: https://cinsscore.com/list/ci-badguys.txt                (keyless)
 * parse_notes: "Fixed 15,000 lines, bare IPv4, no comments." The full list is
 * counted and the real total carried on every row; at most MAX_ROWS entries are
 * materialised so one hourly run stays bounded (the feed's intended use is a
 * lookup set). Every emitted address is a literal line from the feed.
 * No coordinates -> has_geo 0 (R2).
 * Licence: free feed from Sentinel IPS/CINS; attribution requested.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://cinsscore.com/list/ci-badguys.txt"
#define MAX_ROWS 5000

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, CYI_URL, 40000);
  if (!body) { fprintf(stderr, "[cins-army-badguys] fetch failed\n"); return -1; }

  int total = 0;
  for (const char *q = body; *q; q++) if (*q == '\n') total++;

  int n = 0;
  char *cur = body, *line;
  while ((line = jo_next_line(&cur)) != NULL && n < MAX_ROWS) {
    if (!line[0] || line[0] == '#') continue;
    if (!jo_is_ipv4(line)) continue;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "ip", line);
    cJSON_AddStringToObject(p, "feed", "cins-army");
    cJSON_AddStringToObject(p, "sensor_network", "Sentinel IPS");
    cJSON_AddNumberToObject(p, "feed_total", total);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    intel_item it = {0};
    it.remote_key      = line;
    it.title           = line;
    it.summary         = "On the CINS Army list (Sentinel IPS sensor telemetry)";
    it.link            = CYI_URL;
    it.lang            = "en";
    it.record_type     = "malicious-ip";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"blocklist\",\"attacker-ip\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[cins-army-badguys] emitted %d of ~%d listed IPs\n", n, total);
  return 0;
}

static const source_def cyi_cins_army_badguys_def = {
  .id = "cins-army-badguys", .collector = "cyber",
  .name = "CINS Army list (Sentinel IPS)",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "The CINS score's worst IPs from the Sentinel IPS sensor network — an independent sensor set, so overlap with other feeds is a real confidence signal.",
  .license = "Free feed from Sentinel IPS/CINS; attribution requested.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_cins_army_badguys_def)
