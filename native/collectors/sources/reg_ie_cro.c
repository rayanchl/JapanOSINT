/* collectors/sources/reg_ie_cro.c
 * OSINT service — IE_CRO. On-demand company pivot (entity = company name)
 * against Ireland's Companies Registration Office open data API
 * (services.cro.ie/cws). One intel_item per matched company: company number,
 * name, status, registered address.
 *
 * Auth: HTTP Basic with the (free) CRO customer API key, base64-encoded and
 * sent as "Authorization: Basic <base64(CRO_API_KEY)>". Gated on CRO_API_KEY
 * (no key → honest empty, exactly like gbizinfo.c). No matches / fetch failure
 * → emits nothing. Only real, parsed API fields are emitted. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* base64 (standard alphabet, padded). malloc'd; caller frees. */
static char *cro_b64(const unsigned char *in, size_t n) {
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

/* Assemble a one-line registered address from the CWS company object. The CRO
 * CWS response carries address_line_1..4; join the non-empty ones. malloc'd
 * (caller frees) or NULL if none present. */
static char *cro_address(const cJSON *c) {
  static const char *keys[] = {
    "company_addr_1", "company_addr_2", "company_addr_3", "company_addr_4",
    "address_line_1", "address_line_2", "address_line_3", "address_line_4",
    "company_address", "address", NULL };
  size_t cap = 256, len = 0;
  char *out = malloc(cap);
  if (!out) return NULL;
  out[0] = 0;
  int n = 0;
  for (int i = 0; keys[i]; i++) {
    const char *v = jo_sv(c, keys[i]);
    if (!v) continue;
    size_t need = len + strlen(v) + 3;
    if (need > cap) { cap = need * 2; char *t = realloc(out, cap); if (!t) break; out = t; }
    if (n) { strcpy(out + len, ", "); len += 2; }
    strcpy(out + len, v); len += strlen(v);
    n++;
  }
  if (!n) { free(out); return NULL; }
  return out;
}

/* one CWS company → one intel_item. Returns 1 if emitted. */
static int emit_company(intel_sink *sink, const cJSON *c) {
  if (!c) return 0;
  /* CWS field names use lowercase-with-underscores; tolerate a couple of
   * plausible casings without inventing data. */
  const char *num  = jo_sv(c, "company_num");
  if (!num) num    = jo_sv(c, "company_number");
  const char *name = jo_sv(c, "company_name");
  if (!name) name  = jo_sv(c, "name");
  if (!num && !name) return 0;

  const char *status = jo_sv(c, "company_status_desc");
  if (!status) status = jo_sv(c, "company_status");
  if (!status) status = jo_sv(c, "status");
  const char *ctype  = jo_sv(c, "comp_type_desc");
  const char *regdate = jo_sv(c, "company_reg_date");
  char *addr = cro_address(c);

  cJSON *data = cJSON_CreateObject();
  if (name)    cJSON_AddStringToObject(data, "name", name);
  if (num)     cJSON_AddStringToObject(data, "company_num", num);
  if (status)  cJSON_AddStringToObject(data, "status", status);
  if (ctype)   cJSON_AddStringToObject(data, "company_type", ctype);
  if (regdate) cJSON_AddStringToObject(data, "registration_date", regdate);
  if (addr)    cJSON_AddStringToObject(data, "address", addr);
  cJSON_AddStringToObject(data, "source", "Ireland CRO");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  char link[256] = {0};
  if (num)
    snprintf(link, sizeof link,
             "https://core.cro.ie/e-commerce/company/%s", num);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "IE_CRO");
  cJSON_AddStringToObject(props, "source", "Ireland CRO");
  if (num)    cJSON_AddStringToObject(props, "company_num", num);
  if (status) cJSON_AddStringToObject(props, "status", status);
  if (addr)   cJSON_AddStringToObject(props, "address", addr);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = num ? num : name;
  it.title           = name ? name : num;
  it.summary         = addr ? addr : status;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = regdate;
  it.link            = link[0] ? link : NULL;
  it.record_type     = "ie-cro-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"IE_CRO\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); free(addr);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *key = jo_env("CRO_API_KEY");
  if (!key) {
    fprintf(stderr, "[ie_cro] gated (no CRO_API_KEY)\n");
    return 0;
  }

  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
           "https://services.cro.ie/cws/companies?company_name=%s&max=15", enc);
  free(enc);

  /* HTTP Basic: base64(CRO_API_KEY) */
  char *b = cro_b64((const unsigned char *)key, strlen(key));
  if (!b) return 0;
  char auth[512];
  snprintf(auth, sizeof auth, "Authorization: Basic %s", b);
  free(b);
  const char *hdrs[] = { auth, "Accept: application/json", NULL };

  char *body = jo_get(ctx, url, hdrs, "ie_cro");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  int emitted = 0;
  /* CWS returns a bare JSON array of company objects; tolerate an object
   * wrapper ({"companies":[...]}) too, without inventing shape. */
  if (cJSON_IsArray(root)) {
    cJSON *c; cJSON_ArrayForEach(c, root) emitted += emit_company(sink, c);
  } else if (cJSON_IsObject(root)) {
    cJSON *arr = cJSON_GetObjectItem(root, "companies");
    if (cJSON_IsArray(arr)) {
      cJSON *c; cJSON_ArrayForEach(c, arr) emitted += emit_company(sink, c);
    } else {
      emitted += emit_company(sink, root);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[ie_cro] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def reg_ie_cro_def = {
  .id = "IE_CRO", .collector = "osint",
  .name = "Ireland CRO Company Search", .name_ja = "アイルランド企業登記 (CRO)",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/reg_ie_cro",
  .description = "Ireland Companies Registration Office open API — company num/name/status/address (needs free CRO_API_KEY)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg_ie_cro_def)
