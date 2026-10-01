/* collectors/sources/cyi_firehol_level1.c
 * FireHOL level1 — the curated union of the highest-confidence hostile
 * netblocks (bogons, spam nets, known malicious ranges).
 * Endpoint: https://raw.githubusercontent.com/firehol/blocklist-ipsets/master/
 *           firehol_level1.netset                                    (keyless)
 * parse_notes: "CIDR ranges, not single IPs — the matcher must do prefix
 * containment. Long '#' header carries the constituent source names; keep it
 * for provenance." The header block is parsed and its provenance line is
 * carried onto every row; rows themselves are the literal netset entries. A
 * bare address with no '/' is normalised to /32 only in a separate property so
 * the emitted `prefix` stays exactly what the feed published.
 * No coordinates -> has_geo 0 (R2).
 * Licence: firehol/blocklist-ipsets repo (GPLv3-style repo licence); the
 * constituent upstream feeds keep their own terms, recorded in the file header.
 */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://raw.githubusercontent.com/firehol/blocklist-ipsets/master/firehol_level1.netset"

/* dotted quad, optionally /len */
static int is_v4_cidr(const char *s) {
  int parts = 0;
  const char *p = s;
  while (*p && *p != '/') {
    int v = 0, d = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; d++; if (v > 255) return 0; }
    if (!d || d > 3) return 0;
    parts++;
    if (*p == '.') { p++; if (!*p) return 0; }
    else if (*p && *p != '/') return 0;
  }
  if (parts != 4) return 0;
  if (*p == '/') {
    p++;
    int len = 0, d = 0;
    while (*p >= '0' && *p <= '9') { len = len * 10 + (*p - '0'); p++; d++; }
    if (!d || *p || len > 32) return 0;
  }
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, CYI_URL, 30000);
  if (!body) { fprintf(stderr, "[firehol-level1] fetch failed\n"); return -1; }

  char provenance[512] = "";
  char updated[128] = "";
  int n = 0;
  char *cur = body, *line;
  while ((line = jo_next_line(&cur)) != NULL) {
    if (!line[0]) continue;
    if (line[0] == '#') {
      /* keep the two most useful header facts for provenance */
      if (!provenance[0] && strstr(line, "ipset:"))
        snprintf(provenance, sizeof provenance, "%s", line + 1);
      if (!updated[0] && (strstr(line, "Source File Date") || strstr(line, "updated")))
        snprintf(updated, sizeof updated, "%s", line + 1);
      continue;
    }
    if (!is_v4_cidr(line)) continue;

    const char *slash = strchr(line, '/');

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "prefix", line);
    cJSON_AddBoolToObject(p, "is_cidr", slash != NULL);
    cJSON_AddStringToObject(p, "feed", "firehol_level1");
    cJSON_AddStringToObject(p, "confidence_tier", "level1");
    if (provenance[0]) cJSON_AddStringToObject(p, "provenance", provenance);
    if (updated[0])    cJSON_AddStringToObject(p, "feed_updated", updated);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    intel_item it = {0};
    it.remote_key      = line;
    it.title           = line;
    it.summary         = "FireHOL level1 — maximum protection with minimum false positives";
    it.link            = CYI_URL;
    it.lang            = "en";
    it.record_type     = "malicious-netblock";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"blocklist\",\"netblock\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[firehol-level1] emitted %d netsets\n", n);
  return 0;
}

static const source_def cyi_firehol_level1_def = {
  .id = "firehol-level1", .collector = "cyber",
  .name = "FireHOL level1 blocklist (aggregated)",
  .update_interval_sec = 21600, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "Curated union of the highest-confidence hostile netblocks as CIDRs — the low-false-positive tier for scoring an IP.",
  .license = "firehol/blocklist-ipsets (GPLv3-style repo licence); constituent upstream feeds keep their own terms.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_firehol_level1_def)
