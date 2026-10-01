/* collectors/cyber/sources/greynoise_jp.c
 * Port of server/src/collectors/greynoiseJp.js (intelEnvelope).
 * GreyNoise community per-IP classifier over a curated IP list. The intel
 * envelope IS the product: ONE intel row per polled IP (incl. quiet / error
 * rows — JS .map always yields an item). GREYNOISE_API_KEY optional (sent as
 * `key:` header when present); endpoint also tolerates anonymous. Curated
 * _meta dropped. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "core/httpclient.h"
#include "third_party/cJSON.h"
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
    /* The community endpoint answers **404 with a real JSON body** for the
     * commonest verdict ("IP not observed scanning the internet"), and
     * feed_get_json_h() throws away every non-2xx body. Every poll therefore
     * produced 5 rows titled "<ip> — unknown" / summary "Quiet" that were
     * indistinguishable from a genuine quiet verdict. Take the body ourselves
     * and keep the status. */
    http_response resp = {0};
    cJSON *r = NULL;
    int status = 0;
    if (http_request(ctx->http, "GET", url, hdrs, NULL, 0, 10000, 1, &resp) == 0) {
      status = (int)resp.status;
      if (resp.body && (status == 200 || status == 404)) r = cJSON_Parse(resp.body);
      if (r && !cJSON_GetObjectItem(r, "ip")) { cJSON_Delete(r); r = NULL; }
    }
    http_response_free(&resp);

    if (!r) {   /* transport error, 401/429, or an unparseable body: say
                 * nothing rather than persist a fabricated "Quiet" row. */
      fprintf(stderr, "[greynoise-jp] %s: no usable answer (http %d)\n",
              ip, status);
      continue;
    }
    /* GreyNoise's own wording for the 404 case, carried through verbatim. */
    const char *message = jo_sv(r, "message");
    const char *classification = r ? jo_sv(r, "classification") : NULL;
    const char *name = r ? jo_sv(r, "name") : NULL;
    const char *last_seen = r ? jo_sv(r, "last_seen") : NULL;
    const char *first_seen = r ? jo_sv(r, "first_seen") : NULL;
    const cJSON *noisev = r ? cJSON_GetObjectItem(r, "noise") : NULL;
    int has_noise = noisev && (cJSON_IsBool(noisev));
    int noise = has_noise && cJSON_IsTrue(noisev);
    const char *err = NULL;

    /* title = `${ip} — ${classification || message-derived state}` */
    char title[192];
    snprintf(title, sizeof title, "%s \xE2\x80\x94 %s", ip,
             classification ? classification
                            : (status == 404 ? "not observed scanning"
                                             : "no classification"));

    /* summary = name || upstream message || noise state */
    const char *summary = name ? name
                          : (message ? message
                             : (noise ? "Noise (background scanner)" : "Quiet"));

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
    /* keep the upstream verdict text + the status it came with, so a reader
     * can tell "observed and quiet" from "never seen" */
    cJSON_AddItemToObject(p, "message",
      message ? cJSON_CreateString(message) : cJSON_CreateNull());
    cJSON_AddNumberToObject(p, "http_status", status);
    cJSON_AddItemToObject(p, "riot", cJSON_GetObjectItem(r, "riot")
      ? cJSON_Duplicate(cJSON_GetObjectItem(r, "riot"), 1) : cJSON_CreateNull());
    char *pj = cJSON_PrintUnformatted(p);

    /* provenance: rows carried no link at all — GreyNoise's public viz page
     * for the very IP we queried is the natural one. */
    char vlink[96];
    snprintf(vlink, sizeof vlink, "https://viz.greynoise.io/ip/%s", ip);

    intel_item row = {0};
    row.remote_key     = ip;                   /* uid greynoise-jp|<ip> */
    row.title          = title;
    row.summary        = summary;
    row.link           = vlink;
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
