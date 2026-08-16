/* collectors/cyber/sources/cloudflare_radar_jp.c
 * Port of server/src/collectors/cloudflareRadarJp.js (createThreatIntelCollector).
 * env CLOUDFLARE_API_TOKEN → 5 Radar endpoints merged, TOKYO points. */
#include "source.h"
#include "lib/threatintel.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CF_BASE "https://api.cloudflare.com/client/v4/radar"

/* radar(path,token): GET, on !ok return {err:"HTTP <status>"}; here a NULL
 * json → err string. Returns malloc'd cJSON object (caller frees). */
static cJSON *radar(const source_ctx *ctx, const char *path, const char *auth) {
  char url[320];
  snprintf(url, sizeof url, "%s%s", CF_BASE, path);
  const char *hdrs[] = { auth, "Accept: application/json", NULL };
  return feed_get_json_h(ctx->http, url, hdrs, 15000);
}

static void push_kind(cJSON *features, const char *kind, cJSON *json) {
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  /* NO GEOMETRY. Every row here used to be emitted at Tokyo Station
   * (35.6895, 139.6917). What this source reports has no location,
   * and stacking every row on one pin is the fabrication the
   * 2026-07-31 audit deleted from cisa-kev-jp, poc-in-github and
   * peeringdb-jp. Confirmed live by tests/contract/source_contract.py.
   * lib/geojson.c handles an absent geometry correctly — do NOT
   * reintroduce a fallback coordinate. */

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "kind", kind);
  cJSON *res = json ? cJSON_GetObjectItem(json, "result") : NULL;
  cJSON_AddItemToObject(p, "result",
    res ? cJSON_Duplicate(res, 1) : cJSON_CreateNull());
  /* err: json?.err || null — feed returns parsed JSON; no synthetic err. */
  cJSON *err = json ? cJSON_GetObjectItem(json, "err") : NULL;
  cJSON_AddItemToObject(p, "err",
    (err && !cJSON_IsNull(err)) ? cJSON_Duplicate(err, 1) : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "source", "cf_radar");
  cJSON_AddItemToObject(f, "properties", p);
  cJSON_AddItemToArray(features, f);
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)ud;
  char auth[256];
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", key ? key : "");
  cJSON *l3 = radar(ctx, "/attacks/layer3/summary?location=JP&dateRange=7d", auth);
  cJSON *l7 = radar(ctx, "/attacks/layer7/top/origins?location=JP&dateRange=7d&limit=10", auth);
  cJSON *bgp = radar(ctx, "/bgp/leaks/events?involved_country=JP&per_page=20", auth);
  cJSON *iqi = radar(ctx, "/quality/iqi/summary?location=JP&dateRange=7d", auth);
  cJSON *dns = radar(ctx, "/dns/top/locations?location=JP&dateRange=7d&limit=10", auth);

  cJSON *features = cJSON_CreateArray();
  push_kind(features, "attacks_layer3", l3);
  push_kind(features, "attacks_layer7_top_origins", l7);
  push_kind(features, "bgp_leaks", bgp);
  push_kind(features, "quality_iqi", iqi);
  push_kind(features, "dns_top_locations", dns);

  if (l3) cJSON_Delete(l3);
  if (l7) cJSON_Delete(l7);
  if (bgp) cJSON_Delete(bgp);
  if (iqi) cJSON_Delete(iqi);
  if (dns) cJSON_Delete(dns);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, "CLOUDFLARE_API_TOKEN", NULL,
                              run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def cloudflare_radar_jp_def = {
  .id = "cloudflare-radar-jp", .collector = "cyber",
  .name = "Cloudflare Radar (JP)", .name_ja = "Cloudflare Radar 日本",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(cloudflare_radar_jp_def)
