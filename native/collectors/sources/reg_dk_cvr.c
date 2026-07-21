/* collectors/sources/reg_dk_cvr.c
 * OSINT service — DK_CVR. On-demand company pivot (entity = company name, VAT
 * number, or address) against Denmark's CVR (Central Business Register) via the
 * free cvrapi.dk endpoint. Keyless LIVE — the provider only asks for a
 * descriptive User-Agent. One intel_item per matched company, keyed by VAT.
 * A lookup that returns an error object (e.g. "NOT_FOUND") or no company →
 * honest empty (emits nothing, returns 0). Nothing is fabricated: every field
 * comes straight from the parsed API response.
 *
 * API: GET https://cvrapi.dk/api?search=<entity>&country=dk  (JSON object). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* build a single-line address from the CVR object's address components. */
static char *dk_address(const cJSON *c) {
  const char *street = jo_sv(c, "address");
  const char *zip    = jo_sv(c, "zipcode");
  const char *city   = jo_sv(c, "city");
  if (!street && !zip && !city) return NULL;
  char buf[512];
  snprintf(buf, sizeof buf, "%s%s%s%s%s",
           street ? street : "",
           street && (zip || city) ? ", " : "",
           zip ? zip : "",
           zip && city ? " " : "",
           city ? city : "");
  return strdup(buf);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return -1;
  char url[1024];
  snprintf(url, sizeof url,
           "https://cvrapi.dk/api?search=%s&country=dk", enc);
  free(enc);

  const char *hdrs[] = {
    "User-Agent: JapanOSINT - contact",
    "Accept: application/json",
    NULL
  };
  char *body = jo_get(ctx, url, hdrs, "dk_cvr");
  if (!body) return 0;

  cJSON *c = cJSON_Parse(body);
  free(body);
  if (!c) return 0;

  /* error responses carry an "error" field (e.g. NOT_FOUND); vat marks a hit. */
  const char *err = jo_sv(c, "error");
  const cJSON *vatv = cJSON_GetObjectItem(c, "vat");
  const char *name = jo_sv(c, "name");
  if (err || (!vatv && !name)) {
    if (err) fprintf(stderr, "[dk_cvr] error=%s\n", err);
    cJSON_Delete(c);
    return 0;
  }

  char vat[32] = {0};
  if (cJSON_IsNumber(vatv)) snprintf(vat, sizeof vat, "%.0f", vatv->valuedouble);
  else if (cJSON_IsString(vatv) && vatv->valuestring) snprintf(vat, sizeof vat, "%s", vatv->valuestring);

  const char *industrycode = jo_sv(c, "industrycode");
  const char *industrydesc = jo_sv(c, "industrydesc");
  const char *companydesc  = jo_sv(c, "companydesc");
  const char *startdate    = jo_sv(c, "startdate");
  const char *email        = jo_sv(c, "email");
  const char *phone        = NULL;
  { const cJSON *p = cJSON_GetObjectItem(c, "phone");
    if (cJSON_IsString(p)) phone = p->valuestring; }
  char *addr = dk_address(c);

  const cJSON *empv = cJSON_GetObjectItem(c, "employees");
  char employees[32] = {0};
  if (cJSON_IsNumber(empv)) snprintf(employees, sizeof employees, "%.0f", empv->valuedouble);
  else if (cJSON_IsString(empv) && empv->valuestring) snprintf(employees, sizeof employees, "%s", empv->valuestring);

  cJSON *data = cJSON_CreateObject();
  if (name)          cJSON_AddStringToObject(data, "name", name);
  if (vat[0])        cJSON_AddStringToObject(data, "vat", vat);
  if (addr)          cJSON_AddStringToObject(data, "address", addr);
  if (industrycode)  cJSON_AddStringToObject(data, "industry_code", industrycode);
  if (industrydesc)  cJSON_AddStringToObject(data, "industry", industrydesc);
  if (companydesc)   cJSON_AddStringToObject(data, "company_type", companydesc);
  if (employees[0])  cJSON_AddStringToObject(data, "employees", employees);
  if (email)         cJSON_AddStringToObject(data, "email", email);
  if (phone)         cJSON_AddStringToObject(data, "phone", phone);
  if (startdate)     cJSON_AddStringToObject(data, "started", startdate);
  cJSON_AddStringToObject(data, "country", "DK");
  cJSON_AddStringToObject(data, "source", "cvrapi.dk / CVR");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "DK_CVR");
  cJSON_AddStringToObject(props, "source", "CVR (Denmark)");
  if (vat[0])       cJSON_AddStringToObject(props, "vat", vat);
  if (industrydesc) cJSON_AddStringToObject(props, "industry", industrydesc);
  if (employees[0]) cJSON_AddStringToObject(props, "employees", employees);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = vat[0] ? vat : name;
  it.title           = name ? name : vat;
  it.summary         = addr;
  it.body            = bj;
  it.lang            = "da";
  it.published_at    = startdate;
  it.record_type     = "cvr-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"DK_CVR\"]";
  int emitted = sink->emit(sink, &it) >= 0 ? 1 : 0;

  free(bj); free(pj); free(addr);
  cJSON_Delete(c);
  fprintf(stderr, "[dk_cvr] emitted %d\n", emitted);
  return 0;
}

static const source_def dk_cvr_def = {
  .id = "DK_CVR", .collector = "osint",
  .name = "Denmark CVR Company Lookup", .name_ja = "デンマーク企業登記 (CVR)",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/dk_cvr",
  .description = "Denmark CVR (Central Business Register) via cvrapi.dk — company name, VAT, address, industry, employees (keyless)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(dk_cvr_def)
