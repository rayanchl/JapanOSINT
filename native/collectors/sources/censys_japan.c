/* collectors/cyber/sources/censys_japan.c
 * Port of server/src/collectors/censysJapan.js (createThreatIntelCollector).
 * env CENSYS_API_ID (gate) + CENSYS_API_SECRET → Basic auth, POST hosts/search
 * q="location.country_code: JP" per_page 100. geom = coords or null. */
#include "source.h"
#include "lib/threatintel.h"
#include "core/httpclient.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CENSYS_URL "https://search.censys.io/api/v2/hosts/search"

static const char B64[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* standard base64 of arbitrary bytes (Buffer.from(s).toString('base64')) */
static void b64(const char *in, char *out, size_t outsz) {
  size_t n = strlen(in), j = 0;
  size_t i;
  for (i = 0; i + 2 < n; i += 3) {
    unsigned v = ((unsigned char)in[i] << 16) |
                 ((unsigned char)in[i+1] << 8) | (unsigned char)in[i+2];
    if (j + 4 >= outsz) break;
    out[j++] = B64[(v >> 18) & 63];
    out[j++] = B64[(v >> 12) & 63];
    out[j++] = B64[(v >> 6) & 63];
    out[j++] = B64[v & 63];
  }
  if (i < n && j + 4 < outsz) {
    unsigned v = (unsigned char)in[i] << 16;
    int rem = (int)(n - i);
    if (rem == 2) v |= (unsigned char)in[i+1] << 8;
    out[j++] = B64[(v >> 18) & 63];
    out[j++] = B64[(v >> 12) & 63];
    out[j++] = (rem == 2) ? B64[(v >> 6) & 63] : '=';
    out[j++] = '=';
  }
  out[j] = '\0';
}

static cJSON *opt2(cJSON *o, const char *k1, const char *k2) {
  cJSON *a = o ? cJSON_GetObjectItem(o, k1) : NULL;
  cJSON *b = a ? cJSON_GetObjectItem(a, k2) : NULL;
  if (!b || cJSON_IsNull(b) || (cJSON_IsString(b) && !b->valuestring[0]))
    return cJSON_CreateNull();
  return cJSON_Duplicate(b, 1);
}

static cJSON *run_fetch(const char *key, const source_ctx *ctx, void *ud) {
  (void)ud;
  const char *id = key;                        /* CENSYS_API_ID */
  const char *secret = getenv("CENSYS_API_SECRET");
  if (!id || !*id || !secret || !*secret) return NULL; /* JS throws → 0 rows */

  char creds[512];
  snprintf(creds, sizeof creds, "%s:%s", id, secret);
  char enc[768];
  b64(creds, enc, sizeof enc);
  char authh[800];
  snprintf(authh, sizeof authh, "authorization: Basic %s", enc);
  const char *hdrs[] = { authh, "content-type: application/json",
                         "accept: application/json", NULL };
  const char *body =
    "{\"q\":\"location.country_code: JP\",\"per_page\":100}";

  http_response resp = {0};
  if (http_request(ctx->http, "POST", CENSYS_URL, hdrs, body, strlen(body),
                    20000, 2, &resp) != 0 || resp.status < 200 ||
      resp.status >= 300 || !resp.body) {
    http_response_free(&resp);
    return NULL;
  }
  cJSON *data = cJSON_Parse(resp.body);
  http_response_free(&resp);
  if (!data) return NULL;

  cJSON *result = cJSON_GetObjectItem(data, "result");
  cJSON *hits = result ? cJSON_GetObjectItem(result, "hits") : NULL;
  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(hits)) {
    cJSON *h;
    cJSON_ArrayForEach(h, hits) {
      cJSON *loc = cJSON_GetObjectItem(h, "location");
      cJSON *crd = loc ? cJSON_GetObjectItem(loc, "coordinates") : NULL;
      cJSON *latv = crd ? cJSON_GetObjectItem(crd, "latitude") : NULL;
      cJSON *lonv = crd ? cJSON_GetObjectItem(crd, "longitude") : NULL;
      int has = lonv && cJSON_IsNumber(lonv) && latv && cJSON_IsNumber(latv);

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      if (has) {
        cJSON *g = cJSON_CreateObject();
        cJSON_AddStringToObject(g, "type", "Point");
        cJSON *co = cJSON_CreateArray();
        cJSON_AddItemToArray(co, cJSON_CreateNumber(lonv->valuedouble));
        cJSON_AddItemToArray(co, cJSON_CreateNumber(latv->valuedouble));
        cJSON_AddItemToObject(g, "coordinates", co);
        cJSON_AddItemToObject(f, "geometry", g);
      } else {
        cJSON_AddItemToObject(f, "geometry", cJSON_CreateNull());
      }

      cJSON *pr = cJSON_CreateObject();        /* EXACT JS key order */
      cJSON *ipv = cJSON_GetObjectItem(h, "ip");
      const char *ip = (ipv && cJSON_IsString(ipv)) ? ipv->valuestring : "";
      char idbuf[128];
      snprintf(idbuf, sizeof idbuf, "CENSYS_%s", ip);
      cJSON_AddStringToObject(pr, "id", idbuf);
      cJSON_AddItemToObject(pr, "ip",
        ipv ? cJSON_Duplicate(ipv, 1) : cJSON_CreateNull());
      cJSON_AddItemToObject(pr, "asn", opt2(h, "autonomous_system", "asn"));
      cJSON_AddItemToObject(pr, "as_name", opt2(h, "autonomous_system", "name"));
      cJSON_AddItemToObject(pr, "as_country",
        opt2(h, "autonomous_system", "country_code"));
      cJSON_AddItemToObject(pr, "city", opt2(h, "location", "city"));
      cJSON_AddItemToObject(pr, "province", opt2(h, "location", "province"));

      cJSON *svcs = cJSON_GetObjectItem(h, "services");
      cJSON *sarr = cJSON_CreateArray();
      if (cJSON_IsArray(svcs)) {
        cJSON *s;
        cJSON_ArrayForEach(s, svcs) {
          cJSON *pv = cJSON_GetObjectItem(s, "port");
          cJSON *snv = cJSON_GetObjectItem(s, "service_name");
          cJSON *tpv = cJSON_GetObjectItem(s, "transport_protocol");
          char portbuf[32] = "";
          if (pv && cJSON_IsNumber(pv))
            snprintf(portbuf, sizeof portbuf, "%g", pv->valuedouble);
          else if (pv && cJSON_IsString(pv))
            snprintf(portbuf, sizeof portbuf, "%s", pv->valuestring);
          const char *nm = (snv && cJSON_IsString(snv) && snv->valuestring[0])
            ? snv->valuestring
            : ((tpv && cJSON_IsString(tpv)) ? tpv->valuestring : "");
          char entry[128];
          snprintf(entry, sizeof entry, "%s/%s", portbuf, nm);
          cJSON_AddItemToArray(sarr, cJSON_CreateString(entry));
        }
      }
      cJSON_AddItemToObject(pr, "services", sarr);
      cJSON *lu = cJSON_GetObjectItem(h, "last_updated_at");
      cJSON_AddItemToObject(pr, "last_updated",
        (lu && !cJSON_IsNull(lu) && !(cJSON_IsString(lu) && !lu->valuestring[0]))
          ? cJSON_Duplicate(lu, 1) : cJSON_CreateNull());
      cJSON_AddStringToObject(pr, "source", "censys_hosts_search_v2");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
    }
  }
  cJSON_Delete(data);
  return features;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = threatintel_collect(ctx, sink, "CENSYS_API_ID", NULL,
                              run_fetch, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def censys_japan_def = {
  .id = "censys-japan", .collector = "cyber",
  .name = "Censys (Japan hosts)", .name_ja = "Censys 日本ホスト",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(censys_japan_def)
