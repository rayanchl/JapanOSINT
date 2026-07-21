/* collectors/osint/sources/asn_lookup.c
 * OSINT service — port of net.js asnLookup(). On-demand (interval 0); the
 * OSINT dispatcher runs it with ctx->entity = an IP/host. Upstream:
 * ip-api.com free JSON (no key); fields=as,asname,isp,org,query,status,message
 * (exactly net.js's field list). Mirrors net.js: !r.ok → fail; j.status!==
 * "success" → fail; else ok({ip,as,asname,isp,org}).
 * Canonical id "ASN_LOOKUP" (dispatcher.js register(['ASN_LOOKUP',
 * 'BGP_LOOKUP'], asnLookup, 'ASN_LOOKUP')).
 *
 * PER-RECORD EMIT: this is a single-entity lookup yielding ONE real record.
 * Emits ONE clean intel_item carrying the real ip-api fields directly (as,
 * asname, isp, org, query/ip) — NOT a {success,confidence,data} envelope.
 * remote_key = "asn:<ip>". On HTTP failure / status!=success → emit NOTHING
 * and return 0 (honest empty). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* encodeURIComponent: keep A-Za-z0-9 and - _ . ! ~ * ' ( ); %-encode rest. */
static void uri_encode(const char *in, char *out, size_t cap) {
  static const char *keep = "-_.!~*'()";
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && w + 4 < cap; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(keep, c)) {
      out[w++] = (char)c;
    } else {
      snprintf(out + w, cap - w, "%%%02X", c);
      w += 3;
    }
  }
  out[w] = 0;
}

/* j.field → string/number in out, faithful to ip-api's field set. */
static void copy_field(cJSON *out, const char *key, const cJSON *src) {
  const cJSON *v = cJSON_GetObjectItemCaseSensitive(src, key);
  if (cJSON_IsString(v) && v->valuestring)
    cJSON_AddStringToObject(out, key, v->valuestring);
  else if (v && cJSON_IsNumber(v))
    cJSON_AddNumberToObject(out, key, v->valuedouble);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return 0;

  char enc[256];
  uri_encode(entity, enc, sizeof enc);
  char url[512];
  snprintf(url, sizeof url,
           "http://ip-api.com/json/%s?fields=as,asname,isp,org,query,status,message",
           enc);

  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 10000, 2, &hr);
  long status = hr.status;
  cJSON *j = (hc == 0 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);

  /* HTTP failure → honest empty. */
  if (hc != 0 || status < 200 || status >= 300 || !j) {
    if (j) cJSON_Delete(j);
    return 0;
  }

  /* j.status !== "success" → not found → honest empty. */
  const cJSON *st = cJSON_GetObjectItemCaseSensitive(j, "status");
  if (!cJSON_IsString(st) || strcmp(st->valuestring, "success") != 0) {
    cJSON_Delete(j);
    return 0;
  }

  /* Real fetched ip-api fields, emitted directly as the body. */
  cJSON *body = cJSON_CreateObject();
  copy_field(body, "as", j);
  copy_field(body, "asname", j);
  copy_field(body, "isp", j);
  copy_field(body, "org", j);
  copy_field(body, "query", j);
  char *bj = cJSON_PrintUnformatted(body);

  /* Derive AS number + name for the title from j.as ("AS15169 Google LLC")
   * and j.asname. */
  const cJSON *as_v   = cJSON_GetObjectItemCaseSensitive(j, "as");
  const cJSON *asn_v  = cJSON_GetObjectItemCaseSensitive(j, "asname");
  const cJSON *q_v    = cJSON_GetObjectItemCaseSensitive(j, "query");
  const char *as_str  = (cJSON_IsString(as_v) && as_v->valuestring) ? as_v->valuestring : "";
  const char *asn_str = (cJSON_IsString(asn_v) && asn_v->valuestring) ? asn_v->valuestring : "";
  const char *ip_str  = (cJSON_IsString(q_v) && q_v->valuestring) ? q_v->valuestring : entity;

  char title[360];
  if (as_str[0])
    snprintf(title, sizeof title, "%s %s", as_str, asn_str);
  else
    snprintf(title, sizeof title, "ASN %s", ip_str);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "ASN_LOOKUP");
  cJSON_AddStringToObject(props, "entity", entity);
  if (as_str[0])  cJSON_AddStringToObject(props, "as", as_str);
  if (asn_str[0]) cJSON_AddStringToObject(props, "asname", asn_str);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "asn:%s", ip_str);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = asn_str[0] ? asn_str : as_str;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"ASN_LOOKUP\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(body);
  cJSON_Delete(j);
  return rc >= 0 ? 0 : -1;
}

static const source_def asn_lookup_def = {
  .id = "ASN_LOOKUP", .collector = "osint",
  .name = "ASN Lookup", .name_ja = "ASNルックアップ",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/asn-lookup",
  .description = "Resolve ASN/AS name/ISP/org for an IP via ip-api.com.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(asn_lookup_def)
