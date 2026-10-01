/* collectors/sources/reg_cz_ares.c
 * OSINT service — CZ_ARES. On-demand entity pivot against the Czech ARES
 * (Administrativní registr ekonomických subjektů) OPEN JSON REST API
 * (ares.gov.cz), the authoritative open registry of Czech economic subjects.
 * Keyless and LIVE.
 *
 * For any entity we POST {"obchodniJmeno":"<entity>"} to
 *   /ekonomicke-subjekty-v-be/rest/ekonomicke-subjekty/vyhledat
 * and emit one intel_item per matched subject: IČO, name, address, legal form.
 * If the entity is an 8-digit IČO we GET the detail endpoint instead.
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

/* Compose a human-readable address from the ARES `sidlo` object, if present. */
static char *ares_address(const cJSON *subj) {
  const cJSON *s = cJSON_GetObjectItem(subj, "sidlo");
  if (!cJSON_IsObject(s)) return NULL;
  /* ARES provides a pre-formatted textovaAdresa; prefer it. */
  const char *txt = jo_sv(s, "textovaAdresa");
  if (txt) return strdup(txt);
  return NULL;
}

/* one economic subject → one intel_item. Returns 1 if emitted. */
static int emit_subject(intel_sink *sink, const cJSON *subj) {
  if (!subj) return 0;
  const char *ico  = jo_sv(subj, "ico");
  const char *name = jo_sv(subj, "obchodniJmeno");
  if (!ico && !name) return 0;

  const char *pf   = jo_sv(subj, "pravniForma");   /* legal-form code */
  const char *fin  = jo_sv(subj, "financniUrad");
  const char *datv = jo_sv(subj, "datumVzniku");   /* date of establishment */
  char *addr = ares_address(subj);

  cJSON *data = cJSON_CreateObject();
  if (name) cJSON_AddStringToObject(data, "name", name);
  if (ico)  cJSON_AddStringToObject(data, "ico", ico);
  if (addr) cJSON_AddStringToObject(data, "address", addr);
  if (pf)   cJSON_AddStringToObject(data, "legal_form", pf);
  if (fin)  cJSON_AddStringToObject(data, "tax_office", fin);
  if (datv) cJSON_AddStringToObject(data, "established", datv);
  cJSON_AddStringToObject(data, "source", "ARES");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "CZ_ARES");
  cJSON_AddStringToObject(props, "source", "ARES");
  cJSON_AddStringToObject(props, "country", "CZ");
  if (ico) cJSON_AddStringToObject(props, "ico", ico);
  if (pf)  cJSON_AddStringToObject(props, "legal_form", pf);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char link[256];
  if (ico) snprintf(link, sizeof link,
    "https://ares.gov.cz/ekonomicke-subjekty/res/%s", ico);
  else link[0] = 0;

  intel_item it = {0};
  it.remote_key      = ico ? ico : name;
  it.title           = name ? name : ico;
  it.summary         = addr;
  it.body            = bj;
  it.lang            = "cs";
  it.published_at    = datv;
  it.link            = ico ? link : NULL;
  it.record_type     = "ares-subject";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"CZ_ARES\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); free(addr);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  /* 8-digit numeric → treat as IČO and hit the detail endpoint. */
  int alldig = 1, len = 0;
  for (const char *p = q; *p; p++, len++)
    if (!isdigit((unsigned char)*p)) alldig = 0;

  const char *hdrs[] = { "Accept: application/json",
                         "Content-Type: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  int emitted = 0;

  if (alldig && len == 8) {
    char url[256];
    snprintf(url, sizeof url,
      "https://ares.gov.cz/ekonomicke-subjekty-v-be/rest/ekonomicke-subjekty/%s", q);
    char *body = jo_get(ctx, url, hdrs, "cz_ares");
    if (!body) return 0;
    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return 0;
    emitted += emit_subject(sink, root);   /* detail = single subject object */
    cJSON_Delete(root);
    fprintf(stderr, "[cz_ares] emitted %d\n", emitted);
    return 0;
  }

  /* name search — POST JSON body {"obchodniJmeno":"<entity>","pocet":20}. */
  cJSON *req = cJSON_CreateObject();
  cJSON_AddStringToObject(req, "obchodniJmeno", q);
  cJSON_AddNumberToObject(req, "pocet", 20);
  char *reqjson = cJSON_PrintUnformatted(req);
  cJSON_Delete(req);
  if (!reqjson) return 0;

  const char *url =
    "https://ares.gov.cz/ekonomicke-subjekty-v-be/rest/ekonomicke-subjekty/vyhledat";
  http_response hr = {0};
  int rc = http_request(ctx->http, "POST", url, hdrs,
                        reqjson, strlen(reqjson), 20000, 1, &hr);
  free(reqjson);
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[cz_ares] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return 0;
  }
  cJSON *root = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!root) return 0;

  cJSON *arr = cJSON_GetObjectItem(root, "ekonomickeSubjekty");
  if (cJSON_IsArray(arr)) {
    cJSON *s;
    cJSON_ArrayForEach(s, arr) emitted += emit_subject(sink, s);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[cz_ares] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def cz_ares_def = {
  .id = "CZ_ARES", .collector = "osint",
  .name = "Czech ARES Business Registry", .name_ja = "チェコ ARES 企業登記",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://ares.gov.cz",
  .description = "Czech ARES economic-subjects registry (keyless): IČO, name, address, legal form",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cz_ares_def)
