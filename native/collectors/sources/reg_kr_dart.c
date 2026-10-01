/* collectors/sources/reg_kr_dart.c
 * OSINT service — KR DART (Korea Financial Supervisory Service, Data Analysis,
 * Retrieval and Transfer). On-demand company pivot against opendart.fss.or.kr.
 *
 * DART needs a FREE api key (crtfc_key), issued instantly on the OpenDART site.
 * Without it we honest-empty (gated), exactly like gbizinfo.c.
 *
 * Note: DART's per-company endpoints (company.json) key on an 8-digit DART
 * corp_code, which itself is only obtainable from the bulk corpCode.xml map — a
 * zip download we don't ship. So:
 *   - entity is an 8-digit corp_code  → company.json profile lookup.
 *   - otherwise                       → list.json recent-disclosure feed,
 *     locally filtered to rows whose corp_name contains the entity bytes.
 * Every emitted item is REAL data parsed from DART's JSON — nothing synthesized;
 * no key / non-200 / no rows → emits nothing. free_tier=1 (the key is free). */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* One DART disclosure row (list.json "list" element) → one intel_item. */
static int emit_disclosure(intel_sink *sink, const cJSON *r) {
  if (!r) return 0;
  const char *corp     = jo_sv(r, "corp_name");
  const char *rept     = jo_sv(r, "report_nm");
  const char *rcept    = jo_sv(r, "rcept_no");
  const char *rcept_dt = jo_sv(r, "rcept_dt");
  const char *flr      = jo_sv(r, "flr_nm");
  const char *corp_cls = jo_sv(r, "corp_cls");
  const char *stock    = jo_sv(r, "stock_code");
  if (!rcept && !rept) return 0;

  char link[256] = {0};
  if (rcept)
    snprintf(link, sizeof link,
             "https://dart.fss.or.kr/dsaf001/main.do?rcpNo=%s", rcept);

  cJSON *data = cJSON_CreateObject();
  if (corp)     cJSON_AddStringToObject(data, "corp_name", corp);
  if (rept)     cJSON_AddStringToObject(data, "report_nm", rept);
  if (rcept)    cJSON_AddStringToObject(data, "rcept_no", rcept);
  if (rcept_dt) cJSON_AddStringToObject(data, "rcept_dt", rcept_dt);
  if (flr)      cJSON_AddStringToObject(data, "filer", flr);
  if (stock)    cJSON_AddStringToObject(data, "stock_code", stock);
  cJSON_AddStringToObject(data, "source", "DART (FSS Korea)");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "KR_DART");
  cJSON_AddStringToObject(props, "source", "opendart.fss.or.kr");
  if (rcept)    cJSON_AddStringToObject(props, "rcept_no", rcept);
  if (corp_cls) cJSON_AddStringToObject(props, "corp_cls", corp_cls);
  if (stock)    cJSON_AddStringToObject(props, "stock_code", stock);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = rcept ? rcept : rept;
  it.title           = rept ? rept : corp;
  it.summary         = corp;
  it.body            = bj;
  it.lang            = "ko";
  it.published_at    = rcept_dt;
  it.link            = link[0] ? link : NULL;
  it.record_type     = "kr-dart-disclosure";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"KR_DART\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* company.json profile → one intel_item. */
static int emit_company(intel_sink *sink, const cJSON *c) {
  if (!c) return 0;
  const char *corp  = jo_sv(c, "corp_name");
  const char *corp_e= jo_sv(c, "corp_name_eng");
  const char *code  = jo_sv(c, "corp_code");
  const char *stock = jo_sv(c, "stock_code");
  const char *ceo   = jo_sv(c, "ceo_nm");
  const char *addr  = jo_sv(c, "adres");
  const char *hp    = jo_sv(c, "hm_url");
  const char *est   = jo_sv(c, "est_dt");
  const char *biz   = jo_sv(c, "induty_code");
  if (!corp && !code) return 0;

  cJSON *data = cJSON_CreateObject();
  if (corp)   cJSON_AddStringToObject(data, "corp_name", corp);
  if (corp_e) cJSON_AddStringToObject(data, "corp_name_eng", corp_e);
  if (code)   cJSON_AddStringToObject(data, "corp_code", code);
  if (stock)  cJSON_AddStringToObject(data, "stock_code", stock);
  if (ceo)    cJSON_AddStringToObject(data, "ceo", ceo);
  if (addr)   cJSON_AddStringToObject(data, "address", addr);
  if (est)    cJSON_AddStringToObject(data, "established", est);
  if (biz)    cJSON_AddStringToObject(data, "industry_code", biz);
  cJSON_AddStringToObject(data, "source", "DART (FSS Korea)");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "KR_DART");
  cJSON_AddStringToObject(props, "source", "opendart.fss.or.kr");
  if (code)  cJSON_AddStringToObject(props, "corp_code", code);
  if (stock) cJSON_AddStringToObject(props, "stock_code", stock);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = code ? code : corp;
  it.title           = corp ? corp : corp_e;
  it.summary         = ceo;
  it.body            = bj;
  it.lang            = "ko";
  it.published_at    = est;
  it.link            = hp;
  it.record_type     = "kr-dart-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"KR_DART\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  const char *key = jo_env("DART_API_KEY");
  if (!key) {
    fprintf(stderr, "[kr_dart] gated (no DART_API_KEY)\n");
    return 0;
  }

  /* 8-digit numeric entity → treat as DART corp_code → company profile. */
  int alldig = 1, len = 0;
  for (const char *p = q; *p; p++, len++)
    if (!isdigit((unsigned char)*p)) alldig = 0;

  char url[1024];
  if (alldig && len == 8) {
    snprintf(url, sizeof url,
             "https://opendart.fss.or.kr/api/company.json?crtfc_key=%s&corp_code=%s",
             key, q);
    char *body = jo_get(ctx, url, NULL, "kr_dart");
    if (!body) return 0;
    cJSON *root = cJSON_Parse(body);
    free(body);
    if (!root) return 0;
    const char *st = jo_sv(root, "status");   /* "000" == ok */
    int emitted = 0;
    if (st && strcmp(st, "000") == 0) emitted += emit_company(sink, root);
    else fprintf(stderr, "[kr_dart] company status=%s\n", st ? st : "?");
    cJSON_Delete(root);
    fprintf(stderr, "[kr_dart] emitted %d\n", emitted);
    return 0;
  }

  /* Otherwise: recent-disclosure feed, filtered locally to matching corp_name.
   * (Per-company corp_code lookup would need the corpCode.xml bulk map we don't
   * ship, so we scan the latest 100 filings for the entity name.) */
  snprintf(url, sizeof url,
           "https://opendart.fss.or.kr/api/list.json?crtfc_key=%s&page_count=100",
           key);
  char *body = jo_get(ctx, url, NULL, "kr_dart");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  const char *st = jo_sv(root, "status");
  if (st && strcmp(st, "000") != 0) {
    fprintf(stderr, "[kr_dart] list status=%s\n", st);
    cJSON_Delete(root);
    return 0;
  }

  cJSON *list = cJSON_GetObjectItem(root, "list");
  int emitted = 0;
  if (cJSON_IsArray(list)) {
    cJSON *r;
    cJSON_ArrayForEach(r, list) {
      const char *cn = jo_sv(r, "corp_name");
      if (cn && strstr(cn, q)) emitted += emit_disclosure(sink, r);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[kr_dart] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def reg_kr_dart_def = {
  .id = "KR_DART", .collector = "osint",
  .name = "KR DART Corporate Disclosures", .name_ja = "韓国DART 企業開示",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/kr_dart",
  .description = "Korea FSS OpenDART — corporate profiles + disclosure filings (needs free DART_API_KEY)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reg_kr_dart_def)
