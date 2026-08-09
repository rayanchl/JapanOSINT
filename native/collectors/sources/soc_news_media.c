/* soc_news_media.c — news search, sector news and feed/podcast discovery.
 *
 *  GUARDIAN_CONTENT_SEARCH (entity pivot: keyword)
 *      GET https://content.guardianapis.com/search?q=&api-key=&page-size=20
 *      records under response.results[]; &show-fields=trailText,byline gives
 *      the snippet fields we emit.
 *      emits: id, type, sectionId, sectionName, pillarName, webPublicationDate,
 *             webTitle, webUrl, apiUrl, trailText, byline
 *      keys: the literal api-key=test is the publisher's own documented
 *            anonymous tier, so this works with no registration; a
 *            GUARDIAN_API_KEY env var is honoured when set (R6).
 *      LICENCE RESTRICTION: Guardian Open Platform free tier is NON-COMMERCIAL
 *      (developer tier). Read the Open Platform terms before any commercial use.
 *  spaceflight-news-api (scheduled) GET https://api.spaceflightnewsapi.net/v4/articles/
 *      DRF envelope {count, next, previous, results[]}.
 *      emits: id, title, url, image_url, news_site, summary, published_at,
 *             updated_at, authors[].name
 *      licence: free, keyless, open-source project (github.com/TheSpaceDevs).
 *  FEEDLY_FEED_SEARCH (entity pivot: keyword or domain)
 *      GET https://cloud.feedly.com/v3/search/feeds?query=&count=20
 *      emits: feedId, the subscribable feed URL (feedId minus its "feed/"
 *             prefix), title, website, description, language, topics,
 *             subscribers, velocity (posts/week), lastUpdated, created
 *      licence: the endpoint Feedly's own web search box uses; answers
 *               unauthenticated and returns feed metadata only, no article
 *               bodies.
 *  ITUNES_PODCAST_SEARCH (entity pivot: keyword)
 *      GET https://itunes.apple.com/search?term=&entity=podcast&limit=20
 *      TRAP: served as text/javascript with leading blank lines — fetch as
 *      text and parse, do not require a JSON content-type.
 *      emits: collectionId, collectionName, artistName, feedUrl,
 *             collectionViewUrl, artworkUrl600, genres, trackCount,
 *             releaseDate, country
 *      licence: Apple's public Search API; ~20 calls/min, attribution to Apple
 *               required on display.
 *  GPODDER_SEARCH (entity pivot: keyword)
 *      GET https://gpodder.net/search.json?q=  — bare array; 'url' IS the feed.
 *      emits: url (RSS), title, author, description, subscribers,
 *             subscribers_last_week, logo_url, website, mygpo_link
 *      licence: gPodder.net is AGPL open-source infrastructure with a
 *               documented public API; keyless.
 *
 * No coordinates in any of these responses → has_geo stays 0 (R2).
 */
#include "../../lib/osintemit.h"
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const SOC_UA[] = {
  "User-Agent: JapanOSINT-native/1.0 (OSINT research collector; +https://github.com/)",
  "Accept: application/json", NULL
};

/* ---- GUARDIAN_CONTENT_SEARCH -------------------------------------------- */

static int run_guardian(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  const char *key = getenv("GUARDIAN_API_KEY");
  if (!key || !*key) key = "test";   /* the publisher's documented open tier */
  char *enc = soc_urlenc(q);
  char *kenc = soc_urlenc(key);
  if (!enc || !kenc) { free(enc); free(kenc); return 0; }
  char url[700];
  snprintf(url, sizeof url,
           "https://content.guardianapis.com/search?q=%s&api-key=%s"
           "&page-size=20&show-fields=trailText%%2Cbyline", enc, kenc);
  free(enc); free(kenc);

  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
  if (!doc) { fprintf(stderr, "[GUARDIAN_CONTENT_SEARCH] no response for %s\n", q); return 0; }
  const cJSON *resp = cJSON_GetObjectItem(doc, "response");
  const cJSON *res = cJSON_IsObject(resp) ? cJSON_GetObjectItem(resp, "results") : NULL;
  int n = 0;
  const cJSON *r;
  if (cJSON_IsArray(res)) cJSON_ArrayForEach(r, res) {
    const char *id = jo_sv(r, "id");
    const char *wt = jo_sv(r, "webTitle");
    if (!id || !wt) continue;
    const cJSON *f = cJSON_GetObjectItem(r, "fields");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "GUARDIAN_CONTENT_SEARCH");
    cJSON_AddStringToObject(p, "source", "content.guardianapis.com");
    cJSON_AddStringToObject(p, "entity", q);
    jcopy(p, "id", r, "id");
    jcopy(p, "type", r, "type");
    jcopy(p, "section_id", r, "sectionId");
    jcopy(p, "section_name", r, "sectionName");
    jcopy(p, "pillar_name", r, "pillarName");
    jcopy(p, "web_publication_date", r, "webPublicationDate");
    jcopy(p, "web_title", r, "webTitle");
    jcopy(p, "web_url", r, "webUrl");
    jcopy(p, "api_url", r, "apiUrl");
    if (cJSON_IsObject(f)) {
      jcopy(p, "trail_text", f, "trailText");
      jcopy(p, "byline", f, "byline");
    }
    cJSON_AddBoolToObject(p, "success", 1);
    char summary[400];
    snprintf(summary, sizeof summary, "%s · %s%s%.200s",
             jo_sv(r, "sectionName") ? jo_sv(r, "sectionName") : "",
             jo_sv(r, "webPublicationDate") ? jo_sv(r, "webPublicationDate") : "",
             (cJSON_IsObject(f) && jo_sv(f, "byline")) ? " · " : "",
             (cJSON_IsObject(f) && jo_sv(f, "byline")) ? jo_sv(f, "byline") : "");
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"GUARDIAN_CONTENT_SEARCH\",\"news\"]",
                  id, wt, summary, jo_sv(r, "webUrl"),
                  jo_sv(r, "webPublicationDate"), "en");
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[GUARDIAN_CONTENT_SEARCH] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_guardian_def = {
  .id = "GUARDIAN_CONTENT_SEARCH", .collector = "osint",
  .name = "Guardian Open Platform Content Search",
  .update_interval_sec = 0, .run = run_guardian,
  .category = "media", .type = "api",
  .url = "https://content.guardianapis.com/search",
  .description = "Full-text search over the Guardian's article archive with section, pillar, byline and publication-date metadata — searchable back through the whole corpus, not just the last 20 items.",
  .license = "Guardian Open Platform. api-key=test is the publisher's documented anonymous developer tier; GUARDIAN_API_KEY honoured if set. NON-COMMERCIAL developer tier — review the Open Platform terms before commercial use.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_guardian_def)

/* ---- spaceflight-news-api ----------------------------------------------- */

static int run_spaceflight(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
      "https://api.spaceflightnewsapi.net/v4/articles/?limit=20", SOC_UA, 25000);
  if (!doc) { fprintf(stderr, "[spaceflight-news-api] fetch failed\n"); return -1; }
  const cJSON *res = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(res)) {
    fprintf(stderr, "[spaceflight-news-api] unexpected shape\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *a;
  cJSON_ArrayForEach(a, res) {
    const char *title = jo_sv(a, "title");
    const char *url   = jo_sv(a, "url");
    if (!title || !url) continue;
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "spaceflightnewsapi.net");
    jcopy(p, "id", a, "id");
    jcopy(p, "title", a, "title");
    jcopy(p, "url", a, "url");
    jcopy(p, "image_url", a, "image_url");
    jcopy(p, "news_site", a, "news_site");
    jcopy(p, "summary", a, "summary");
    jcopy(p, "published_at", a, "published_at");
    jcopy(p, "updated_at", a, "updated_at");
    /* authors[] is an array of objects carrying a name */
    cJSON *au = cJSON_CreateArray();
    const cJSON *authors = cJSON_GetObjectItem(a, "authors"), *x;
    if (au && cJSON_IsArray(authors)) cJSON_ArrayForEach(x, authors) {
      const char *nm = jo_sv(x, "name");
      if (nm) cJSON_AddItemToArray(au, cJSON_CreateString(nm));
    }
    if (au) cJSON_AddItemToObject(p, "authors", au);
    char rk[64], summary[400];
    const cJSON *idv = cJSON_GetObjectItem(a, "id");
    snprintf(rk, sizeof rk, "sfn-%ld",
             cJSON_IsNumber(idv) ? (long)idv->valuedouble : 0L);
    snprintf(summary, sizeof summary, "%s · %.300s",
             jo_sv(a, "news_site") ? jo_sv(a, "news_site") : "",
             jo_sv(a, "summary") ? jo_sv(a, "summary") : "");
    n += soc_emit(sink, p, "space-news",
                  "[\"spaceflight\",\"news\",\"launch\"]",
                  rk, title, summary, url, jo_sv(a, "published_at"), "en");
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[spaceflight-news-api] emitted %d\n", n);
  return 0;
}

static const source_def soc_spaceflight_def = {
  .id = "spaceflight-news-api", .collector = "social",
  .name = "Spaceflight News API",
  .update_interval_sec = 3600, .run = run_spaceflight,
  .category = "media", .type = "api",
  .url = "https://api.spaceflightnewsapi.net/v4/articles/",
  .description = "Normalised aggregation of articles from every major space-and-launch outlet with publisher attribution, author and summary — a pre-deduplicated sector news corpus.",
  .license = "Free, keyless, open-source project (github.com/TheSpaceDevs), offered for reuse.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_spaceflight_def)

/* ---- FEEDLY_FEED_SEARCH -------------------------------------------------- */

static int run_feedly(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[400];
  snprintf(url, sizeof url,
           "https://cloud.feedly.com/v3/search/feeds?query=%s&count=20", enc);
  free(enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
  if (!doc) { fprintf(stderr, "[FEEDLY_FEED_SEARCH] no response for %s\n", q); return 0; }
  const cJSON *res = cJSON_GetObjectItem(doc, "results");
  int n = 0;
  const cJSON *r;
  if (cJSON_IsArray(res)) cJSON_ArrayForEach(r, res) {
    const char *fid = jo_sv(r, "feedId");
    const char *title = jo_sv(r, "title");
    if (!fid || !title) continue;
    /* feedId is "feed/<absolute url>" — strip the prefix to get a
     * subscribable endpoint. */
    const char *feed_url = (strncmp(fid, "feed/", 5) == 0) ? fid + 5 : fid;
    const cJSON *subs = cJSON_GetObjectItem(r, "subscribers");
    const cJSON *vel  = cJSON_GetObjectItem(r, "velocity");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "FEEDLY_FEED_SEARCH");
    cJSON_AddStringToObject(p, "source", "cloud.feedly.com");
    cJSON_AddStringToObject(p, "entity", q);
    jcopy(p, "feed_id", r, "feedId");
    cJSON_AddStringToObject(p, "feed_url", feed_url);
    jcopy(p, "title", r, "title");
    jcopy(p, "website", r, "website");
    jcopy(p, "description", r, "description");
    jcopy(p, "language", r, "language");
    jcopy(p, "topics", r, "topics");
    jcopy(p, "subscribers", r, "subscribers");
    jcopy(p, "velocity", r, "velocity");         /* posts per week */
    jcopy(p, "last_updated_ms", r, "lastUpdated");
    jcopy(p, "created_ms", r, "created");
    jcopy(p, "score", r, "score");
    cJSON_AddBoolToObject(p, "success", 1);
    char summary[300];
    snprintf(summary, sizeof summary, "%ld subscribers · %.1f posts/week · %s",
             cJSON_IsNumber(subs) ? (long)subs->valuedouble : 0L,
             cJSON_IsNumber(vel) ? vel->valuedouble : 0.0,
             jo_sv(r, "language") ? jo_sv(r, "language") : "?");
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"FEEDLY_FEED_SEARCH\",\"rss\"]",
                  fid, title, summary,
                  jo_sv(r, "website") ? jo_sv(r, "website") : feed_url,
                  NULL, jo_sv(r, "language"));
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[FEEDLY_FEED_SEARCH] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_feedly_def = {
  .id = "FEEDLY_FEED_SEARCH", .collector = "osint",
  .name = "Feedly Feed Discovery Search",
  .update_interval_sec = 0, .run = run_feedly,
  .category = "media", .type = "api",
  .url = "https://cloud.feedly.com/v3/search/feeds",
  .description = "Given a brand, domain or topic, returns the RSS/Atom feeds that cover it with subscriber counts and posting velocity — feed discovery as an OSINT primitive.",
  .license = "cloud.feedly.com/v3/search/feeds answers unauthenticated; feed metadata only, no article bodies.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_feedly_def)

/* ---- ITUNES_PODCAST_SEARCH ---------------------------------------------- */

static int run_itunes(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[400];
  snprintf(url, sizeof url,
           "https://itunes.apple.com/search?term=%s&entity=podcast&limit=20", enc);
  free(enc);
  /* Served as text/javascript with leading blank lines — fetch as text. */
  char *txt = feed_get_text(ctx->http, url, 20000);
  if (!txt) { fprintf(stderr, "[ITUNES_PODCAST_SEARCH] no response for %s\n", q); return 0; }
  cJSON *doc = cJSON_Parse(txt);
  free(txt);
  if (!doc) { fprintf(stderr, "[ITUNES_PODCAST_SEARCH] parse failed for %s\n", q); return 0; }
  const cJSON *res = cJSON_GetObjectItem(doc, "results");
  int n = 0;
  const cJSON *r;
  if (cJSON_IsArray(res)) cJSON_ArrayForEach(r, res) {
    const char *name = jo_sv(r, "collectionName");
    if (!name) continue;
    const cJSON *cid = cJSON_GetObjectItem(r, "collectionId");
    const cJSON *tc  = cJSON_GetObjectItem(r, "trackCount");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "ITUNES_PODCAST_SEARCH");
    cJSON_AddStringToObject(p, "source", "itunes.apple.com (Apple Podcasts)");
    cJSON_AddStringToObject(p, "entity", q);
    jcopy(p, "collection_id", r, "collectionId");
    jcopy(p, "collection_name", r, "collectionName");
    jcopy(p, "artist_name", r, "artistName");
    jcopy(p, "feed_url", r, "feedUrl");
    jcopy(p, "collection_view_url", r, "collectionViewUrl");
    jcopy(p, "artwork_url_600", r, "artworkUrl600");
    jcopy(p, "genres", r, "genres");
    jcopy(p, "track_count", r, "trackCount");
    jcopy(p, "release_date", r, "releaseDate");
    jcopy(p, "country", r, "country");
    cJSON_AddBoolToObject(p, "success", 1);
    char rk[64], summary[400];
    snprintf(rk, sizeof rk, "itunes-%ld",
             cJSON_IsNumber(cid) ? (long)cid->valuedouble : 0L);
    snprintf(summary, sizeof summary, "%s · %ld episodes · feed %s",
             jo_sv(r, "artistName") ? jo_sv(r, "artistName") : "?",
             cJSON_IsNumber(tc) ? (long)tc->valuedouble : 0L,
             jo_sv(r, "feedUrl") ? jo_sv(r, "feedUrl") : "(none published)");
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"ITUNES_PODCAST_SEARCH\",\"podcast\"]",
                  rk, name, summary,
                  jo_sv(r, "collectionViewUrl") ? jo_sv(r, "collectionViewUrl")
                                              : jo_sv(r, "feedUrl"),
                  jo_sv(r, "releaseDate"), NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[ITUNES_PODCAST_SEARCH] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_itunes_def = {
  .id = "ITUNES_PODCAST_SEARCH", .collector = "osint",
  .name = "Apple Podcasts (iTunes Search API)",
  .update_interval_sec = 0, .run = run_itunes,
  .category = "media", .type = "api",
  .url = "https://itunes.apple.com/search?entity=podcast",
  .description = "The largest podcast directory searchable without a key — resolves a show or host name to the raw RSS feedUrl behind it, plus genres, episode count and country.",
  .license = "Apple public Search API; ~20 calls/min, attribution to Apple required on display.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_itunes_def)

/* ---- GPODDER_SEARCH ------------------------------------------------------ */

static int run_gpodder(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!jo_looks_like_keyword(q)) return 0;
  char *enc = soc_urlenc(q);
  if (!enc) return 0;
  char url[400];
  snprintf(url, sizeof url, "https://gpodder.net/search.json?q=%s", enc);
  free(enc);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[GPODDER_SEARCH] no usable response for %s\n", q);
    cJSON_Delete(doc); return 0;
  }
  int n = 0;
  const cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    const char *feed = jo_sv(r, "url");        /* 'url' IS the RSS feed */
    const char *title = jo_sv(r, "title");
    if (!feed || !title) continue;
    const cJSON *subs = cJSON_GetObjectItem(r, "subscribers");
    const cJSON *slw  = cJSON_GetObjectItem(r, "subscribers_last_week");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "service", "GPODDER_SEARCH");
    cJSON_AddStringToObject(p, "source", "gpodder.net");
    cJSON_AddStringToObject(p, "entity", q);
    jcopy(p, "feed_url", r, "url");
    jcopy(p, "title", r, "title");
    jcopy(p, "author", r, "author");
    jcopy(p, "description", r, "description");
    jcopy(p, "subscribers", r, "subscribers");
    jcopy(p, "subscribers_last_week", r, "subscribers_last_week");
    jcopy(p, "logo_url", r, "logo_url");
    jcopy(p, "website", r, "website");
    jcopy(p, "mygpo_link", r, "mygpo_link");
    if (cJSON_IsNumber(subs) && cJSON_IsNumber(slw))
      cJSON_AddNumberToObject(p, "subscriber_delta",
                              subs->valuedouble - slw->valuedouble);
    cJSON_AddBoolToObject(p, "success", 1);
    char summary[300];
    snprintf(summary, sizeof summary, "%s · %ld subscribers (was %ld)",
             jo_sv(r, "author") ? jo_sv(r, "author") : "?",
             cJSON_IsNumber(subs) ? (long)subs->valuedouble : 0L,
             cJSON_IsNumber(slw) ? (long)slw->valuedouble : 0L);
    n += soc_emit(sink, p, "osint_service_result",
                  "[\"osint-search\",\"GPODDER_SEARCH\",\"podcast\"]",
                  feed, title, summary,
                  jo_sv(r, "mygpo_link") ? jo_sv(r, "mygpo_link") : feed,
                  NULL, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[GPODDER_SEARCH] emitted %d for %s\n", n, q);
  return 0;
}

static const source_def soc_gpodder_def = {
  .id = "GPODDER_SEARCH", .collector = "osint",
  .name = "gPodder.net Podcast Directory Search",
  .update_interval_sec = 0, .run = run_gpodder,
  .category = "media", .type = "api",
  .url = "https://gpodder.net/search.json",
  .description = "Open, non-Apple podcast index keyed on the feed URL itself, with subscriber counts and a week-on-week delta — an independent cross-check against the Apple directory.",
  .license = "gPodder.net is AGPL open-source infrastructure with a documented public API; keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_gpodder_def)
