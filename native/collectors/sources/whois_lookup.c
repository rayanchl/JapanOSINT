/* collectors/osint/sources/whois_lookup.c
 * OSINT service — port of OSINTsaas osint_tools/whois_lookup.c
 * (whois_lookup_domain → handle_domain_whois). Canonical SERVICE =
 * DOMAIN_WHOIS. On-demand (interval 0); the OSINT dispatcher runs it with
 * ctx->entity = a domain. Upstream: ICANN RDAP (https://rdap.org/domain/
 * <domain>) — no key. Faithfully reproduces query_rdap_domain(): extract
 * registrar/status/nameservers/events (created=registration,
 * expires=expiration).
 *
 * PER-RECORD EMIT: single-entity lookup yielding ONE real record. Emits ONE
 * clean intel_item carrying the real RDAP fields directly (registrar, status,
 * nameservers, created, expires) — NOT a {success,confidence,data} envelope.
 * title = "WHOIS <domain>", remote_key = "whois:<domain>". On RDAP non-200 /
 * parse failure → emit NOTHING and return 0 (honest empty). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *domain = ctx->entity;
  if (!domain || !*domain) return 0;

  char url[512];
  snprintf(url, sizeof url, "https://rdap.org/domain/%s", domain);

  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 15000, 2, &hr);
  cJSON *json = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);

  /* query_rdap_domain failure path → emit nothing (honest empty). */
  if (!json) return 0;

  /* Real fetched RDAP fields, emitted directly as the body. */
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

  char *bj = cJSON_PrintUnformatted(out);

  const cJSON *reg_v = cJSON_GetObjectItem(out, "registrar");
  const char *reg_s = (reg_v && cJSON_IsString(reg_v)) ? reg_v->valuestring : NULL;

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "DOMAIN_WHOIS");
  cJSON_AddStringToObject(props, "entity", domain);
  if (reg_s) cJSON_AddStringToObject(props, "registrar", reg_s);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "whois:%s", domain);
  char title[320];
  snprintf(title, sizeof title, "WHOIS %s", domain);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = reg_s ? reg_s : domain;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"DOMAIN_WHOIS\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(out);
  cJSON_Delete(json);
  return rc >= 0 ? 0 : -1;
}

static const source_def whois_lookup_def = {
  .id = "DOMAIN_WHOIS", .collector = "osint",
  .name = "Domain WHOIS", .name_ja = "ドメインWHOIS",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/domain-whois",
  .description = "Fetch domain WHOIS (registrar/NS/dates) via ICANN RDAP.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(whois_lookup_def)
