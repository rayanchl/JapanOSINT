/* collectors/osint/sources/asn_lookup.c
 * OSINT service — port of net.js asnLookup(). On-demand (interval 0); the
 * OSINT dispatcher runs it with ctx->entity = an IP/host. Upstream:
 * ip-api.com free JSON (no key); fields=as,asname,isp,org,query,status,message
 * (exactly net.js's field list). Mirrors net.js: !r.ok → fail("ip-api
 * <status>"); j.status!=="success" → fail(j.message||"lookup_failed"); else
 * ok({ip:j.query,as:j.as,asname:j.asname,isp:j.isp,org:j.org}, 90).
 * Canonical id "ASN_LOOKUP" (dispatcher.js register(['ASN_LOOKUP',
 * 'BGP_LOOKUP'], asnLookup, 'ASN_LOOKUP') — dedupKey/first name == ASN_LOOKUP).
 * Emits one osint_service_result intel row (body = envelope), like
 * dns_records.c. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
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

/* j.field → string in out, or null in out (faithful to JS object spread where
 * an absent ip-api field is undefined/omitted; cJSON null is the closest
 * JSON-stringify-stable representation and the pipeline reads it leniently). */
static void copy_field(cJSON *out, const char *key, const cJSON *src) {
  const cJSON *v = cJSON_GetObjectItemCaseSensitive(src, key);
  if (cJSON_IsString(v) && v->valuestring)
    cJSON_AddStringToObject(out, key, v->valuestring);
  else if (v && cJSON_IsNumber(v))
    cJSON_AddNumberToObject(out, key, v->valuedouble);
  else
    cJSON_AddNullToObject(out, key);
}

static int emit_result(intel_sink *sink, const char *entity, int success,
                       int confidence, cJSON *data /*owned*/,
                       const char *error) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  if (data) cJSON_AddItemToObject(env, "data", data);
  else cJSON_AddNullToObject(env, "data");
  if (error) cJSON_AddStringToObject(env, "error", error);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "ASN_LOOKUP");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "asn:%s", entity);
  char title[320];
  snprintf(title, sizeof title, "ASN_LOOKUP — %s", entity);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = success ? "ASN resolved" : (error ? error : "lookup failed");
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"ASN_LOOKUP\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return -1;

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

  if (hc != 0 || status < 200 || status >= 300) {
    char err[64];
    snprintf(err, sizeof err, "ip-api %ld", status);
    if (j) cJSON_Delete(j);
    return emit_result(sink, entity, 0, 0, NULL, err);
  }
  if (!j) return emit_result(sink, entity, 0, 0, NULL, "ip-api parse");

  const cJSON *st = cJSON_GetObjectItemCaseSensitive(j, "status");
  if (!cJSON_IsString(st) || strcmp(st->valuestring, "success") != 0) {
    const cJSON *msg = cJSON_GetObjectItemCaseSensitive(j, "message");
    const char *e = (cJSON_IsString(msg) && msg->valuestring && *msg->valuestring)
                      ? msg->valuestring : "lookup_failed";
    int rc = emit_result(sink, entity, 0, 0, NULL, e);
    cJSON_Delete(j);
    return rc;
  }

  /* net.js: ok({ ip:j.query, as:j.as, asname:j.asname, isp:j.isp, org:j.org }, 90) */
  cJSON *data = cJSON_CreateObject();
  {
    const cJSON *qv = cJSON_GetObjectItemCaseSensitive(j, "query");
    if (cJSON_IsString(qv) && qv->valuestring)
      cJSON_AddStringToObject(data, "ip", qv->valuestring);
    else
      cJSON_AddNullToObject(data, "ip");
  }
  copy_field(data, "as", j);
  copy_field(data, "asname", j);
  copy_field(data, "isp", j);
  copy_field(data, "org", j);
  cJSON_Delete(j);
  return emit_result(sink, entity, 1, 90, data, NULL);
}

static const source_def asn_lookup_def = {
  .id = "ASN_LOOKUP", .collector = "osint",
  .name = "ASN Lookup", .name_ja = "ASNルックアップ",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(asn_lookup_def)
