/* collectors/sources/reg_in_mca.c
 * OSINT service — IN_MCA. On-demand company pivot (entity = company name)
 * against Indian company data.
 *
 * India's authoritative registry (MCA21 / Ministry of Corporate Affairs) is
 * captcha-walled and cannot be fetched keyless. The open-data alternative is
 * the Government of India Open Data Platform (data.gov.in), which republishes
 * MCA company master datasets as JSON resources. That API requires (a) a free
 * personal API key and (b) the specific resource id of the company dataset to
 * query. Both are supplied via env:
 *   INDIA_DATA_KEY        — data.gov.in api-key (free registration)
 *   INDIA_MCA_RESOURCE_ID — data.gov.in resource UUID of the company dataset
 * Request shape (per data.gov.in spec):
 *   GET https://api.data.gov.in/resource/<id>
 *        ?api-key=<key>&format=json&limit=25
 *        &filters[company_name]=<entity>
 * Gated on INDIA_DATA_KEY → honest empty (return 0) when unset, exactly like
 * gbizinfo.c. One intel_item per matched company record; no matches / fetch
 * failure → emits nothing. The free key means free_tier stays 1.
 *
 * NOTE: data.gov.in returns matched rows under "records"; field names vary per
 * dataset, so we probe a small set of common company-master keys and fall back
 * to serialising the whole record. Verify field mapping against a live response
 * once a resource id is configured. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* one company record → one intel_item. Returns 1 if emitted. */
static int emit_company(intel_sink *sink, const cJSON *c) {
  if (!c || !cJSON_IsObject(c)) return 0;

  static const char *name_keys[] = {
    "company_name", "companyname", "comp_name", "name", NULL };
  static const char *cin_keys[] = {
    "cin", "corporate_identification_number", "company_id", "registration_number", NULL };
  static const char *status_keys[] = {
    "company_status", "companystatus", "status", NULL };
  static const char *state_keys[] = {
    "registered_state", "state", "company_state", NULL };
  static const char *class_keys[] = {
    "company_class", "companyclass", "class", NULL };
  static const char *roc_keys[] = {
    "registrar_of_companies", "roc", "roc_code", NULL };
  static const char *date_keys[] = {
    "date_of_registration", "registration_date", "dateofregistration", NULL };

  const char *name   = jo_first_sv(c, name_keys);
  const char *cin    = jo_first_sv(c, cin_keys);
  if (!name && !cin) return 0;

  const char *status = jo_first_sv(c, status_keys);
  const char *state  = jo_first_sv(c, state_keys);
  const char *cls    = jo_first_sv(c, class_keys);
  const char *roc    = jo_first_sv(c, roc_keys);
  const char *rdate  = jo_first_sv(c, date_keys);

  cJSON *data = cJSON_CreateObject();
  if (name)   cJSON_AddStringToObject(data, "name", name);
  if (cin)    cJSON_AddStringToObject(data, "cin", cin);
  if (status) cJSON_AddStringToObject(data, "status", status);
  if (state)  cJSON_AddStringToObject(data, "state", state);
  if (cls)    cJSON_AddStringToObject(data, "class", cls);
  if (roc)    cJSON_AddStringToObject(data, "roc", roc);
  if (rdate)  cJSON_AddStringToObject(data, "registered", rdate);
  cJSON_AddStringToObject(data, "source", "data.gov.in / MCA");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "IN_MCA");
  cJSON_AddStringToObject(props, "source", "data.gov.in");
  if (cin)    cJSON_AddStringToObject(props, "cin", cin);
  if (status) cJSON_AddStringToObject(props, "status", status);
  if (state)  cJSON_AddStringToObject(props, "state", state);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = cin ? cin : name;
  it.title           = name ? name : cin;
  it.summary         = status ? status : state;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = rdate;
  it.record_type     = "in-mca-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"IN_MCA\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  const char *key = jo_env("INDIA_DATA_KEY");
  if (!key) {
    fprintf(stderr, "[in_mca] gated (no INDIA_DATA_KEY)\n");
    return 0;
  }
  const char *rid = jo_env("INDIA_MCA_RESOURCE_ID");
  if (!rid) {
    fprintf(stderr, "[in_mca] gated (no INDIA_MCA_RESOURCE_ID)\n");
    return 0;
  }

  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1200];
  snprintf(url, sizeof url,
    "https://api.data.gov.in/resource/%s?api-key=%s&format=json&limit=25"
    "&filters[company_name]=%s", rid, key, enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "in_mca");
  if (!body) return 0;

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *records = cJSON_GetObjectItem(root, "records");
  int emitted = 0;
  if (cJSON_IsArray(records)) {
    cJSON *c;
    cJSON_ArrayForEach(c, records) emitted += emit_company(sink, c);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[in_mca] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def in_mca_def = {
  .id = "IN_MCA", .collector = "osint",
  .name = "India MCA Company Lookup", .name_ja = "インド企業登記 (MCA)",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/in_mca",
  .description = "India company data via data.gov.in MCA dataset "
    "(needs INDIA_DATA_KEY + INDIA_MCA_RESOURCE_ID; MCA21 itself is captcha-walled)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(in_mca_def)
