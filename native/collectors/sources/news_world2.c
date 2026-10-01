/* collectors/sources/news_world2.c
 * OSINT services — key-gated global news search, pivot on ctx->entity (keyword).
 * Honest-empty when the key is unset. Shared article emitter.
 *
 *   NEWSAPI_SEARCH    newsapi.org/v2/everything            NEWSAPI_KEY
 *   GNEWS_SEARCH      gnews.io/api/v4/search               GNEWS_API_KEY
 *   MEDIASTACK_SEARCH api.mediastack.com/v1/news           MEDIASTACK_KEY */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *NW_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int nw_emit_article(intel_sink *sink, const char *service, const char *title,
                           const char *url, const char *desc, const char *pub,
                           const char *src, const char *author) {
  if (!title && !url) return 0;
  cJSON *d = cJSON_CreateObject();
  if (title) cJSON_AddStringToObject(d, "title", title);
  if (src) cJSON_AddStringToObject(d, "outlet", src);
  if (author) cJSON_AddStringToObject(d, "author", author);
  if (desc) cJSON_AddStringToObject(d, "description", desc);
  cJSON_AddStringToObject(d, "source", service);
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", service);
  if (src) cJSON_AddStringToObject(p, "outlet", src);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char tags[96]; snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", service);
  intel_item it = {0};
  it.remote_key = url ? url : title; it.title = title ? title : url;
  it.summary = src; it.body = bj; it.author = author;
  it.link = url; it.lang = "en"; it.published_at = pub;
  it.record_type = "news-article"; it.properties_json = pj; it.tags_json = tags;
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_newsapi(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("NEWSAPI_KEY");
  if (!k) { fprintf(stderr, "[NEWSAPI_SEARCH] no NEWSAPI_KEY\n"); return 0; }
  char url[640];
  snprintf(url, sizeof url,
    "https://newsapi.org/v2/everything?q=%s&pageSize=20&sortBy=publishedAt&language=en&apiKey=%s", enc, k);
  const char *hdrs[] = { "Accept: application/json", NW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "NEWSAPI_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *arts = cJSON_GetObjectItem(root, "articles");
  int n = 0;
  if (cJSON_IsArray(arts)) {
    const cJSON *a;
    cJSON_ArrayForEach(a, arts) {
      const cJSON *srcobj = cJSON_GetObjectItem(a, "source");
      n += nw_emit_article(sink, "NEWSAPI_SEARCH", jo_sv(a, "title"), jo_sv(a, "url"),
                           jo_sv(a, "description"), jo_sv(a, "publishedAt"),
                           srcobj ? jo_sv(srcobj, "name") : NULL, jo_sv(a, "author"));
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run_gnews(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("GNEWS_API_KEY");
  if (!k) { fprintf(stderr, "[GNEWS_SEARCH] no GNEWS_API_KEY\n"); return 0; }
  char url[640];
  snprintf(url, sizeof url, "https://gnews.io/api/v4/search?q=%s&max=20&lang=en&token=%s", enc, k);
  const char *hdrs[] = { "Accept: application/json", NW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "GNEWS_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *arts = cJSON_GetObjectItem(root, "articles");
  int n = 0;
  if (cJSON_IsArray(arts)) {
    const cJSON *a;
    cJSON_ArrayForEach(a, arts) {
      const cJSON *srcobj = cJSON_GetObjectItem(a, "source");
      n += nw_emit_article(sink, "GNEWS_SEARCH", jo_sv(a, "title"), jo_sv(a, "url"),
                           jo_sv(a, "description"), jo_sv(a, "publishedAt"),
                           srcobj ? jo_sv(srcobj, "name") : NULL, NULL);
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run_mediastack(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("MEDIASTACK_KEY");
  if (!k) { fprintf(stderr, "[MEDIASTACK_SEARCH] no MEDIASTACK_KEY\n"); return 0; }
  char url[640];
  snprintf(url, sizeof url,
    "http://api.mediastack.com/v1/news?access_key=%s&keywords=%s&limit=20&languages=en", k, enc);
  const char *hdrs[] = { "Accept: application/json", NW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "MEDIASTACK_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *data = cJSON_GetObjectItem(root, "data");
  int n = 0;
  if (cJSON_IsArray(data)) {
    const cJSON *a;
    cJSON_ArrayForEach(a, data) {
      n += nw_emit_article(sink, "MEDIASTACK_SEARCH", jo_sv(a, "title"), jo_sv(a, "url"),
                           jo_sv(a, "description"), jo_sv(a, "published_at"),
                           jo_sv(a, "source"), jo_sv(a, "author"));
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
  if      (!strcmp(sid, "NEWSAPI_SEARCH"))    n = run_newsapi(ctx, sink, enc);
  else if (!strcmp(sid, "GNEWS_SEARCH"))      n = run_gnews(ctx, sink, enc);
  else if (!strcmp(sid, "MEDIASTACK_SEARCH")) n = run_mediastack(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define NW_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "investigation", \
    .type = "api", .url = "internal://osint/news", .description = DESC, \
    .layer = NULL, .free_tier = 0 }; \
  REGISTER_SOURCE(sym)

NW_DEF(nw_newsapi_def, "NEWSAPI_SEARCH", "NewsAPI Search",
  "NewsAPI (NEWSAPI_KEY): global news article search by keyword.");
NW_DEF(nw_gnews_def, "GNEWS_SEARCH", "GNews Search",
  "GNews (GNEWS_API_KEY): global news search by keyword.");
NW_DEF(nw_mediastack_def, "MEDIASTACK_SEARCH", "Mediastack Search",
  "Mediastack (MEDIASTACK_KEY): global news search by keyword.");
