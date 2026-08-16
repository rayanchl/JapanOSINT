/* collectors/sources/reg_il_data.c
 * OSINT service — IL_COMPANIES. On-demand entity pivot against the Israeli
 * companies register, published OPEN via the data.gov.il CKAN datastore
 * (Ministry of Justice / Corporations Authority open dataset). Keyless and
 * LIVE — no API key required.
 *
 * For any entity we query the CKAN datastore_search action with a free-text
 * q=<entity> against the companies resource and emit one intel_item per
 * matched record: company number, name, status and address.
 *
 * Only real, parsed API fields are emitted; fetch failure / no matches →
 * honest empty (return 0). */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* companies dataset resource on data.gov.il */
#define IL_RESOURCE_ID "f004176c-b85f-4542-8901-7b3176f9a054"

/* cJSON value → string, tolerating string OR number fields (CKAN datastore
 * returns some columns as numbers). malloc'd (caller frees) or NULL. */
static char *il_field(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return NULL;
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0])
    return strdup(v->valuestring);
  if (cJSON_IsNumber(v)) {
    char buf[64];
    double d = v->valuedouble;
    if (d == (double)(long long)d)
      snprintf(buf, sizeof buf, "%lld", (long long)d);
    else
      snprintf(buf, sizeof buf, "%g", d);
    return strdup(buf);
  }
  return NULL;
}

/* first non-NULL string field among several candidate column keys.
 * malloc'd (caller frees) or NULL. */
static char *il_first(const cJSON *o, const char *const *keys) {
  for (int i = 0; keys[i]; i++) {
    char *v = il_field(o, keys[i]);
    if (v) return v;
  }
  return NULL;
}

/* one record → one intel_item. Returns 1 if emitted. */
static int emit_company(intel_sink *sink, const cJSON *r) {
  if (!cJSON_IsObject(r)) return 0;

  /* the dataset uses Hebrew-keyed columns; match on the known column names. */
  static const char *k_num[]  = { "\xd7\x9e\xd7\xa1\xd7\xa4\xd7\xa8 \xd7\x97\xd7\x91\xd7\xa8\xd7\x94", /* מספר חברה */
                                  "company_number", "Company_Number", NULL };
  static const char *k_name[] = { "\xd7\xa9\xd7\x9d \xd7\x97\xd7\x91\xd7\xa8\xd7\x94", /* שם חברה */
                                  "company_name", "Company_Name", NULL };
  static const char *k_stat[] = { "\xd7\xa1\xd7\x98\xd7\x98\xd7\x95\xd7\xa1 \xd7\x97\xd7\x91\xd7\xa8\xd7\x94", /* סטטוס חברה */
                                  "status", "company_status", NULL };
  static const char *k_addr[] = { "\xd7\xa8\xd7\x97\xd7\x95\xd7\x91", /* רחוב */
                                  "\xd7\x9b\xd7\xaa\xd7\x95\xd7\x91\xd7\xaa", /* כתובת */
                                  "address", NULL };
  static const char *k_city[] = { "\xd7\xa2\xd7\x99\xd7\xa8", /* עיר */
                                  "city", NULL };

  char *num  = il_first(r, k_num);
  char *name = il_first(r, k_name);
  char *stat = il_first(r, k_stat);
  char *addr = il_first(r, k_addr);
  char *city = il_first(r, k_city);

  if (!name && !num) {
    free(num); free(name); free(stat); free(addr); free(city);
    return 0;
  }

  /* compose a human address string when both street and city are present. */
  char *full_addr = NULL;
  if (addr && city) {
    size_t n = strlen(addr) + strlen(city) + 3;
    full_addr = (char *)malloc(n);
    if (full_addr) snprintf(full_addr, n, "%s, %s", addr, city);
  }
  const char *addr_show = full_addr ? full_addr : (addr ? addr : city);

  cJSON *data = cJSON_CreateObject();
  if (name)      cJSON_AddStringToObject(data, "name", name);
  if (num)       cJSON_AddStringToObject(data, "company_number", num);
  if (stat)      cJSON_AddStringToObject(data, "status", stat);
  if (addr_show) cJSON_AddStringToObject(data, "address", addr_show);
  if (city)      cJSON_AddStringToObject(data, "city", city);
  cJSON_AddStringToObject(data, "source", "data.gov.il");
  cJSON_AddStringToObject(data, "country", "IL");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "IL_COMPANIES");
  cJSON_AddStringToObject(props, "source", "data.gov.il");
  cJSON_AddStringToObject(props, "country", "IL");
  if (num)  cJSON_AddStringToObject(props, "company_number", num);
  if (stat) cJSON_AddStringToObject(props, "status", stat);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = num ? num : name;
  it.title           = name ? name : num;
  it.summary         = addr_show ? addr_show : stat;
  it.body            = bj;
  it.lang            = "he";
  it.record_type     = "il-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"IL_COMPANIES\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  free(num); free(name); free(stat); free(addr); free(city); free(full_addr);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  char url[1024];
  snprintf(url, sizeof url,
    "https://data.gov.il/api/3/action/datastore_search?resource_id=%s&q=%s&limit=25",
    IL_RESOURCE_ID, enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "il_companies");
  if (!body) return 0;

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  const cJSON *ok = cJSON_GetObjectItem(root, "success");
  const cJSON *result = cJSON_GetObjectItem(root, "result");
  int emitted = 0;
  if (ok && cJSON_IsBool(ok) && cJSON_IsTrue(ok) && result) {
    const cJSON *records = cJSON_GetObjectItem(result, "records");
    if (cJSON_IsArray(records)) {
      const cJSON *rec;
      cJSON_ArrayForEach(rec, records) emitted += emit_company(sink, rec);
    }
  }
  cJSON_Delete(root);

  fprintf(stderr, "[il_companies] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def il_companies_def = {
  .id = "IL_COMPANIES", .collector = "osint",
  .name = "Israel Companies Register", .name_ja = "\xe3\x82\xa4\xe3\x82\xb9\xe3\x83\xa9\xe3\x82\xa8\xe3\x83\xab\xe4\xbc\x81\xe6\xa5\xad\xe7\x99\xbb\xe8\xa8\x98",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://data.gov.il/api/3/action/datastore_search",
  .description = "Israel companies register via data.gov.il CKAN (keyless): company number, name, status, address",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(il_companies_def)
