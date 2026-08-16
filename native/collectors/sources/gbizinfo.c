/* collectors/osint/sources/gbizinfo.c
 * OSINT service — GBIZINFO. On-demand company pivot (entity = company name or
 * 13-digit corporate number) against METI gBizINFO (info.gbiz.go.jp), the
 * authoritative open company database: profile + procurement/subsidy linkage.
 * Free, but requires a (free) API token sent as the X-hojinInfo-api-token
 * header — gated on GBIZINFO_TOKEN (no token → honest empty, like other
 * key-gated collectors). One intel_item per matched company, keyed by
 * corporate_number. No matches / fetch failure → emits nothing.
 *
 * NOTE: field mapping follows the public gBizINFO v1 spec ("hojin-infos" array);
 * verify against a live response once a token is configured. */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* one company → one intel_item. Returns 1 if emitted. */
static int emit_company(intel_sink *sink, const cJSON *c) {
  if (!c) return 0;
  const char *cn   = jo_sv(c, "corporate_number");
  const char *name = jo_sv(c, "name");
  const char *loc  = jo_sv(c, "location");
  if (!name && !cn) return 0;

  const char *kana = jo_sv(c, "kana");
  const char *biz  = jo_sv(c, "business_summary");
  const char *url  = jo_sv(c, "company_url");
  const char *est  = jo_sv(c, "date_of_establishment");
  const char *upd  = jo_sv(c, "update_date");
  const char *post = jo_sv(c, "postal_code");

  cJSON *data = cJSON_CreateObject();
  if (name) cJSON_AddStringToObject(data, "name", name);
  if (cn)   cJSON_AddStringToObject(data, "corporate_number", cn);
  if (kana) cJSON_AddStringToObject(data, "kana", kana);
  if (loc)  cJSON_AddStringToObject(data, "location", loc);
  if (post) cJSON_AddStringToObject(data, "postal_code", post);
  if (est)  cJSON_AddStringToObject(data, "established", est);
  if (biz)  cJSON_AddStringToObject(data, "business_summary", biz);
  if (url)  cJSON_AddStringToObject(data, "company_url", url);
  cJSON_AddStringToObject(data, "source", "gBizINFO");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "GBIZINFO");
  cJSON_AddStringToObject(props, "source", "gBizINFO");
  if (cn)  cJSON_AddStringToObject(props, "corporate_number", cn);
  if (loc) cJSON_AddStringToObject(props, "location", loc);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = cn ? cn : name;
  it.title           = name ? name : cn;
  it.summary         = loc;
  it.body            = bj;
  it.lang            = "ja";
  it.published_at    = est ? est : upd;
  it.link            = url;
  it.record_type     = "gbizinfo-company";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"GBIZINFO\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *token = getenv("GBIZINFO_TOKEN");
  if (!token || !*token) {
    fprintf(stderr, "[gbizinfo] gated (no GBIZINFO_TOKEN)\n");
    return 0;
  }

  /* 13-digit corporate number → detail endpoint; else name search. */
  int alldig = 1, len = 0;
  for (const char *p = q; *p; p++, len++) if (!isdigit((unsigned char)*p)) alldig = 0;
  char url[1024];
  if (alldig && len == 13) {
    snprintf(url, sizeof url, "https://info.gbiz.go.jp/hojin/v1/hojin/%s", q);
  } else {
    char enc[512]; int j = 0;
    for (int i = 0; q[i] && j < 505; i++) {
      unsigned char c = (unsigned char)q[i];
      if (c == ' ') enc[j++] = '+';
      else if (isalnum(c) || c=='-'||c=='_'||c=='.') enc[j++] = (char)c;
      else { snprintf(enc+j, 4, "%%%02X", c); j += 3; }   /* %-encode UTF-8 */
    }
    enc[j] = 0;
    snprintf(url, sizeof url,
      "https://info.gbiz.go.jp/hojin/v1/hojin?name=%s&limit=10", enc);
  }

  char hdr[256];
  snprintf(hdr, sizeof hdr, "X-hojinInfo-api-token: %s", token);
  const char *hdrs[] = { hdr, "Accept: application/json", NULL };

  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 20000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[gbizinfo] http status=%ld\n", hr.status);
    http_response_free(&hr); return 0;
  }
  cJSON *root = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!root) return 0;

  cJSON *infos = cJSON_GetObjectItem(root, "hojin-infos");
  int emitted = 0;
  if (cJSON_IsArray(infos)) {
    cJSON *c; cJSON_ArrayForEach(c, infos) emitted += emit_company(sink, c);
  } else if (cJSON_IsObject(infos)) {
    emitted += emit_company(sink, infos);   /* detail endpoint shape tolerance */
  }
  cJSON_Delete(root);
  fprintf(stderr, "[gbizinfo] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def gbizinfo_def = {
  .id = "GBIZINFO", .collector = "osint",
  .name = "gBizINFO Company Lookup", .name_ja = "gBizINFO 企業情報",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/gbizinfo",
  .description = "METI gBizINFO — Japan company profiles + procurement/subsidy linkage (needs GBIZINFO_TOKEN)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(gbizinfo_def)
