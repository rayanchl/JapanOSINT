/* collectors/osint/sources/news_archive.c
 * OSINT service — NEWS_ARCHIVE (osint_dispatcher.c service_registry[]:
 * → handle_news_search → news_search). On-demand (interval 0); ctx->entity =
 * a keyword/entity. Sources: GDELT DOC 2.0 (api.gdeltproject.org, no key) +
 * NewsAPI.org (key-gated on NEWSAPI_KEY; query_newsapi returns NULL without
 * the key, reproduced faithfully).
 *
 * PER-RECORD EMIT: emits ONE osint_service_result row per ARTICLE (keyed by
 * the article URL), not a single summary blob. Each row carries the article
 * title, a per-article body, the source/domain as summary, the article date,
 * and the article URL as link. If no articles are returned, emits nothing
 * (honest empty — no seeded rows). The former hardcoded osint_tips array and
 * the synthetic source/country/sentiment tally summary row have been
 * removed. */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static cJSON *query_gdelt(http_client *http, const char *query) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "GDELT Project");
  char *enc = jo_urlencode(query);
  if (!enc) { cJSON_AddStringToObject(result, "error", "Query encoding failed"); return result; }
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.gdeltproject.org/api/v2/doc/doc?query=%s&mode=artlist&maxrecords=25&format=json",
    enc);
  free(enc);

  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    /* GDELT rate-limits hard and intermittently: without this line a throttled
     * run is indistinguishable from "the keyword has no coverage". */
    fprintf(stderr, "[news-archive] GDELT rc=%d http status=%d — 0 articles\n",
            hc, (int)hr.status);
    http_response_free(&hr);
    cJSON_AddStringToObject(result, "error", "API request failed");
    return result;
  }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json) { cJSON_AddStringToObject(result, "error", "Invalid JSON response"); return result; }

  cJSON *out = cJSON_CreateArray();
  cJSON *articles = cJSON_GetObjectItem(json, "articles");
  if (articles && cJSON_IsArray(articles)) {
    int c = cJSON_GetArraySize(articles);
    cJSON_AddNumberToObject(result, "total_results", c);
    for (int i = 0; i < c && i < 25; i++) {
      cJSON *a = cJSON_GetArrayItem(articles, i);
      if (!a) continue;
      cJSON *ao = cJSON_CreateObject();
      cJSON *v;
      if ((v = cJSON_GetObjectItem(a, "title")) && cJSON_IsString(v))
        cJSON_AddStringToObject(ao, "title", v->valuestring);
      if ((v = cJSON_GetObjectItem(a, "url")) && cJSON_IsString(v))
        cJSON_AddStringToObject(ao, "url", v->valuestring);
      if ((v = cJSON_GetObjectItem(a, "domain")) && cJSON_IsString(v))
        cJSON_AddStringToObject(ao, "source", v->valuestring);
      if ((v = cJSON_GetObjectItem(a, "seendate")) && cJSON_IsString(v))
        cJSON_AddStringToObject(ao, "date", v->valuestring);
      if ((v = cJSON_GetObjectItem(a, "language")) && cJSON_IsString(v))
        cJSON_AddStringToObject(ao, "language", v->valuestring);
      cJSON *tone = cJSON_GetObjectItem(a, "tone");
      if (tone && cJSON_IsNumber(tone)) {
        double tv = tone->valuedouble;
        cJSON_AddNumberToObject(ao, "tone_score", tv);
        cJSON_AddStringToObject(ao, "sentiment",
          tv > 1 ? "positive" : (tv < -1 ? "negative" : "neutral"));
      }
      if ((v = cJSON_GetObjectItem(a, "sourcecountry")) && cJSON_IsString(v))
        cJSON_AddStringToObject(ao, "source_country", v->valuestring);
      cJSON_AddItemToArray(out, ao);
    }
  }
  cJSON_AddItemToObject(result, "articles", out);
  cJSON_Delete(json);
  return result;
}

/* query_newsapi: key-gated on NEWSAPI_KEY → NULL without it. */
static cJSON *query_newsapi(http_client *http, const char *query) {
  const char *api_key = getenv("NEWSAPI_KEY");
  if (!api_key || !*api_key) return NULL;

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "NewsAPI.org");
  char *enc = jo_urlencode(query);
  if (!enc) { cJSON_AddStringToObject(result, "error", "Query encoding failed"); return result; }
  char url[1024];
  snprintf(url, sizeof url,
    "https://newsapi.org/v2/everything?q=%s&sortBy=publishedAt&pageSize=20&apiKey=%s",
    enc, api_key);
  free(enc);

  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    http_response_free(&hr);
    cJSON_AddStringToObject(result, "error", "API request failed");
    return result;
  }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json) { cJSON_AddStringToObject(result, "error", "Invalid JSON response"); return result; }

  cJSON *status = cJSON_GetObjectItem(json, "status");
  if (!status || !cJSON_IsString(status) || strcmp(status->valuestring, "ok") != 0) {
    cJSON_AddStringToObject(result, "error", "API returned error status");
    cJSON_Delete(json);
    return result;
  }
  cJSON *out = cJSON_CreateArray();
  cJSON *articles = cJSON_GetObjectItem(json, "articles");
  if (articles && cJSON_IsArray(articles)) {
    cJSON *total = cJSON_GetObjectItem(json, "totalResults");
    if (total && cJSON_IsNumber(total))
      cJSON_AddNumberToObject(result, "total_results", total->valuedouble);
    int c = cJSON_GetArraySize(articles);
    for (int i = 0; i < c; i++) {
      cJSON *a = cJSON_GetArrayItem(articles, i);
      if (!a) continue;
      cJSON *ao = cJSON_CreateObject();
      cJSON *v;
      if ((v = cJSON_GetObjectItem(a, "title")) && cJSON_IsString(v))
        cJSON_AddStringToObject(ao, "title", v->valuestring);
      if ((v = cJSON_GetObjectItem(a, "description")) && cJSON_IsString(v))
        cJSON_AddStringToObject(ao, "description", v->valuestring);
      if ((v = cJSON_GetObjectItem(a, "url")) && cJSON_IsString(v))
        cJSON_AddStringToObject(ao, "url", v->valuestring);
      cJSON *src = cJSON_GetObjectItem(a, "source");
      if (src) {
        cJSON *nm = cJSON_GetObjectItem(src, "name");
        if (nm && cJSON_IsString(nm))
          cJSON_AddStringToObject(ao, "source", nm->valuestring);
      }
      if ((v = cJSON_GetObjectItem(a, "publishedAt")) && cJSON_IsString(v))
        cJSON_AddStringToObject(ao, "date", v->valuestring);
      if ((v = cJSON_GetObjectItem(a, "author")) && cJSON_IsString(v) && *v->valuestring)
        cJSON_AddStringToObject(ao, "author", v->valuestring);
      cJSON_AddItemToArray(out, ao);
    }
  }
  cJSON_AddItemToObject(result, "articles", out);
  cJSON_Delete(json);
  return result;
}

/* Emit one intel row for a single article object. Returns 1 if emitted.
 * `a` carries fields built by query_gdelt / query_newsapi: title, url,
 * source, date, sentiment, source_country, etc. `tags` and `svc` identify
 * the calling service. */
static int emit_article(intel_sink *sink, const char *svc, const char *query,
                        const char *tags, cJSON *a) {
  if (!a) return 0;
  cJSON *t   = cJSON_GetObjectItem(a, "title");
  cJSON *u   = cJSON_GetObjectItem(a, "url");
  cJSON *src = cJSON_GetObjectItem(a, "source");
  cJSON *dt  = cJSON_GetObjectItem(a, "date");
  cJSON *se  = cJSON_GetObjectItem(a, "sentiment");
  cJSON *cn  = cJSON_GetObjectItem(a, "source_country");

  const char *url_s = (u && cJSON_IsString(u)) ? u->valuestring : NULL;
  if (!url_s) return 0;   /* no stable key → skip (never fabricate) */
  const char *title_s = (t && cJSON_IsString(t)) ? t->valuestring : NULL;
  const char *src_s   = (src && cJSON_IsString(src)) ? src->valuestring : NULL;
  const char *date_s  = (dt && cJSON_IsString(dt)) ? dt->valuestring : NULL;

  /* Per-article body. */
  cJSON *data = cJSON_CreateObject();
  if (title_s) cJSON_AddStringToObject(data, "title", title_s);
  cJSON_AddStringToObject(data, "url", url_s);
  if (src_s) cJSON_AddStringToObject(data, "source", src_s);
  if (date_s) cJSON_AddStringToObject(data, "date", date_s);
  if (se && cJSON_IsString(se)) cJSON_AddStringToObject(data, "sentiment", se->valuestring);
  if (cn && cJSON_IsString(cn)) cJSON_AddStringToObject(data, "country", cn->valuestring);
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", svc);
  cJSON_AddStringToObject(props, "entity", query);
  if (src_s) cJSON_AddStringToObject(props, "source", src_s);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[1100];
  snprintf(rk, sizeof rk, "article:%s", url_s);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title_s ? title_s : url_s;
  it.body            = bj;
  it.summary         = src_s ? src_s : "news article";
  it.published_at    = date_s;
  it.link            = url_s;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = tags;
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int emit_articles(intel_sink *sink, const char *svc, const char *query,
                         const char *tags, cJSON *src_obj) {
  int n = 0;
  if (!src_obj) return 0;
  cJSON *arr = cJSON_GetObjectItem(src_obj, "articles");
  if (arr && cJSON_IsArray(arr)) {
    int c = cJSON_GetArraySize(arr);
    for (int i = 0; i < c; i++)
      n += emit_article(sink, svc, query, tags, cJSON_GetArrayItem(arr, i));
  }
  return n;
}

static int run_svc(const source_ctx *ctx, intel_sink *sink, const char *svc) {
  const char *query = ctx->entity;
  if (!query || !*query) return -1;

  char tags[96];
  snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", svc);

  cJSON *gdelt   = query_gdelt(ctx->http, query);
  cJSON *newsapi = query_newsapi(ctx->http, query);   /* NULL without key */

  int emitted = 0;
  emitted += emit_articles(sink, svc, query, tags, gdelt);
  emitted += emit_articles(sink, svc, query, tags, newsapi);

  cJSON_Delete(gdelt);
  if (newsapi) cJSON_Delete(newsapi);

  (void)emitted;
  return 0;   /* honest empty is not an error */
}

static int run_archive(const source_ctx *ctx, intel_sink *sink) {
  return run_svc(ctx, sink, "NEWS_ARCHIVE");
}

static const source_def news_archive_def = {
  .id = "NEWS_ARCHIVE", .collector = "osint",
  .name = "News Archive", .name_ja = "ニュースアーカイブ",
  .update_interval_sec = 0, .run = run_archive,
  .category = "news", .type = "api",
  .url = "internal://osint/news-archive",
  .description = "Search archived/older news articles for a query.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(news_archive_def)
