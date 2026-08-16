/* collectors/sources/cyi_et_compromised_ips.c
 * Emerging Threats (Proofpoint) compromised IPs — hosts confirmed compromised
 * and in use for attacks.
 * Endpoint: https://rules.emergingthreats.net/blockrules/compromised-ips.txt
 * Keyless.
 * parse_notes: "Bare IPv4, no header. At 586 entries this is one of the few IP
 * feeds where emitting a row per IP is reasonable" — so the whole list is
 * emitted, one row per literal address read from the feed.
 * No coordinates -> has_geo 0 (R2).
 * Licence: Emerging Threats open ruleset feed (BSD-style open rules); no key.
 */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://rules.emergingthreats.net/blockrules/compromised-ips.txt"

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, CYI_URL, 20000);
  if (!body) { fprintf(stderr, "[et-compromised-ips] fetch failed\n"); return -1; }

  int n = 0;
  char *cur = body, *line;
  while ((line = jo_next_line(&cur)) != NULL) {
    if (!line[0] || line[0] == '#') continue;
    if (!jo_is_ipv4(line)) continue;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "ip", line);
    cJSON_AddStringToObject(p, "feed", "emerging-threats-compromised");
    cJSON_AddStringToObject(p, "classification", "confirmed-compromised-host");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    intel_item it = {0};
    it.remote_key      = line;
    it.title           = line;
    it.summary         = "Emerging Threats: host confirmed compromised and used in attacks";
    it.link            = CYI_URL;
    it.lang            = "en";
    it.record_type     = "malicious-ip";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"blocklist\",\"compromised-host\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[et-compromised-ips] emitted %d\n", n);
  return 0;
}

static const source_def cyi_et_compromised_ips_def = {
  .id = "et-compromised-ips", .collector = "cyber",
  .name = "Emerging Threats compromised IPs",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "Proofpoint ET's small, high-signal list of hosts confirmed compromised and used in attacks.",
  .license = "Emerging Threats open ruleset feed (BSD-style open rules); no key.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_et_compromised_ips_def)
