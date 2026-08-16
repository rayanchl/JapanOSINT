/* collectors/sources/cyi_fastly_ip_ranges.c
 * Fastly edge IP ranges — so a host behind Fastly is never geolocated as if the
 * PoP were the origin.
 * Endpoint: https://api.fastly.com/public-ip-list                    (keyless)
 * parse_notes: "Two flat string arrays" — addresses[] (v4) and
 * ipv6_addresses[] (v6). Every emitted CIDR is a literal string from the reply.
 * No coordinates -> has_geo 0 (R2).
 * Licence: Fastly public endpoint, no key.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://api.fastly.com/public-ip-list"

static int emit_cidrs(intel_sink *sink, const cJSON *arr, const char *af) {
  int n = 0;
  const cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    if (!cJSON_IsString(e) || !e->valuestring || !e->valuestring[0]) continue;
    const char *cidr = e->valuestring;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "prefix", cidr);
    cJSON_AddStringToObject(p, "address_family", af);
    cJSON_AddStringToObject(p, "provider", "fastly");
    cJSON_AddStringToObject(p, "role", "cdn-edge");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    intel_item it = {0};
    it.remote_key      = cidr;
    it.title           = cidr;
    it.summary         = "Fastly CDN edge range";
    it.link            = CYI_URL;
    it.lang            = "en";
    it.record_type     = "cdn-ip-range";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"cdn\",\"fastly\",\"ip-range\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 20000);
  if (!doc) { fprintf(stderr, "[fastly-ip-ranges] fetch/parse failed\n"); return -1; }

  int n = emit_cidrs(sink, cJSON_GetObjectItem(doc, "addresses"), "ipv4");
  n += emit_cidrs(sink, cJSON_GetObjectItem(doc, "ipv6_addresses"), "ipv6");

  cJSON_Delete(doc);
  fprintf(stderr, "[fastly-ip-ranges] emitted %d\n", n);
  return 0;
}

static const source_def cyi_fastly_ip_ranges_def = {
  .id = "fastly-ip-ranges", .collector = "cyber",
  .name = "Fastly edge IP ranges",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "api", .url = CYI_URL,
  .description = "Fastly's complete edge ranges — lets the platform say 'this host is behind Fastly, the answer is not the origin'.",
  .license = "Fastly public endpoint, no key.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_fastly_ip_ranges_def)
