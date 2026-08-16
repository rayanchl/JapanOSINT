/* collectors/sources/cyi_threatview_ip_feed.c
 * ThreatView high-confidence C2 / malicious-infrastructure IP feed.
 * Endpoint: https://threatview.io/Downloads/IP-High-Confidence-Feed.txt
 * Keyless.
 * TRAP (parse_notes): "Contains a few obviously bogus entries (0.0.0.2, 0.1.0.0
 * seen in the head) — drop non-routable/bogon addresses before emitting." The
 * bogon filter below does exactly that (0/8, 10/8, 100.64/10, 127/8,
 * 169.254/16, 172.16/12, 192.168/16, 224/4 and above).
 * Emits one row per surviving literal address; the feed total and the number
 * dropped are logged. No coordinates -> has_geo 0 (R2).
 * Licence: threatview.io free feeds, attribution requested; confirm terms
 * before commercial redistribution.
 */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://threatview.io/Downloads/IP-High-Confidence-Feed.txt"
#define MAX_ROWS 5000

/* parse dotted quad; 0 on failure */
static int parse_v4(const char *s, unsigned oct[4]) {
  int parts = 0;
  while (*s && parts < 4) {
    unsigned v = 0; int d = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (unsigned)(*s - '0'); s++; d++; if (v > 255) return 0; }
    if (!d || d > 3) return 0;
    oct[parts++] = v;
    if (*s == '.') { s++; if (!*s) return 0; }
    else if (*s) return 0;
  }
  return parts == 4 && !*s;
}

static int is_bogon(const unsigned o[4]) {
  if (o[0] == 0 || o[0] == 10 || o[0] == 127 || o[0] >= 224) return 1;
  if (o[0] == 169 && o[1] == 254) return 1;
  if (o[0] == 172 && o[1] >= 16 && o[1] <= 31) return 1;
  if (o[0] == 192 && o[1] == 168) return 1;
  if (o[0] == 100 && o[1] >= 64 && o[1] <= 127) return 1;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, CYI_URL, 40000);
  if (!body) { fprintf(stderr, "[threatview-ip-feed] fetch failed\n"); return -1; }

  int n = 0, dropped = 0, seen = 0;
  char *cur = body, *line;
  while ((line = jo_next_line(&cur)) != NULL) {
    if (!line[0] || line[0] == '#') continue;
    unsigned o[4];
    if (!parse_v4(line, o)) continue;
    seen++;
    if (is_bogon(o)) { dropped++; continue; }
    if (n >= MAX_ROWS) continue;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "ip", line);
    cJSON_AddStringToObject(p, "feed", "threatview-high-confidence");
    cJSON_AddStringToObject(p, "classification", "c2-or-malicious-infrastructure");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    intel_item it = {0};
    it.remote_key      = line;
    it.title           = line;
    it.summary         = "ThreatView high-confidence C2 / malicious infrastructure";
    it.link            = CYI_URL;
    it.lang            = "en";
    it.record_type     = "malicious-ip";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"c2\",\"blocklist\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[threatview-ip-feed] emitted %d of %d (%d bogons dropped)\n",
          n, seen, dropped);
  return 0;
}

static const source_def cyi_threatview_ip_feed_def = {
  .id = "threatview-ip-feed", .collector = "cyber",
  .name = "ThreatView high-confidence C2/malicious IP feed",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "ThreatView's C2 and malicious-infrastructure IP set, derived from malware infrastructure tracking rather than login abuse.",
  .license = "threatview.io free feeds; attribution requested, confirm terms before commercial redistribution.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_threatview_ip_feed_def)
