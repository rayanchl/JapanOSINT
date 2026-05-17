/* collectors/osint/sources/news_archive.c
 * OSINT service — faithful port of OSINTsaas osint_tools/news_aggregator.c
 * (handle_news_search → news_search). Canonical service NEWS_ARCHIVE
 * (osint_dispatcher.c service_registry[]: → handle_news_search
 * → news_search). On-demand (interval 0); ctx->entity = a
 * keyword/entity. Sources: GDELT DOC 2.0 (api.gdeltproject.org, no key) +
 * NewsAPI.org (key-gated on NEWSAPI_KEY; OSINTsaas query_newsapi returns NULL
 * without the key → newsapi_status note, reproduced faithfully). Reproduces
 * query_gdelt / query_newsapi / analyze_sources article+sentiment tally /
 * osint_tips verbatim; success=true, confidence 85. Emits ONE
 * osint_service_result row; body = {success,confidence,data} envelope, like
 * ip_geolocation.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* url_encode (== OSINTsaas common.c url_encode: %-encode all but
 * unreserved A-Za-z0-9 -_.~). caller frees. */
static char *url_encode(const char *s) {
  static const char *unres = "-_.~";
  size_t n = strlen(s);
  char *out = malloc(n * 3 + 1);
  if (!out) return NULL;
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(unres, c))
      out[w++] = (char)c;
    else { sprintf(out + w, "%%%02X", c); w += 3; }
  }
  out[w] = 0;
  return out;
}

static cJSON *query_gdelt(http_client *http, const char *query) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "GDELT Project");
  char *enc = url_encode(query);
  if (!enc) { cJSON_AddStringToObject(result, "error", "Query encoding failed"); return result; }
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.gdeltproject.org/api/v2/doc/doc?query=%s&mode=artlist&maxrecords=25&format=json",
    enc);
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
  char *enc = url_encode(query);
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

/* analyze_sources: source/country counts + sentiment distribution. */
static cJSON *analyze_sources(cJSON *articles) {
  cJSON *stats = cJSON_CreateObject();
  cJSON *sc = cJSON_CreateObject();
  cJSON *cc = cJSON_CreateObject();
  int pos = 0, neg = 0, neu = 0;
  if (articles && cJSON_IsArray(articles)) {
    int c = cJSON_GetArraySize(articles);
    for (int i = 0; i < c; i++) {
      cJSON *a = cJSON_GetArrayItem(articles, i);
      if (!a) continue;
      cJSON *src = cJSON_GetObjectItem(a, "source");
      if (src && cJSON_IsString(src)) {
        cJSON *ex = cJSON_GetObjectItem(sc, src->valuestring);
        if (ex) cJSON_SetNumberValue(ex, ex->valuedouble + 1);
        else cJSON_AddNumberToObject(sc, src->valuestring, 1);
      }
      cJSON *cn = cJSON_GetObjectItem(a, "source_country");
      if (cn && cJSON_IsString(cn)) {
        cJSON *ex = cJSON_GetObjectItem(cc, cn->valuestring);
        if (ex) cJSON_SetNumberValue(ex, ex->valuedouble + 1);
        else cJSON_AddNumberToObject(cc, cn->valuestring, 1);
      }
      cJSON *se = cJSON_GetObjectItem(a, "sentiment");
      if (se && cJSON_IsString(se)) {
        if (strcmp(se->valuestring, "positive") == 0) pos++;
        else if (strcmp(se->valuestring, "negative") == 0) neg++;
        else neu++;
      }
    }
  }
  cJSON_AddItemToObject(stats, "sources", sc);
  cJSON_AddItemToObject(stats, "countries", cc);
  cJSON *ss = cJSON_CreateObject();
  cJSON_AddNumberToObject(ss, "positive", pos);
  cJSON_AddNumberToObject(ss, "negative", neg);
  cJSON_AddNumberToObject(ss, "neutral", neu);
  cJSON_AddItemToObject(stats, "sentiment_distribution", ss);
  return stats;
}

static int run_svc(const source_ctx *ctx, intel_sink *sink, const char *svc) {
  const char *query = ctx->entity;
  if (!query || !*query) return -1;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", query);
  cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));

  cJSON *gdelt = query_gdelt(ctx->http, query);
  cJSON_AddItemToObject(root, "gdelt", gdelt);

  cJSON *newsapi = query_newsapi(ctx->http, query);
  if (newsapi) cJSON_AddItemToObject(root, "newsapi", newsapi);
  else cJSON_AddStringToObject(root, "newsapi_status",
    "API key not configured (set NEWSAPI_KEY environment variable)");

  cJSON *all = cJSON_CreateArray();
  cJSON *ga = cJSON_GetObjectItem(gdelt, "articles");
  if (ga && cJSON_IsArray(ga)) {
    int c = cJSON_GetArraySize(ga);
    for (int i = 0; i < c; i++) {
      cJSON *art = cJSON_Duplicate(cJSON_GetArrayItem(ga, i), 1);
      if (art) { cJSON_AddStringToObject(art, "api_source", "GDELT");
                 cJSON_AddItemToArray(all, art); }
    }
  }
  if (newsapi) {
    cJSON *na = cJSON_GetObjectItem(newsapi, "articles");
    if (na && cJSON_IsArray(na)) {
      int c = cJSON_GetArraySize(na);
      for (int i = 0; i < c; i++) {
        cJSON *art = cJSON_Duplicate(cJSON_GetArrayItem(na, i), 1);
        if (art) { cJSON_AddStringToObject(art, "api_source", "NewsAPI");
                   cJSON_AddItemToArray(all, art); }
      }
    }
  }
  cJSON *analysis = analyze_sources(all);
  cJSON_AddItemToObject(root, "analysis", analysis);
  cJSON_AddNumberToObject(root, "total_articles", cJSON_GetArraySize(all));
  cJSON_Delete(all);

  cJSON *tips = cJSON_CreateArray();
  cJSON_AddItemToArray(tips, cJSON_CreateString("Cross-reference news with official sources"));
  cJSON_AddItemToArray(tips, cJSON_CreateString("Check article dates for currency of information"));
  cJSON_AddItemToArray(tips, cJSON_CreateString("Consider source bias and reputation"));
  cJSON_AddItemToArray(tips, cJSON_CreateString("Look for primary sources cited in articles"));
  cJSON_AddItemToObject(root, "osint_tips", tips);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);          /* OSINTsaas: always true */
  cJSON_AddNumberToObject(env, "confidence", 85);
  cJSON_AddItemToObject(env, "data", root);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", svc);
  cJSON_AddStringToObject(props, "entity", query);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 85);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[320];
  snprintf(rk, sizeof rk, "%s:%s", svc, query);
  char title[360];
  snprintf(title, sizeof title, "%s — %s", svc, query);
  char tags[96];
  snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", svc);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = "news aggregated";
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = tags;
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run_archive(const source_ctx *ctx, intel_sink *sink) {
  return run_svc(ctx, sink, "NEWS_ARCHIVE");
}

static const source_def news_archive_def = {
  .id = "NEWS_ARCHIVE", .collector = "osint",
  .name = "News Archive", .name_ja = "ニュースアーカイブ",
  .update_interval_sec = 0, .run = run_archive,
};
REGISTER_SOURCE(news_archive_def)
