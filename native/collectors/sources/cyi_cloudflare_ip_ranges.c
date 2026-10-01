/* collectors/sources/cyi_cloudflare_ip_ranges.c
 * Cloudflare edge/proxy IP ranges.
 * Endpoint: https://api.cloudflare.com/client/v4/ips                 (keyless)
 * parse_notes: "Same host as the existing cloudflare-radar-jp source but this
 * endpoint needs no token, unlike the /client/v4/radar paths. Envelope is
 * {success, result:{...}} — check success before reading result." Both checks
 * are done here; a success:false envelope is treated as an upstream failure.
 * Emits: ipv4_cidrs[]/ipv6_cidrs[] entries plus the result etag.
 * No coordinates -> has_geo 0 (R2).
 * Licence: Cloudflare public API endpoint, works without a token.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://api.cloudflare.com/client/v4/ips"

static int emit_cidrs(intel_sink *sink, const cJSON *arr, const char *af,
                      const char *etag) {
  int n = 0;
  const cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    if (!cJSON_IsString(e) || !e->valuestring || !e->valuestring[0]) continue;
    const char *cidr = e->valuestring;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "prefix", cidr);
    cJSON_AddStringToObject(p, "address_family", af);
    cJSON_AddStringToObject(p, "provider", "cloudflare");
    cJSON_AddStringToObject(p, "role", "cdn-proxy");
    if (etag) cJSON_AddStringToObject(p, "etag", etag);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    intel_item it = {0};
    it.remote_key      = cidr;
    it.title           = cidr;
    it.summary         = "Cloudflare proxy range";
    it.link            = CYI_URL;
    it.lang            = "en";
    it.record_type     = "cdn-ip-range";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"cdn\",\"cloudflare\",\"ip-range\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 20000);
  if (!doc) { fprintf(stderr, "[cloudflare-ip-ranges] fetch/parse failed\n"); return -1; }

  const cJSON *ok = cJSON_GetObjectItem(doc, "success");
  const cJSON *res = cJSON_GetObjectItem(doc, "result");
  if (!cJSON_IsTrue(ok) || !cJSON_IsObject(res)) {
    cJSON_Delete(doc);
    fprintf(stderr, "[cloudflare-ip-ranges] envelope success=false\n");
    return -1;                          /* the API itself reported failure */
  }

  const cJSON *et = cJSON_GetObjectItem(res, "etag");
  const char *etag = (cJSON_IsString(et) && et->valuestring && et->valuestring[0])
                       ? et->valuestring : NULL;

  int n = emit_cidrs(sink, cJSON_GetObjectItem(res, "ipv4_cidrs"), "ipv4", etag);
  n += emit_cidrs(sink, cJSON_GetObjectItem(res, "ipv6_cidrs"), "ipv6", etag);

  cJSON_Delete(doc);
  fprintf(stderr, "[cloudflare-ip-ranges] emitted %d\n", n);
  return 0;
}

static const source_def cyi_cloudflare_ip_ranges_def = {
  .id = "cloudflare-ip-ranges", .collector = "cyber",
  .name = "Cloudflare edge IP ranges",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "api", .url = CYI_URL,
  .description = "Cloudflare's proxy ranges, keyless — completes the CDN attribution set so a 104.x hit is never mistaken for the customer's own hosting.",
  .license = "Cloudflare public API endpoint, no token required.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_cloudflare_ip_ranges_def)
