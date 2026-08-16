/* collectors/sources/reg_uk_companies.c
 * OSINT service — UK Companies House company search. On-demand company pivot
 * (entity = company name or number) against the UK statutory company register
 * (api.company-information.service.gov.uk). One intel_item per matched company:
 * name, company number, status, registered address.
 *
 * Auth: HTTP Basic with username = COMPANIES_HOUSE_API_KEY, empty password —
 * i.e. base64("<key>:"). The key is FREE (self-service registration), so this
 * stays free_tier=1; gated on COMPANIES_HOUSE_API_KEY (no key → honest empty,
 * exactly like gbizinfo.c). No matches / fetch failure → emits nothing. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* base64 (standard alphabet, padded). malloc'd; caller frees. */
static char *uk_b64(const unsigned char *in, size_t n) {
  static const char *T =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char *out = malloc(((n + 2) / 3) * 4 + 1);
  if (!out) return NULL;
  size_t o = 0;
  for (size_t i = 0; i < n; i += 3) {
    unsigned v = in[i] << 16;
    if (i + 1 < n) v |= in[i + 1] << 8;
    if (i + 2 < n) v |= in[i + 2];
    out[o++] = T[(v >> 18) & 63];
    out[o++] = T[(v >> 12) & 63];
    out[o++] = (i + 1 < n) ? T[(v >> 6) & 63] : '=';
    out[o++] = (i + 2 < n) ? T[v & 63] : '=';
  }
  out[o] = 0;
  return out;
}

/* one search-hit → one intel_item. Returns 1 if emitted. */
static int emit_company(intel_sink *sink, const cJSON *c) {
  if (!c) return 0;
  const char *title  = jo_sv(c, "title");
  const char *number = jo_sv(c, "company_number");
  if (!title && !number) return 0;

  const char *status = jo_sv(c, "company_status");
  const char *ctype  = jo_sv(c, "company_type");
  const char *created = jo_sv(c, "date_of_creation");
  const char *snippet = jo_sv(c, "address_snippet");

  const cJSON *addr = cJSON_GetObjectItem(c, "address");
  const char *line1 = jo_sv(addr, "address_line_1");
  const char *loc   = jo_sv(addr, "locality");
  const char *pcode = jo_sv(addr, "postal_code");
  const char *ctry  = jo_sv(addr, "country");

  cJSON *data = cJSON_CreateObject();
  if (title)   cJSON_AddStringToObject(data, "name", title);
  if (number)  cJSON_AddStringToObject(data, "company_number", number);
  if (status)  cJSON_AddStringToObject(data, "status", status);
  if (ctype)   cJSON_AddStringToObject(data, "company_type", ctype);
  if (created) cJSON_AddStringToObject(data, "date_of_creation", created);
  if (snippet) cJSON_AddStringToObject(data, "address", snippet);
  else {
    if (line1) cJSON_AddStringToObject(data, "address_line_1", line1);
    if (loc)   cJSON_AddStringToObject(data, "locality", loc);
    if (pcode) cJSON_AddStringToObject(data, "postal_code", pcode);
    if (ctry)  cJSON_AddStringToObject(data, "country", ctry);
  }
  cJSON_AddStringToObject(data, "source", "UK Companies House");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  char link[256] = {0};
  if (number)
    snprintf(link, sizeof link,
             "https://find-and-update.company-information.service.gov.uk/company/%s",
             number);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "UK_COMPANIES");
  cJSON_AddStringToObject(props, "source", "UK Companies House");
  if (number) cJSON_AddStringToObject(props, "company_number", number);
  if (status) cJSON_AddStringToObject(props, "status", status);
  if (snippet) cJSON_AddStringToObject(props, "address", snippet);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = number ? number : title;
  it.title           = title ? title : number;
  it.summary         = snippet ? snippet : status;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = created;
  it.link            = link[0] ? link : NULL;
  it.record_type     = "uk-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"UK_COMPANIES\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *key = jo_env("COMPANIES_HOUSE_API_KEY");
  if (!key) {
    fprintf(stderr, "[uk_companies] gated (no COMPANIES_HOUSE_API_KEY)\n");
    return 0;
  }

  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
           "https://api.company-information.service.gov.uk/search/companies?q=%s&items_per_page=20",
           enc);
  free(enc);

  /* HTTP Basic: username=key, empty password → base64("key:") */
  char raw[256];
  snprintf(raw, sizeof raw, "%s:", key);
  char *b = uk_b64((const unsigned char *)raw, strlen(raw));
  if (!b) return 0;
  char auth[400];
  snprintf(auth, sizeof auth, "Authorization: Basic %s", b);
  free(b);
  const char *hdrs[] = { auth, "Accept: application/json", NULL };

  char *body = jo_get(ctx, url, hdrs, "uk_companies");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  int emitted = 0;
  cJSON *items = cJSON_GetObjectItem(root, "items");
  if (cJSON_IsArray(items)) {
    cJSON *c; cJSON_ArrayForEach(c, items) emitted += emit_company(sink, c);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[uk_companies] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def reg_uk_companies_def = {
  .id = "UK_COMPANIES", .collector = "osint",
  .name = "UK Companies House", .name_ja = "英国会社登記",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/reg_uk_companies",
  .description = "UK Companies House statutory register — company name/number/status/address (needs free COMPANIES_HOUSE_API_KEY)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg_uk_companies_def)
