/* collectors/sources/corporate_world2.c
 * OSINT service — OpenCorporates global company search (key-gated), pivot on
 * ctx->entity (a company name). Honest-empty when OPENCORPORATES_API_KEY unset.
 *   OPENCORPORATES  api.opencorporates.com/v0.4/companies/search?q=&api_token= */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *CO_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  if (strcmp(ctx->source_id ? ctx->source_id : "", "OPENCORPORATES") != 0) return 0;
  const char *k = jo_env("OPENCORPORATES_API_KEY");
  if (!k) { fprintf(stderr, "[OPENCORPORATES] no OPENCORPORATES_API_KEY\n"); return 0; }
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[640];
  snprintf(url, sizeof url,
    "https://api.opencorporates.com/v0.4/companies/search?q=%s&per_page=20&api_token=%s", enc, k);
  free(enc);
  const char *hdrs[] = { "Accept: application/json", CO_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "OPENCORPORATES");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *results = cJSON_GetObjectItem(root, "results");
  const cJSON *companies = results ? cJSON_GetObjectItem(results, "companies") : NULL;
  int emitted = 0;
  if (cJSON_IsArray(companies)) {
    const cJSON *wrap;
    cJSON_ArrayForEach(wrap, companies) {
      const cJSON *c = cJSON_GetObjectItem(wrap, "company");
      if (!c) continue;
      const char *name = jo_sv(c, "name");
      const char *num = jo_sv(c, "company_number");
      const char *jur = jo_sv(c, "jurisdiction_code");
      const char *ctype = jo_sv(c, "company_type");
      const char *inc = jo_sv(c, "incorporation_date");
      const char *ocurl = jo_sv(c, "opencorporates_url");
      const char *status = jo_sv(c, "current_status");
      if (!name && !num) continue;
      cJSON *d = cJSON_CreateObject();
      if (name) cJSON_AddStringToObject(d, "name", name);
      if (num) cJSON_AddStringToObject(d, "company_number", num);
      if (jur) cJSON_AddStringToObject(d, "jurisdiction", jur);
      if (ctype) cJSON_AddStringToObject(d, "company_type", ctype);
      if (inc) cJSON_AddStringToObject(d, "incorporation_date", inc);
      if (status) cJSON_AddStringToObject(d, "status", status);
      cJSON_AddStringToObject(d, "source", "OpenCorporates");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "OPENCORPORATES");
      if (jur) cJSON_AddStringToObject(p, "jurisdiction", jur);
      if (num) cJSON_AddStringToObject(p, "company_number", num);
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char summ[160];
      snprintf(summ, sizeof summ, "%s%s%s", jur ? jur : "", (jur && num) ? " · " : "", num ? num : "");
      intel_item it = {0};
      it.remote_key = ocurl ? ocurl : name; it.title = name ? name : num;
      it.summary = summ[0] ? summ : NULL; it.body = bj; it.link = ocurl;
      it.lang = "en"; it.published_at = inc;
      it.record_type = "company";
      it.properties_json = pj; it.tags_json = "[\"osint-search\",\"OPENCORPORATES\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[OPENCORPORATES] emitted %d\n", emitted);
  return 0;
}

static const source_def opencorporates_def = {
  .id = "OPENCORPORATES", .collector = "osint",
  .name = "OpenCorporates Company Search", .name_ja = "OpenCorporates 企業検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://api.opencorporates.com",
  .description = "OpenCorporates (OPENCORPORATES_API_KEY): global company search (number, jurisdiction, type).",
  .layer = NULL, .free_tier = 0,
};
REGISTER_SOURCE(opencorporates_def)
