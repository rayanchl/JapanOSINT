/* collectors/osint/sources/whois_lookup.c
 * OSINT service — port of OSINTsaas osint_tools/whois_lookup.c (whois_lookup_domain
 * → handle_domain_whois). Canonical SERVICE = DOMAIN_WHOIS (dispatcher row
 * {SERVICE_DOMAIN_WHOIS, handle_domain_whois, "DOMAIN_WHOIS", true}). On-demand
 * (interval 0); the OSINT dispatcher runs it with ctx->entity = a domain.
 * Upstream: ICANN RDAP (https://rdap.org/domain/<domain>) — no key. Faithfully
 * reproduces query_rdap_domain(): extract registrar/status/nameservers/events
 * (created=registration, expires=expiration); on RDAP non-200/parse failure the
 * fallback object {domain,error:"RDAP query failed",note:...} with success=false,
 * confidence 30 (vs 95 on success). Emits ONE osint_service_result row like
 * dns_records.c (body = envelope JSON). The dispatcher only resolved domains
 * (handle_domain_whois passes is_valid_domain entities); IP WHOIS path is not
 * reachable as DOMAIN_WHOIS so it is intentionally not reproduced here. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int emit_result(intel_sink *sink, const char *entity, int success,
                        int confidence, cJSON *data /*owned*/, const char *error) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  if (data) cJSON_AddItemToObject(env, "data", data);
  else cJSON_AddNullToObject(env, "data");
  if (error) cJSON_AddStringToObject(env, "error", error);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "DOMAIN_WHOIS");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "whois:%s", entity);
  char title[320];
  snprintf(title, sizeof title, "DOMAIN_WHOIS — %s", entity);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = success ? "WHOIS resolved" : (error ? error : "WHOIS failed");
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"DOMAIN_WHOIS\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *domain = ctx->entity;
  if (!domain || !*domain) return -1;

  char url[512];
  snprintf(url, sizeof url, "https://rdap.org/domain/%s", domain);

  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 15000, 2, &hr);
  cJSON *json = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  long status = hr.status;
  http_response_free(&hr);

  /* query_rdap_domain failure path → whois_lookup_domain fallback */
  if (hc != 0 || status != 200 || !json) {
    if (json) cJSON_Delete(json);
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "domain", domain);
    cJSON_AddStringToObject(d, "error", "RDAP query failed");
    cJSON_AddStringToObject(d, "note", "WHOIS data may be restricted");
    return emit_result(sink, domain, 0, 30, d, "RDAP query failed");
  }

  cJSON *out = cJSON_CreateObject();
  cJSON_AddStringToObject(out, "domain", domain);
  cJSON_AddStringToObject(out, "source", "RDAP");

  const cJSON *registrar = cJSON_GetObjectItem(json, "registrar");
  if (registrar && cJSON_IsString(registrar))
    cJSON_AddStringToObject(out, "registrar", registrar->valuestring);

  const cJSON *st = cJSON_GetObjectItem(json, "status");
  if (st && cJSON_IsArray(st)) {
    cJSON *arr = cJSON_CreateArray();
    int n = cJSON_GetArraySize(st);
    for (int i = 0; i < n && i < 10; i++) {
      const cJSON *item = cJSON_GetArrayItem(st, i);
      if (item && cJSON_IsString(item))
        cJSON_AddItemToArray(arr, cJSON_CreateString(item->valuestring));
    }
    cJSON_AddItemToObject(out, "status", arr);
  }

  const cJSON *ns = cJSON_GetObjectItem(json, "nameservers");
  if (ns && cJSON_IsArray(ns)) {
    cJSON *arr = cJSON_CreateArray();
    int n = cJSON_GetArraySize(ns);
    for (int i = 0; i < n && i < 10; i++) {
      const cJSON *e = cJSON_GetArrayItem(ns, i);
      const cJSON *ldh = e ? cJSON_GetObjectItem(e, "ldhName") : NULL;
      if (ldh && cJSON_IsString(ldh))
        cJSON_AddItemToArray(arr, cJSON_CreateString(ldh->valuestring));
    }
    cJSON_AddItemToObject(out, "nameservers", arr);
  }

  const cJSON *events = cJSON_GetObjectItem(json, "events");
  if (events && cJSON_IsArray(events)) {
    const cJSON *reg = NULL, *exp = NULL;
    int n = cJSON_GetArraySize(events);
    for (int i = 0; i < n; i++) {
      const cJSON *ev = cJSON_GetArrayItem(events, i);
      const cJSON *action = ev ? cJSON_GetObjectItem(ev, "eventAction") : NULL;
      const cJSON *date = ev ? cJSON_GetObjectItem(ev, "eventDate") : NULL;
      if (action && date && cJSON_IsString(action) && cJSON_IsString(date)) {
        if (strcmp(action->valuestring, "registration") == 0) reg = date;
        else if (strcmp(action->valuestring, "expiration") == 0) exp = date;
      }
    }
    if (reg) cJSON_AddStringToObject(out, "created", reg->valuestring);
    if (exp) cJSON_AddStringToObject(out, "expires", exp->valuestring);
  }

  cJSON_Delete(json);
  return emit_result(sink, domain, 1, 95, out, NULL);
}

static const source_def whois_lookup_def = {
  .id = "DOMAIN_WHOIS", .collector = "osint",
  .name = "Domain WHOIS", .name_ja = "ドメインWHOIS",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(whois_lookup_def)
