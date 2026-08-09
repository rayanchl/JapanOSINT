/* collectors/sources/cyi_vpn_ip_ranges_x4bnet.c
 * X4BNet commercial-VPN egress ranges — the CIDR complement to the Mullvad and
 * VPN Gate lists, covering providers that publish no API of their own.
 * Endpoint: https://raw.githubusercontent.com/X4BNet/lists_vpn/main/output/
 *           vpn/ipv4.txt                                              (keyless)
 * parse_notes: "CIDR per line, prefix-containment matching needed." Rows carry
 * the literal CIDR plus the prefix length so a containment matcher downstream
 * has what it needs; a sibling datacenter/ipv4.txt list exists in the same repo
 * for hosting ranges (not wired here).
 * The list is counted in full and the true total carried on every row; at most
 * MAX_ROWS entries are materialised so one run stays bounded.
 * No coordinates -> has_geo 0 (R2).
 * Licence: X4BNet/lists_vpn, MIT-licensed repo.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://raw.githubusercontent.com/X4BNet/lists_vpn/main/output/vpn/ipv4.txt"
#define MAX_ROWS 5000

/* dotted quad with an optional /len; returns the prefix length or -1 */
static int v4_cidr_len(const char *s) {
  int parts = 0;
  const char *p = s;
  while (*p && *p != '/') {
    int v = 0, d = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; d++; if (v > 255) return -1; }
    if (!d || d > 3) return -1;
    parts++;
    if (*p == '.') { p++; if (!*p) return -1; }
    else if (*p && *p != '/') return -1;
  }
  if (parts != 4) return -1;
  if (*p != '/') return 32;
  p++;
  int len = 0, d = 0;
  while (*p >= '0' && *p <= '9') { len = len * 10 + (*p - '0'); p++; d++; }
  if (!d || *p || len > 32) return -1;
  return len;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, CYI_URL, 40000);
  if (!body) { fprintf(stderr, "[vpn-ip-ranges-x4bnet] fetch failed\n"); return -1; }

  int total = 0;
  for (const char *q = body; *q; q++) if (*q == '\n') total++;

  int n = 0;
  char *cur = body, *line;
  while ((line = jo_next_line(&cur)) != NULL && n < MAX_ROWS) {
    if (!line[0] || line[0] == '#') continue;
    int len = v4_cidr_len(line);
    if (len < 0) continue;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "prefix", line);
    cJSON_AddNumberToObject(p, "prefix_length", len);
    cJSON_AddStringToObject(p, "classification", "commercial-vpn-egress");
    cJSON_AddStringToObject(p, "feed", "X4BNet/lists_vpn");
    cJSON_AddNumberToObject(p, "feed_total", total);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    intel_item it = {0};
    it.remote_key      = line;
    it.title           = line;
    it.summary         = "Commercial VPN egress range (X4BNet)";
    it.link            = CYI_URL;
    it.lang            = "en";
    it.record_type     = "vpn-ip-range";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"vpn\",\"ip-range\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[vpn-ip-ranges-x4bnet] emitted %d of ~%d CIDRs\n", n, total);
  return 0;
}

static const source_def cyi_vpn_ip_ranges_x4bnet_def = {
  .id = "vpn-ip-ranges-x4bnet", .collector = "cyber",
  .name = "X4BNet VPN/datacentre IP ranges",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "Commercial-VPN egress ranges as CIDRs, covering providers that publish no API of their own — the answer to 'was this session behind a VPN'.",
  .license = "X4BNet/lists_vpn — MIT-licensed repo.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_vpn_ip_ranges_x4bnet_def)
