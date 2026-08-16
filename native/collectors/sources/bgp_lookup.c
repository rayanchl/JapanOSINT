/* collectors/osint/sources/bgp_lookup.c — BGP_LOOKUP, ported onto the
 * JapanOSINT source ABI. Resolves an IP or ASN to its BGP origin (prefix,
 * announcing ASN(s), holder) via RIPEstat — free, no key. Honest-empty on no
 * data. */
#include "source.h"
#include "core/httpclient.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static const char *jstr(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

static cJSON *ripestat(http_client *http, const char *endpoint, const char *resource) {
  char url[400];
  snprintf(url, sizeof url, "https://stat.ripe.net/data/%s/data.json?resource=%s",
           endpoint, resource);
  http_response hr = {0};
  if (http_request(http, "GET", url, NULL, NULL, 0, 12000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  return j;
}

static int emit(intel_sink *sink, const char *entity, cJSON *data) {
  char *bj = cJSON_PrintUnformatted(data);
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "BGP_LOOKUP");
  cJSON_AddStringToObject(props, "entity", entity);
  char *pj = cJSON_PrintUnformatted(props);
  char rk[128], title[200];
  snprintf(rk, sizeof rk, "bgp:%s", entity);
  snprintf(title, sizeof title, "BGP — %s", entity);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.body = bj; it.summary = "BGP origin";
  it.record_type = "osint_service_result"; it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"BGP_LOOKUP\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int bgp_run(const source_ctx *ctx, intel_sink *sink) {
  const char *e = ctx->entity;
  if (!e || !*e) return 0;

  /* ASN form: "AS123" or bare digits → as-overview. */
  int is_asn = (strncasecmp(e, "AS", 2) == 0 && isdigit((unsigned char)e[2]));
  if (!is_asn) { is_asn = 1; for (const char *p = e; *p; p++) if (!isdigit((unsigned char)*p)) { is_asn = 0; break; } }

  if (is_asn) {
    char res[32];
    snprintf(res, sizeof res, "%s%s", (strncasecmp(e, "AS", 2) == 0) ? "" : "AS", e);
    cJSON *j = ripestat(ctx->http, "as-overview", res);
    cJSON *d = j ? cJSON_GetObjectItem(j, "data") : NULL;
    if (!d) { if (j) cJSON_Delete(j); return 0; }
    cJSON *out = cJSON_CreateObject();
    cJSON_AddStringToObject(out, "source", "stat.ripe.net");
    cJSON_AddStringToObject(out, "asn", res);
    const char *holder = jstr(d, "holder");
    if (holder) cJSON_AddStringToObject(out, "holder", holder);
    cJSON *ann = cJSON_GetObjectItem(d, "announced");
    if (ann) cJSON_AddItemToObject(out, "announced", cJSON_Duplicate(ann, 1));
    int r = emit(sink, e, out);
    cJSON_Delete(out); cJSON_Delete(j);
    return r;
  }

  /* IP form → network-info (prefix + asns). */
  cJSON *j = ripestat(ctx->http, "network-info", e);
  cJSON *d = j ? cJSON_GetObjectItem(j, "data") : NULL;
  if (!d) { if (j) cJSON_Delete(j); return 0; }
  cJSON *out = cJSON_CreateObject();
  cJSON_AddStringToObject(out, "source", "stat.ripe.net");
  cJSON_AddStringToObject(out, "ip", e);
  const char *prefix = jstr(d, "prefix");
  if (prefix) cJSON_AddStringToObject(out, "prefix", prefix);
  cJSON *asns = cJSON_GetObjectItem(d, "asns");
  if (asns) cJSON_AddItemToObject(out, "asns", cJSON_Duplicate(asns, 1));
  /* enrich with the first ASN's holder */
  if (asns && cJSON_IsArray(asns) && cJSON_GetArraySize(asns) > 0) {
    /* A prefix can be announced by several ASNs (multi-origin, or a leak).
     * Resolve the holder of EVERY one, not just the first
     * (docs/SOURCE_EXHAUSTIVENESS.md). */
    cJSON *holders = cJSON_CreateArray();
    cJSON *an;
    cJSON_ArrayForEach(an, asns) {
      if (!cJSON_IsString(an)) continue;
      char res[32]; snprintf(res, sizeof res, "AS%s", an->valuestring);
      cJSON *jo = ripestat(ctx->http, "as-overview", res);
      cJSON *dd = jo ? cJSON_GetObjectItem(jo, "data") : NULL;
      const char *holder = dd ? jstr(dd, "holder") : NULL;
      if (holder) {
        cJSON *h = cJSON_CreateObject();
        cJSON_AddStringToObject(h, "asn", res);
        cJSON_AddStringToObject(h, "holder", holder);
        cJSON_AddItemToArray(holders, h);
        if (!cJSON_GetObjectItem(out, "holder"))
          cJSON_AddStringToObject(out, "holder", holder);   /* headline */
      }
      if (jo) cJSON_Delete(jo);
    }
    if (cJSON_GetArraySize(holders)) cJSON_AddItemToObject(out, "holders", holders);
    else cJSON_Delete(holders);
  }
  int r = emit(sink, e, out);
  cJSON_Delete(out); cJSON_Delete(j);
  return r;
}

static const source_def bgp_def = {
  .id = "BGP_LOOKUP", .collector = "osint", .name = "BGP Lookup", .run = bgp_run,
  .category = "investigation", .type = "api", .url = "https://stat.ripe.net/",
  .description = "IP/ASN → BGP prefix, announcing ASN(s) and holder via RIPEstat (free).",
  .free_tier = 1,
};
REGISTER_SOURCE(bgp_def)
