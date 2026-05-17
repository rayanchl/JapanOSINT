/* collectors/cyber/sources/hudson_rock_jp.c
 * Port of server/src/collectors/hudsonRockJp.js (createThreatIntelCollector).
 * Keyless. HudsonRock Cavalier per-JP-domain infostealer summary, TOKYO points. */
#include "../../../source.h"
#include "../../../lib/threatintel.h"
#include "../../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOKYO_LON 139.6917
#define TOKYO_LAT 35.6895
#define HR_BASE "https://cavalier.hudsonrock.com/api/json/v2/osint-tools/search-by-domain"

static const char *const DEFAULT_DOMAINS[] = {
  "mufg.jp", "smbc.co.jp", "mizuho-fg.co.jp", "rakuten-bank.co.jp", "japanpost.jp",
  "ntt.co.jp", "docomo.ne.jp", "kddi.com", "softbank.jp", "iij.ad.jp", "rakuten.co.jp",
  "sony.co.jp", "panasonic.com", "fujitsu.com", "nec.com", "hitachi.co.jp",
  "mitsubishielectric.co.jp", "toshiba.co.jp", "canon.jp", "ricoh.co.jp",
  "toyota.co.jp", "honda.co.jp", "nissan.co.jp", "subaru.co.jp", "mazda.co.jp",
  "jal.co.jp", "ana.co.jp", "jr-east.co.jp", "jr-central.co.jp",
  "meti.go.jp", "mod.go.jp", "kantei.go.jp", "mofa.go.jp", "soumu.go.jp",
};
#define N_DEFAULT_DOMAINS (int)(sizeof DEFAULT_DOMAINS / sizeof DEFAULT_DOMAINS[0])

/* data?.x ?? null → real cJSON value or JSON null. */
static cJSON *dn(cJSON *data, const char *k) {
  cJSON *v = data ? cJSON_GetObjectItem(data, k) : NULL;
  return (v && !cJSON_IsNull(v)) ? cJSON_Duplicate(v, 1) : cJSON_CreateNull();
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)key; (void)ud;
  cJSON *features = cJSON_CreateArray();
  for (int i = 0; i < N_DEFAULT_DOMAINS; i++) {
    const char *domain = DEFAULT_DOMAINS[i];
    char url[320];
    snprintf(url, sizeof url, "%s?domain=%s", HR_BASE, domain);
    const char *hdrs[] = { "Accept: application/json", NULL };
    cJSON *data = feed_get_json_h(ctx->http, url, hdrs, 15000);

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LON));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LAT));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "idx", i);
    cJSON_AddStringToObject(p, "domain", domain);
    cJSON_AddItemToObject(p, "total_employees", dn(data, "total_employees"));
    cJSON_AddItemToObject(p, "total_users", dn(data, "total_users"));
    cJSON_AddItemToObject(p, "employees", dn(data, "employees"));
    cJSON_AddItemToObject(p, "users", dn(data, "users"));
    cJSON_AddItemToObject(p, "third_parties", dn(data, "third_parties"));
    cJSON_AddItemToObject(p, "stealers", dn(data, "stealers"));
    cJSON_AddItemToObject(p, "total_external_domains", dn(data, "total_external_domains"));
    cJSON_AddItemToObject(p, "external_domains", dn(data, "external_domains"));
    cJSON_AddItemToObject(p, "message", dn(data, "message"));
    /* err: r.err || null — HTTP error (no body) → "fetch_failed"-ish; the
     * C fetch yields NULL data, JS would have set err. Keep null when data
     * present, else surface a generic fetch error. */
    cJSON_AddItemToObject(p, "err",
      data ? cJSON_CreateNull() : cJSON_CreateString("fetch_failed"));
    cJSON_AddStringToObject(p, "source", "hudsonrock_osint");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
    if (data) cJSON_Delete(data);
  }
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, NULL, NULL, run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def hudson_rock_jp_def = {
  .id = "hudson-rock-jp", .collector = "cyber",
  .name = "HudsonRock (JP corps)", .name_ja = "HudsonRock 日本企業",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(hudson_rock_jp_def)
