/* collectors/osint/sources/ip_geolocation.c
 * OSINT service — port of net.js ipGeolocation(). On-demand (interval 0); the
 * OSINT dispatcher runs it with ctx->entity = an IP/host. Upstream:
 * ip-api.com free JSON (no key); fields=66846719 (the full field set net.js
 * requests). Mirrors net.js exactly: r.ok gate → fail("ip-api <status>");
 * j.status!=="success" → fail(j.message||"lookup_failed"); else ok(j,95).
 * Emits one osint_service_result intel row whose body is the {success,data,
 * confidence} payload (data = JSON.stringify(j)), like dns_records.c. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* encodeURIComponent: keep A-Za-z0-9 and - _ . ! ~ * ' ( ); %-encode rest.
 * (== net.js encodeURIComponent(entity) on the ip-api path.) */
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

/* Emit the dispatcher's uniform {success,data,confidence(,error)} envelope as
 * one intel row (body carries it; dual_sink captures body for Phase-2). */
static int emit_result(intel_sink *sink, const char *entity, int success,
                       int confidence, cJSON *data /*owned*/,
                       const char *error) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  if (data) cJSON_AddItemToObject(env, "data", data);   /* JSON value (== JS data string parsed) */
  else cJSON_AddNullToObject(env, "data");
  if (error) cJSON_AddStringToObject(env, "error", error);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "IP_GEOLOCATION");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "ipgeo:%s", entity);
  char title[320];
  snprintf(title, sizeof title, "IP_GEOLOCATION — %s", entity);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = success ? "IP geolocated" : (error ? error : "lookup failed");
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"IP_GEOLOCATION\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);            /* frees data too (it was added to env) */
  return rc >= 0 ? 0 : -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return -1;

  char enc[256];
  uri_encode(entity, enc, sizeof enc);
  char url[512];
  snprintf(url, sizeof url,
           "http://ip-api.com/json/%s?fields=66846719", enc);

  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 10000, 2, &hr);
  long status = hr.status;
  cJSON *j = (hc == 0 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);

  /* net.js: if (!r.ok) return fail(`ip-api ${r.status}`); */
  if (hc != 0 || status < 200 || status >= 300) {
    char err[64];
    snprintf(err, sizeof err, "ip-api %ld", status);
    if (j) cJSON_Delete(j);
    return emit_result(sink, entity, 0, 0, NULL, err);
  }
  if (!j) return emit_result(sink, entity, 0, 0, NULL, "ip-api parse");

  /* net.js: if (j.status !== 'success') return fail(j.message||'lookup_failed') */
  const cJSON *st = cJSON_GetObjectItemCaseSensitive(j, "status");
  if (!cJSON_IsString(st) || strcmp(st->valuestring, "success") != 0) {
    const cJSON *msg = cJSON_GetObjectItemCaseSensitive(j, "message");
    const char *e = (cJSON_IsString(msg) && msg->valuestring && *msg->valuestring)
                      ? msg->valuestring : "lookup_failed";
    int rc = emit_result(sink, entity, 0, 0, NULL, e);
    cJSON_Delete(j);
    return rc;
  }

  /* net.js: return ok(j, 95);  (data = full j object) */
  return emit_result(sink, entity, 1, 95, j /*owned by env*/, NULL);
}

static const source_def ip_geolocation_def = {
  .id = "IP_GEOLOCATION", .collector = "osint",
  .name = "IP Geolocation", .name_ja = "IPジオロケーション",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(ip_geolocation_def)
