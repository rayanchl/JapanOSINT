/* collectors/sources/search_world.c
 * OSINT services — key-gated web-search engines, pivot on ctx->entity (query).
 * Honest-empty when the key is unset.
 *
 *   SERPAPI_SEARCH    serpapi.com/search.json?engine=google&q=  SERPAPI_KEY
 *   BING_WEB_SEARCH   api.bing.microsoft.com/v7.0/search?q=     BING_SEARCH_KEY */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *SE_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int emit_result(intel_sink *sink, const char *service, const char *title,
                       const char *link, const char *snippet) {
  if (!title && !link) return 0;
  cJSON *d = cJSON_CreateObject();
  if (title) cJSON_AddStringToObject(d, "title", title);
  if (snippet) cJSON_AddStringToObject(d, "snippet", snippet);
  if (link) cJSON_AddStringToObject(d, "url", link);
  cJSON_AddStringToObject(d, "source", service);
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", service); cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char tags[96]; snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", service);
  intel_item it = {0};
  it.remote_key = link ? link : title; it.title = title ? title : link;
  it.summary = snippet; it.body = bj; it.link = link; it.lang = "en";
  it.record_type = "web-result"; it.properties_json = pj; it.tags_json = tags;
  int rc = sink->emit(sink, &it); free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_serpapi(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("SERPAPI_KEY");
  if (!k) { fprintf(stderr, "[SERPAPI_SEARCH] no SERPAPI_KEY\n"); return 0; }
  char url[640];
  snprintf(url, sizeof url, "https://serpapi.com/search.json?engine=google&num=20&q=%s&api_key=%s", enc, k);
  const char *hdrs[] = { "Accept: application/json", SE_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "SERPAPI_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *org = cJSON_GetObjectItem(root, "organic_results");
  int n = 0;
  if (cJSON_IsArray(org)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, org) {
      n += emit_result(sink, "SERPAPI_SEARCH", jo_sv(r, "title"), jo_sv(r, "link"), jo_sv(r, "snippet"));
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run_bing(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("BING_SEARCH_KEY");
  if (!k) { fprintf(stderr, "[BING_WEB_SEARCH] no BING_SEARCH_KEY\n"); return 0; }
  char url[512];
  snprintf(url, sizeof url, "https://api.bing.microsoft.com/v7.0/search?count=20&q=%s", enc);
  char keyh[160]; snprintf(keyh, sizeof keyh, "Ocp-Apim-Subscription-Key: %s", k);
  const char *hdrs[] = { "Accept: application/json", SE_UA, keyh, NULL };
  char *body = jo_get(ctx, url, hdrs, "BING_WEB_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *web = cJSON_GetObjectItem(root, "webPages");
  const cJSON *values = web ? cJSON_GetObjectItem(web, "value") : NULL;
  int n = 0;
  if (cJSON_IsArray(values)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, values) {
      n += emit_result(sink, "BING_WEB_SEARCH", jo_sv(r, "name"), jo_sv(r, "url"), jo_sv(r, "snippet"));
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "SERPAPI_SEARCH"))  n = run_serpapi(ctx, sink, enc);
  else if (!strcmp(sid, "BING_WEB_SEARCH")) n = run_bing(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define SE_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "investigation", \
    .type = "api", .url = "internal://osint/search", .description = DESC, \
    .layer = NULL, .free_tier = 0 }; \
  REGISTER_SOURCE(sym)

SE_DEF(se_serpapi_def, "SERPAPI_SEARCH", "SerpAPI Google Search",
  "SerpAPI (SERPAPI_KEY): Google organic results (title, link, snippet).");
SE_DEF(se_bing_def, "BING_WEB_SEARCH", "Bing Web Search",
  "Bing Web Search (BING_SEARCH_KEY): web results (name, url, snippet).");
