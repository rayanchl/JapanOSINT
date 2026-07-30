/* collectors/sources/finance_world2.c
 * OSINT services — key-gated market/company symbol search, pivot on ctx->entity
 * (a company name or ticker). Honest-empty when the key is unset.
 *
 *   ALPHAVANTAGE_SEARCH  alphavantage.co/query?function=SYMBOL_SEARCH  ALPHAVANTAGE_KEY
 *   FINNHUB_SEARCH       finnhub.io/api/v1/search?q=                   FINNHUB_KEY */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *FN_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run_alphavantage(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("ALPHAVANTAGE_KEY");
  if (!k) { fprintf(stderr, "[ALPHAVANTAGE_SEARCH] no ALPHAVANTAGE_KEY\n"); return 0; }
  char url[512];
  snprintf(url, sizeof url,
    "https://www.alphavantage.co/query?function=SYMBOL_SEARCH&keywords=%s&apikey=%s", enc, k);
  const char *hdrs[] = { "Accept: application/json", FN_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "ALPHAVANTAGE_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *matches = cJSON_GetObjectItem(root, "bestMatches");
  int emitted = 0;
  if (cJSON_IsArray(matches)) {
    const cJSON *m;
    cJSON_ArrayForEach(m, matches) {
      const char *sym = jo_sv(m, "1. symbol");
      const char *name = jo_sv(m, "2. name");
      const char *type = jo_sv(m, "3. type");
      const char *region = jo_sv(m, "4. region");
      const char *cur = jo_sv(m, "8. currency");
      if (!sym) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "symbol", sym);
      if (name) cJSON_AddStringToObject(d, "name", name);
      if (type) cJSON_AddStringToObject(d, "type", type);
      if (region) cJSON_AddStringToObject(d, "region", region);
      if (cur) cJSON_AddStringToObject(d, "currency", cur);
      cJSON_AddStringToObject(d, "source", "Alpha Vantage");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "ALPHAVANTAGE_SEARCH");
      cJSON_AddStringToObject(p, "symbol", sym);
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char summ[160]; snprintf(summ, sizeof summ, "%s%s%s", sym, region ? " · " : "", region ? region : "");
      intel_item it = {0};
      it.remote_key = sym; it.title = name ? name : sym; it.summary = summ;
      it.body = bj; it.lang = "en";
      it.record_type = "security";
      it.properties_json = pj; it.tags_json = "[\"osint-search\",\"ALPHAVANTAGE_SEARCH\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_finnhub(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("FINNHUB_KEY");
  if (!k) { fprintf(stderr, "[FINNHUB_SEARCH] no FINNHUB_KEY\n"); return 0; }
  char url[512];
  snprintf(url, sizeof url, "https://finnhub.io/api/v1/search?q=%s&token=%s", enc, k);
  const char *hdrs[] = { "Accept: application/json", FN_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "FINNHUB_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *result = cJSON_GetObjectItem(root, "result");
  int emitted = 0;
  if (cJSON_IsArray(result)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, result) {
      const char *sym = jo_sv(r, "symbol");
      const char *desc = jo_sv(r, "description");
      const char *disp = jo_sv(r, "displaySymbol");
      const char *type = jo_sv(r, "type");
      if (!sym) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "symbol", sym);
      if (disp) cJSON_AddStringToObject(d, "display_symbol", disp);
      if (desc) cJSON_AddStringToObject(d, "description", desc);
      if (type) cJSON_AddStringToObject(d, "type", type);
      cJSON_AddStringToObject(d, "source", "Finnhub");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "FINNHUB_SEARCH");
      cJSON_AddStringToObject(p, "symbol", sym);
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      intel_item it = {0};
      it.remote_key = sym; it.title = desc ? desc : sym; it.summary = disp;
      it.body = bj; it.lang = "en";
      it.record_type = "security";
      it.properties_json = pj; it.tags_json = "[\"osint-search\",\"FINNHUB_SEARCH\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
      /* (cap removed: every record the upstream returned is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "ALPHAVANTAGE_SEARCH")) n = run_alphavantage(ctx, sink, enc);
  else if (!strcmp(sid, "FINNHUB_SEARCH"))      n = run_finnhub(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define FN_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "government", \
    .type = "api", .url = "internal://osint/finance", .description = DESC, \
    .layer = NULL, .free_tier = 0 }; \
  REGISTER_SOURCE(sym)

FN_DEF(fn_av_def, "ALPHAVANTAGE_SEARCH", "Alpha Vantage Symbol Search",
  "Alpha Vantage (ALPHAVANTAGE_KEY): ticker/company symbol search (region, currency).");
FN_DEF(fn_finnhub_def, "FINNHUB_SEARCH", "Finnhub Symbol Search",
  "Finnhub (FINNHUB_KEY): symbol/company search (description, type).");
