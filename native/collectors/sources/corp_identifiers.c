/* collectors/osint/sources/corp_identifiers.c — corporate-identifier OSINT,
 * ported from OSINTsaas company_lookup.c (handle_lei_search / handle_vat_validator)
 * onto the JapanOSINT source ABI.
 *
 *   LEI_SEARCH    — company name → GLEIF Legal Entity Identifier records (free)
 *   VAT_VALIDATOR — EU VAT number → EU VIES validation (free)
 *
 * Both hit free public APIs (no key); honest-empty on no match / error. */
#include "../../source.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void urlenc(const char *s, char *out, size_t cap) {
  size_t o = 0;
  for (const char *p = s; *p && o + 4 < cap; p++) {
    unsigned char c = (unsigned char)*p;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out[o++] = (char)c;
    else o += (size_t)snprintf(out + o, cap - o, "%%%02X", c);
  }
  out[o] = 0;
}

static const char *jstr(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

/* ── LEI_SEARCH (GLEIF) ─────────────────────────────────────────────────── */
static int lei_run(const source_ctx *ctx, intel_sink *sink) {
  const char *name = ctx->entity;
  if (!name || !*name) return 0;
  char enc[256]; urlenc(name, enc, sizeof enc);
  char url[512];
  snprintf(url, sizeof url,
    "https://api.gleif.org/api/v1/lei-records?filter[entity.legalName]=%s&page[size]=5", enc);
  http_response hr = {0};
  if (http_request(ctx->http, "GET", url, NULL, NULL, 0, 12000, 2, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  if (!j) return 0;
  cJSON *data = cJSON_GetObjectItem(j, "data");
  int emitted = 0, n = (data && cJSON_IsArray(data)) ? cJSON_GetArraySize(data) : 0;
  for (int i = 0; i < n; i++) {
    cJSON *rec = cJSON_GetArrayItem(data, i);
    cJSON *attr = cJSON_GetObjectItem(rec, "attributes");
    cJSON *ent = attr ? cJSON_GetObjectItem(attr, "entity") : NULL;
    cJSON *ln = ent ? cJSON_GetObjectItem(ent, "legalName") : NULL;
    const char *lei = attr ? jstr(attr, "lei") : NULL;
    const char *legal = ln ? jstr(ln, "name") : NULL;
    if (!lei) continue;

    cJSON *out = cJSON_CreateObject();
    cJSON_AddStringToObject(out, "source", "api.gleif.org");
    cJSON_AddStringToObject(out, "lei", lei);
    if (legal) cJSON_AddStringToObject(out, "legal_name", legal);
    cJSON *addr = ent ? cJSON_GetObjectItem(ent, "legalAddress") : NULL;
    if (addr) {
      const char *country = jstr(addr, "country"), *city = jstr(addr, "city");
      if (country) cJSON_AddStringToObject(out, "country", country);
      if (city) cJSON_AddStringToObject(out, "city", city);
    }
    cJSON *reg = attr ? cJSON_GetObjectItem(attr, "registration") : NULL;
    const char *status = reg ? jstr(reg, "status") : NULL;
    if (status) cJSON_AddStringToObject(out, "registration_status", status);
    char *bj = cJSON_PrintUnformatted(out);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "LEI_SEARCH");
    cJSON_AddStringToObject(props, "entity", name);
    char *pj = cJSON_PrintUnformatted(props);
    char rk[128], title[256];
    snprintf(rk, sizeof rk, "lei:%s", lei);
    snprintf(title, sizeof title, "LEI %s — %s", lei, legal ? legal : name);
    intel_item it = {0};
    it.remote_key = rk; it.title = title; it.body = bj; it.summary = legal ? legal : "LEI record";
    it.record_type = "osint_service_result"; it.properties_json = pj;
    it.tags_json = "[\"osint-search\",\"LEI_SEARCH\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(bj); free(pj); cJSON_Delete(out); cJSON_Delete(props);
  }
  cJSON_Delete(j);
  return emitted;
}

/* ── VAT_VALIDATOR (EU VIES) ────────────────────────────────────────────── */
static int vat_run(const source_ctx *ctx, intel_sink *sink) {
  const char *vat = ctx->entity;
  if (!vat || strlen(vat) < 3) return 0;
  /* split leading 2-letter ISO country from the numeric part */
  char cc[3] = {0}; cc[0] = (char)toupper((unsigned char)vat[0]); cc[1] = (char)toupper((unsigned char)vat[1]);
  if (!isalpha((unsigned char)cc[0]) || !isalpha((unsigned char)cc[1])) return 0;
  char num[64]; size_t o = 0;
  for (const char *p = vat + 2; *p && o < sizeof num - 1; p++)
    if (isalnum((unsigned char)*p)) num[o++] = *p;
  num[o] = 0;
  if (!o) return 0;

  char url[256];
  snprintf(url, sizeof url,
    "https://ec.europa.eu/taxation_customs/vies/rest-api/ms/%s/vat/%s", cc, num);
  http_response hr = {0};
  if (http_request(ctx->http, "GET", url, NULL, NULL, 0, 12000, 2, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  if (!j) return 0;

  cJSON *out = cJSON_CreateObject();
  cJSON_AddStringToObject(out, "source", "ec.europa.eu/vies");
  cJSON_AddStringToObject(out, "country_code", cc);
  cJSON_AddStringToObject(out, "vat_number", num);
  cJSON *valid = cJSON_GetObjectItem(j, "valid");
  int isvalid = valid && cJSON_IsBool(valid) && cJSON_IsTrue(valid);
  cJSON_AddBoolToObject(out, "valid", isvalid);
  const char *nm = jstr(j, "name"), *ad = jstr(j, "address");
  if (nm && *nm) cJSON_AddStringToObject(out, "name", nm);
  if (ad && *ad) cJSON_AddStringToObject(out, "address", ad);
  cJSON_Delete(j);
  char *bj = cJSON_PrintUnformatted(out);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "VAT_VALIDATOR");
  cJSON_AddStringToObject(props, "entity", vat);
  char *pj = cJSON_PrintUnformatted(props);
  char rk[96], title[160];
  snprintf(rk, sizeof rk, "vat:%s%s", cc, num);
  snprintf(title, sizeof title, "VAT %s%s — %s", cc, num, isvalid ? "valid" : "not valid");
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.body = bj; it.summary = title;
  it.record_type = "osint_service_result"; it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"VAT_VALIDATOR\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); cJSON_Delete(out); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static const source_def lei_def = {
  .id = "LEI_SEARCH", .collector = "osint", .name = "LEI Search", .run = lei_run,
  .category = "investigation", .type = "api", .url = "https://api.gleif.org/",
  .description = "Company name → Legal Entity Identifier (LEI) records from GLEIF (free).",
  .free_tier = 1,
};
REGISTER_SOURCE(lei_def)

static const source_def vat_def = {
  .id = "VAT_VALIDATOR", .collector = "osint", .name = "VAT Validator", .run = vat_run,
  .category = "investigation", .type = "api",
  .url = "https://ec.europa.eu/taxation_customs/vies/",
  .description = "EU VAT number validation + registered name/address via VIES (free).",
  .free_tier = 1,
};
REGISTER_SOURCE(vat_def)
