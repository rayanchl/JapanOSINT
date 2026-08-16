/* collectors/sources/cyi_aws_ip_ranges.c
 * AWS public IP ranges — the authoritative netblock -> region -> service map.
 * Endpoint: https://ip-ranges.amazonaws.com/ip-ranges.json          (keyless)
 * parse_notes: "prefixes[] (v4) and ipv6_prefixes[] (v6) are separate arrays
 * with different key names (ip_prefix vs ipv6_prefix)" — both are read, with
 * the right key each. syncToken/createDate are carried onto every row so a
 * change of the publish token is visible downstream.
 * Emits: prefix, region, service, network_border_group, syncToken, createDate.
 * AWS publishes no coordinates -> has_geo 0 (R2).
 * Licence: published by AWS for public use.
 */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://ip-ranges.amazonaws.com/ip-ranges.json"

static int emit_arr(intel_sink *sink, const cJSON *arr, const char *pfx_key,
                    const char *af, const char *sync, const char *created) {
  int n = 0;
  const cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    const char *pfx = jo_sv(e, pfx_key);
    if (!pfx) continue;
    const char *region = jo_sv(e, "region");
    const char *service = jo_sv(e, "service");
    const char *nbg = jo_sv(e, "network_border_group");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "prefix", pfx);
    cJSON_AddStringToObject(p, "address_family", af);
    cJSON_AddStringToObject(p, "cloud", "aws");
    if (region)  cJSON_AddStringToObject(p, "region", region);
    if (service) cJSON_AddStringToObject(p, "service", service);
    if (nbg)     cJSON_AddStringToObject(p, "network_border_group", nbg);
    if (sync)    cJSON_AddStringToObject(p, "sync_token", sync);
    if (created) cJSON_AddStringToObject(p, "create_date", created);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char summary[192];
    snprintf(summary, sizeof summary, "AWS %s%s%s",
             region ? region : "", (region && service) ? " · " : "",
             service ? service : "");
    char key[160];
    snprintf(key, sizeof key, "%s|%s|%s", pfx, service ? service : "-",
             region ? region : "-");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = pfx;
    it.summary         = summary;
    it.link            = CYI_URL;
    it.lang            = "en";
    it.record_type     = "cloud-ip-range";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"cloud\",\"aws\",\"ip-range\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 40000);
  if (!doc) { fprintf(stderr, "[aws-ip-ranges] fetch/parse failed\n"); return -1; }

  const char *sync = jo_sv(doc, "syncToken");
  const char *created = jo_sv(doc, "createDate");

  int n = emit_arr(sink, cJSON_GetObjectItem(doc, "prefixes"), "ip_prefix",
                   "ipv4", sync, created);
  n += emit_arr(sink, cJSON_GetObjectItem(doc, "ipv6_prefixes"), "ipv6_prefix",
                "ipv6", sync, created);

  cJSON_Delete(doc);
  fprintf(stderr, "[aws-ip-ranges] emitted %d (syncToken %s)\n", n, sync ? sync : "n/a");
  return 0;
}

static const source_def cyi_aws_ip_ranges_def = {
  .id = "aws-ip-ranges", .collector = "cyber",
  .name = "AWS public IP ranges",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "Authoritative AWS netblock-to-region-to-service map — the attribution layer that tells cloud-hosted from residential for every IP the platform sees.",
  .license = "Published by AWS for public use.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_aws_ip_ranges_def)
