/* collectors/sources/cyi_gcp_ip_ranges.c
 * Google Cloud public IP ranges — the Google half of the cloud-attribution set.
 * Endpoint: https://www.gstatic.com/ipranges/cloud.json              (keyless)
 * parse_notes: "Single prefixes[] array where each entry carries either
 * ipv4Prefix or ipv6Prefix, never both" — both keys are checked per entry.
 * Emits: prefix, address family, service, scope (region), plus syncToken and
 * creationTime carried onto every row. No coordinates -> has_geo 0 (R2).
 * Licence: published by Google for public use.
 */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://www.gstatic.com/ipranges/cloud.json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, CYI_URL, 30000);
  if (!doc) { fprintf(stderr, "[gcp-ip-ranges] fetch/parse failed\n"); return -1; }

  const char *sync = jo_sv(doc, "syncToken");
  const char *created = jo_sv(doc, "creationTime");

  int n = 0;
  const cJSON *e;
  cJSON_ArrayForEach(e, cJSON_GetObjectItem(doc, "prefixes")) {
    const char *v4 = jo_sv(e, "ipv4Prefix");
    const char *v6 = jo_sv(e, "ipv6Prefix");
    const char *pfx = v4 ? v4 : v6;
    if (!pfx) continue;
    const char *service = jo_sv(e, "service");
    const char *scope = jo_sv(e, "scope");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "prefix", pfx);
    cJSON_AddStringToObject(p, "address_family", v4 ? "ipv4" : "ipv6");
    cJSON_AddStringToObject(p, "cloud", "gcp");
    if (service) cJSON_AddStringToObject(p, "service", service);
    if (scope)   cJSON_AddStringToObject(p, "scope", scope);
    if (sync)    cJSON_AddStringToObject(p, "sync_token", sync);
    if (created) cJSON_AddStringToObject(p, "creation_time", created);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char summary[192];
    snprintf(summary, sizeof summary, "%s%s%s",
             service ? service : "Google Cloud",
             scope ? " · " : "", scope ? scope : "");
    char key[160];
    snprintf(key, sizeof key, "%s|%s", pfx, scope ? scope : "-");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = pfx;
    it.summary         = summary;
    it.link            = CYI_URL;
    it.lang            = "en";
    it.published_at    = created;
    it.record_type     = "cloud-ip-range";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"cloud\",\"gcp\",\"ip-range\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[gcp-ip-ranges] emitted %d (syncToken %s)\n", n, sync ? sync : "n/a");
  return 0;
}

static const source_def cyi_gcp_ip_ranges_def = {
  .id = "gcp-ip-ranges", .collector = "cyber",
  .name = "Google Cloud public IP ranges",
  .update_interval_sec = 86400, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "GCP netblocks with region scope — the Google half of the cloud-attribution table; new regions show up the moment their prefixes appear.",
  .license = "Published by Google for public use.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_gcp_ip_ranges_def)
