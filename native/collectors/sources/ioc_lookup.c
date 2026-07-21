/* collectors/osint/sources/ioc_lookup.c — IOC_LOOKUP, ported from OSINTsaas
 * threat_feed.c onto the JapanOSINT source ABI. Enriches an indicator of
 * compromise:
 *   - file hash (md5/sha1/sha256) → CIRCL hashlookup
 *   - ip / domain / url          → abuse.ch ThreatFox
 * Both are free, no key. Honest-empty when the indicator is unknown. */
#include "../../source.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int hex_len(const char *s) {
  int n = 0; for (const char *p = s; *p; p++, n++) if (!isxdigit((unsigned char)*p)) return -1;
  return n;
}
static const char *jstr(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

static int emit(intel_sink *sink, const char *entity, const char *src, cJSON *data) {
  char *bj = cJSON_PrintUnformatted(data);
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "IOC_LOOKUP");
  cJSON_AddStringToObject(props, "entity", entity);
  char *pj = cJSON_PrintUnformatted(props);
  char rk[256], title[256];
  snprintf(rk, sizeof rk, "ioc:%s:%s", src, entity);
  snprintf(title, sizeof title, "IOC %s — %s", entity, src);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.body = bj; it.summary = "ioc match";
  it.record_type = "osint_service_result"; it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"IOC_LOOKUP\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int ioc_run(const source_ctx *ctx, intel_sink *sink) {
  const char *e = ctx->entity;
  if (!e || !*e) return 0;
  int hl = hex_len(e);

  if (hl == 32 || hl == 40 || hl == 64) {          /* file hash → CIRCL */
    const char *algo = hl == 32 ? "md5" : (hl == 40 ? "sha1" : "sha256");
    char url[256]; snprintf(url, sizeof url, "https://hashlookup.circl.lu/lookup/%s/%s", algo, e);
    const char *headers[] = { "Accept: application/json", NULL };
    http_response hr = {0};
    if (http_request(ctx->http, "GET", url, headers, NULL, 0, 12000, 1, &hr) != 0 ||
        hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
    cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
    if (!j) return 0;
    if (cJSON_GetObjectItem(j, "message")) { cJSON_Delete(j); return 0; }  /* not found */
    cJSON_AddStringToObject(j, "source", "hashlookup.circl.lu");
    int r = emit(sink, e, "circl-hashlookup", j);
    cJSON_Delete(j);
    return r;
  }

  /* ip / domain / url → ThreatFox */
  char body[256];
  snprintf(body, sizeof body, "{\"query\":\"search_ioc\",\"search_term\":\"%s\"}", e);
  const char *headers[] = { "Content-Type: application/json", NULL };
  http_response hr = {0};
  if (http_request(ctx->http, "POST", "https://threatfox-api.abuse.ch/api/v1/",
                   headers, body, strlen(body), 12000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  if (!j) return 0;
  const char *status = jstr(j, "query_status");
  cJSON *data = cJSON_GetObjectItem(j, "data");
  int emitted = 0;
  if (status && strcmp(status, "ok") == 0 && data && cJSON_IsArray(data)) {
    int n = cJSON_GetArraySize(data);
    for (int i = 0; i < n && i < 15; i++) {
      cJSON *d = cJSON_GetArrayItem(data, i);
      cJSON *out = cJSON_CreateObject();
      cJSON_AddStringToObject(out, "source", "threatfox.abuse.ch");
      const char *keep[] = {"ioc","threat_type","malware","malware_printable",
                            "confidence_level","first_seen","tags",NULL};
      for (int k = 0; keep[k]; k++) {
        cJSON *v = cJSON_GetObjectItem(d, keep[k]);
        if (v) cJSON_AddItemToObject(out, keep[k], cJSON_Duplicate(v, 1));
      }
      emitted += emit(sink, e, "threatfox", out);
      cJSON_Delete(out);
    }
  }
  cJSON_Delete(j);
  return emitted;
}

static const source_def ioc_def = {
  .id = "IOC_LOOKUP", .collector = "osint", .name = "IOC Lookup", .run = ioc_run,
  .category = "investigation", .type = "api", .url = "https://threatfox.abuse.ch/",
  .description = "Indicator-of-compromise enrichment: hashes (CIRCL) + IP/domain/URL (ThreatFox), free.",
  .free_tier = 1,
};
REGISTER_SOURCE(ioc_def)
