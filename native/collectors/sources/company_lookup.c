/* collectors/osint/sources/company_lookup.c
 * OSINT service — COMPANY_LOOKUP. Canonical SERVICE id in osint_dispatcher.c:
 * {SERVICE_COMPANY_LOOKUP, handle_company_lookup, "COMPANY_LOOKUP", true}
 * (alias OPENCORPORATES_SEARCH shares the SAME handler; primary registered =
 * COMPANY_LOOKUP). LEI_SEARCH / VAT_VALIDATOR are bound to different handlers →
 * not this file. Entity = company name. No API key (OpenCorporates v0.4
 * public).
 *
 * PER-RECORD EMIT: queries OpenCorporates v0.4 companies/search and emits ONE
 * intel_item per company in the results, keyed by jurisdiction+company_number.
 * Each row's body carries the real fetched fields (name, jurisdiction, number,
 * status, type, incorporation date, address). No companies / fetch failure →
 * emits nothing (honest empty — no fabricated registry-name stubs). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Emit one intel row for a single OpenCorporates company object. Returns 1 if
 * emitted. */
static int emit_company(intel_sink *sink, const cJSON *c) {
  if (!c) return 0;
  const cJSON *nm = cJSON_GetObjectItem(c, "name");
  const cJSON *ju = cJSON_GetObjectItem(c, "jurisdiction_code");
  const cJSON *cn = cJSON_GetObjectItem(c, "company_number");
  const cJSON *st = cJSON_GetObjectItem(c, "current_status");
  const cJSON *ct = cJSON_GetObjectItem(c, "company_type");
  const cJSON *id = cJSON_GetObjectItem(c, "incorporation_date");
  const cJSON *ad = cJSON_GetObjectItem(c, "registered_address_in_full");
  const cJSON *url = cJSON_GetObjectItem(c, "opencorporates_url");

  const char *name = (nm && cJSON_IsString(nm)) ? nm->valuestring : NULL;
  const char *jur  = (ju && cJSON_IsString(ju)) ? ju->valuestring : NULL;
  const char *num  = (cn && cJSON_IsString(cn)) ? cn->valuestring : NULL;

  /* Real per-company body. */
  cJSON *data = cJSON_CreateObject();
  if (name) cJSON_AddStringToObject(data, "name", name);
  if (jur)  cJSON_AddStringToObject(data, "jurisdiction", jur);
  if (num)  cJSON_AddStringToObject(data, "company_number", num);
  if (st && cJSON_IsString(st)) cJSON_AddStringToObject(data, "status", st->valuestring);
  if (ct && cJSON_IsString(ct)) cJSON_AddStringToObject(data, "type", ct->valuestring);
  if (id && cJSON_IsString(id)) cJSON_AddStringToObject(data, "incorporated", id->valuestring);
  if (ad && cJSON_IsString(ad)) cJSON_AddStringToObject(data, "address", ad->valuestring);
  cJSON_AddStringToObject(data, "source", "OpenCorporates");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "COMPANY_LOOKUP");
  cJSON_AddStringToObject(props, "source", "OpenCorporates");
  if (st && cJSON_IsString(st)) cJSON_AddStringToObject(props, "status", st->valuestring);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  /* Stable per-company key = jurisdiction:company_number. */
  char rk[320];
  snprintf(rk, sizeof rk, "company:%s:%s",
           jur ? jur : "?", num ? num : (name ? name : "?"));

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = name ? name : "company";
  it.body            = bj;
  it.summary         = name ? name : "company";
  it.link            = (url && cJSON_IsString(url)) ? url->valuestring : NULL;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"COMPANY_LOOKUP\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char enc[512]; int j = 0;
  for (int i = 0; q[i] && j < 511; i++)
    enc[j++] = q[i] == ' ' ? '+' : q[i];
  enc[j] = 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.opencorporates.com/v0.4/companies/search?q=%s&per_page=10",
    enc);

  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    /* 2026-07: OpenCorporates closed the keyless v0.4 tier — the search
     * endpoint answers 401 {"error":{"message":"Invalid Api Token…"}}. Say so
     * instead of returning a silent empty that looks like "no such company". */
    fprintf(stderr, "[company-lookup] OpenCorporates http status=%d "
                    "(v0.4 now requires an api_token) — 0 rows\n", (int)hr.status);
    http_response_free(&hr); return 0;
  }
  cJSON *root = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!root) return 0;

  cJSON *ro = cJSON_GetObjectItem(root, "results");
  cJSON *comps = ro ? cJSON_GetObjectItem(ro, "companies") : NULL;
  if (!comps || !cJSON_IsArray(comps)) { cJSON_Delete(root); return 0; }

  int emitted = 0;
  int n = cJSON_GetArraySize(comps);
  for (int i = 0; i < n && i < 20; i++) {
    cJSON *cw = cJSON_GetArrayItem(comps, i);
    cJSON *c = cw ? cJSON_GetObjectItem(cw, "company") : NULL;
    if (c) emitted += emit_company(sink, c);
  }
  cJSON_Delete(root);
  (void)emitted;
  return 0;   /* honest empty is not an error */
}

static const source_def company_lookup_def = {
  .id = "COMPANY_LOOKUP", .collector = "osint",
  .name = "Company Lookup", .name_ja = "企業検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/company-lookup",
  .description = "Queries OpenCorporates for company registration records",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(company_lookup_def)
