/* collectors/cyber/sources/virustotal_jp.c
 * Port of server/src/collectors/virustotalJp.js (createThreatIntelCollector).
 * env VIRUSTOTAL_API_KEY → per-JP-domain VT v3 reputation, 16s spacing,
 * stop on 429. TOKYO points. */
#include "../../source.h"
#include "../../lib/threatintel.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TOKYO_LON 139.6917
#define TOKYO_LAT 35.6895
#define VT_BASE "https://www.virustotal.com/api/v3/domains/"

static const char *const DEFAULT_DOMAINS[] = {
  "mufg.jp", "rakuten.co.jp", "japanpost.jp", "jal.co.jp", "ana.co.jp",
  "sony.co.jp", "toyota.co.jp", "meti.go.jp", "mod.go.jp", "kantei.go.jp",
  "docomo.ne.jp", "softbank.jp", "kddi.com",
};
#define N_DEFAULT_DOMAINS (int)(sizeof DEFAULT_DOMAINS / sizeof DEFAULT_DOMAINS[0])

static void add_tokyo_geom(cJSON *f) {
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *co = cJSON_CreateArray();
  cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LON));
  cJSON_AddItemToArray(co, cJSON_CreateNumber(TOKYO_LAT));
  cJSON_AddItemToObject(g, "coordinates", co);
  cJSON_AddItemToObject(f, "geometry", g);
}

/* a.x — VT attribute or undefined; JS leaves the prop value `undefined`
 * (serialised away). Here emit only when present (object key dropped if
 * absent → matches JSON.stringify dropping undefined). */
static void add_attr(cJSON *p, cJSON *a, const char *k) {
  cJSON *v = a ? cJSON_GetObjectItem(a, k) : NULL;
  if (v) cJSON_AddItemToObject(p, k, cJSON_Duplicate(v, 1));
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)ud;
  cJSON *features = cJSON_CreateArray();
  char keyh[256];
  snprintf(keyh, sizeof keyh, "x-apikey: %s", key ? key : "");

  for (int i = 0; i < N_DEFAULT_DOMAINS; i++) {
    const char *domain = DEFAULT_DOMAINS[i];
    char url[256];
    snprintf(url, sizeof url, "%s%s", VT_BASE, domain);
    const char *hdrs[] = { keyh, "Accept: application/json", NULL };
    cJSON *dom = feed_get_json_h(ctx->http, url, hdrs, 15000);

    /* dom?.rate_limited (HTTP 429 in JS). feed_get_json_h yields NULL on
     * non-2xx, so we cannot distinguish 429 from other errors; treat a
     * "rate_limited" body field if upstream ever returns it. */
    cJSON *rl = dom ? cJSON_GetObjectItem(dom, "rate_limited") : NULL;
    if (rl && cJSON_IsTrue(rl)) {
      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      add_tokyo_geom(f);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "domain", domain);
      cJSON_AddBoolToObject(p, "rate_limited", 1);
      cJSON_AddStringToObject(p, "source", "virustotal");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
      if (dom) cJSON_Delete(dom);
      break;
    }

    cJSON *data = dom ? cJSON_GetObjectItem(dom, "data") : NULL;
    cJSON *a = data ? cJSON_GetObjectItem(data, "attributes") : NULL;

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    add_tokyo_geom(f);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddNumberToObject(p, "idx", i);
    cJSON_AddStringToObject(p, "domain", domain);
    add_attr(p, a, "last_analysis_stats");
    /* last_dns_records: array slice(0,5) else [] */
    cJSON *dns = a ? cJSON_GetObjectItem(a, "last_dns_records") : NULL;
    cJSON *dns_out = cJSON_CreateArray();
    if (cJSON_IsArray(dns)) {
      int n = cJSON_GetArraySize(dns); if (n > 5) n = 5;
      for (int k = 0; k < n; k++)
        cJSON_AddItemToArray(dns_out,
          cJSON_Duplicate(cJSON_GetArrayItem(dns, k), 1));
    }
    cJSON_AddItemToObject(p, "last_dns_records", dns_out);
    add_attr(p, a, "registrar");
    add_attr(p, a, "creation_date");
    add_attr(p, a, "last_modification_date");
    add_attr(p, a, "reputation");
    add_attr(p, a, "total_votes");
    add_attr(p, a, "categories");
    /* err: dom?.err || null */
    cJSON *err = dom ? cJSON_GetObjectItem(dom, "err") : NULL;
    cJSON_AddItemToObject(p, "err",
      (err && !cJSON_IsNull(err)) ? cJSON_Duplicate(err, 1)
        : (dom ? cJSON_CreateNull() : cJSON_CreateString("fetch_failed")));
    cJSON_AddStringToObject(p, "source", "virustotal");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
    if (dom) cJSON_Delete(dom);

    /* 16s spacing to fit the free 4 req/min budget (JS setTimeout 16000). */
    if (i < N_DEFAULT_DOMAINS - 1) {
      struct timespec ts = { 16, 0 };
      nanosleep(&ts, NULL);
    }
  }
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, "VIRUSTOTAL_API_KEY", NULL,
                              run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def virustotal_jp_def = {
  .id = "virustotal-jp", .collector = "cyber",
  .name = "VirusTotal (JP corps)", .name_ja = "VirusTotal 日本企業",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(virustotal_jp_def)
