/* collectors/cyber/sources/greynoise_jp.c
 * Port of server/src/collectors/greynoiseJp.js (intelEnvelope).
 * GreyNoise community per-IP classifier over a curated IP list. The intel
 * envelope IS the product: ONE intel row per polled IP (incl. quiet / error
 * rows — JS .map always yields an item). GREYNOISE_API_KEY optional (sent as
 * `key:` header when present); endpoint also tolerates anonymous. Curated
 * _meta dropped. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE "https://api.greynoise.io/v3/community"

/* DEFAULT_IPS — verbatim JS order (GREYNOISE_IPS env override not modelled). */
static const char *IPS[] = {
  "8.8.8.8", "1.1.1.1",
  "133.71.100.50",
  "210.152.11.100",
  "203.104.130.1",
};

static const char *sv(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && v->valuestring && v->valuestring[0])
           ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("GREYNOISE_API_KEY");
  char keyh[256];
  const char *hdrs_keyed[3];
  const char *hdrs_anon[2] = { "Accept: application/json", NULL };
  const char *const *hdrs;
  if (key && *key) {
    snprintf(keyh, sizeof keyh, "key: %s", key);
    hdrs_keyed[0] = "Accept: application/json";
    hdrs_keyed[1] = keyh;
    hdrs_keyed[2] = NULL;
    hdrs = hdrs_keyed;
  } else {
    hdrs = hdrs_anon;
  }

  int n = 0;
  for (size_t i = 0; i < sizeof(IPS) / sizeof(IPS[0]); i++) {
    const char *ip = IPS[i];
    char url[256];
    snprintf(url, sizeof url, "%s/%s", BASE, ip);
    cJSON *r = feed_get_json_h(ctx->http, url, hdrs, 10000);

    /* r is the GreyNoise body, or — on HTTP error / 429 — JS synthesises a
     * tiny object. We model: success → use body; failure → null fields with
     * a generic error marker. rate_limited only when we'd see a 429, which
     * feed_get_json_h cannot distinguish (returns NULL), so treat as error. */
    const char *classification = r ? sv(r, "classification") : NULL;
    const char *name = r ? sv(r, "name") : NULL;
    const char *last_seen = r ? sv(r, "last_seen") : NULL;
    const char *first_seen = r ? sv(r, "first_seen") : NULL;
    const cJSON *noisev = r ? cJSON_GetObjectItem(r, "noise") : NULL;
    int has_noise = noisev && (cJSON_IsBool(noisev));
    int noise = has_noise && cJSON_IsTrue(noisev);
    const char *err = r ? NULL : "fetch failed";

    /* title = `${ip} — ${classification||'unknown'}` */
    char title[128];
    snprintf(title, sizeof title, "%s \xE2\x80\x94 %s",
             ip, classification ? classification : "unknown");

    /* summary = name || (noise ? 'Noise (background scanner)' : 'Quiet') */
    const char *summary = name ? name
                          : (noise ? "Noise (background scanner)" : "Quiet");

    /* tags = ['ip-classifier', class? `class:<c>`:null, noise?'noise':null] */
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("ip-classifier"));
    if (classification) {
      char ct[64];
      snprintf(ct, sizeof ct, "class:%s", classification);
      cJSON_AddItemToArray(tags, cJSON_CreateString(ct));
    }
    if (noise) cJSON_AddItemToArray(tags, cJSON_CreateString("noise"));
    char *tj = cJSON_PrintUnformatted(tags);

    /* properties — EXACT JS key order */
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "ip", ip);
    cJSON_AddItemToObject(p, "noise",
      has_noise ? cJSON_CreateBool(noise) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "classification",
      classification ? cJSON_CreateString(classification) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "name",
      name ? cJSON_CreateString(name) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "last_seen",
      last_seen ? cJSON_CreateString(last_seen) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "first_seen",
      first_seen ? cJSON_CreateString(first_seen) : cJSON_CreateNull());
    cJSON_AddBoolToObject(p, "rate_limited", 0);
    cJSON_AddItemToObject(p, "error",
      err ? cJSON_CreateString(err) : cJSON_CreateNull());
    char *pj = cJSON_PrintUnformatted(p);

    intel_item row = {0};
    row.remote_key     = ip;                   /* uid greynoise-jp|<ip> */
    row.title          = title;
    row.summary        = summary;
    row.lang           = "en";
    row.published_at   = last_seen;            /* r.last_seen || null */
    row.properties_json = pj;
    row.tags_json      = tj;
    if (sink->emit(sink, &row) >= 0) n++;

    free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags);
    if (r) cJSON_Delete(r);
  }
  fprintf(stderr, "[greynoise-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def greynoise_jp_def = {
  .id = "greynoise-jp", .collector = "cyber",
  .name = "GreyNoise (curated IPs)",
  .name_ja = "GreyNoise (IP\xE3\x83\xAA\xE3\x82\xB9\xE3\x83\x88)",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(greynoise_jp_def)
