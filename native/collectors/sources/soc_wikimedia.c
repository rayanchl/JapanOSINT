/* soc_wikimedia.c — Wikimedia attention + change streams (four scheduled feeds).
 * Every request carries a descriptive User-Agent, as Wikimedia's UA policy
 * requires. All keyless.
 *
 *  wikipedia-top-pageviews  GET https://wikimedia.org/api/rest_v1/metrics/
 *                               pageviews/top/en.wikipedia/all-access/Y/M/D
 *      records live under items[0].articles[]; the target day is (today-2) UTC
 *      because the current day's aggregate is not final.
 *      emits: project, access, date, article, rank, views
 *      licence: Wikimedia REST API; data CC0.
 *  wikimedia-featured-feed  GET https://api.wikimedia.org/feed/v1/wikipedia/en/
 *                               featured/Y/M/D
 *      one document with independent sections — we emit one row per news[]
 *      item and per mostread.articles[] entry, plus the featured article,
 *      never one row for the whole document.
 *      emits: tfa title/extract/pageid/wikibase_item, news story text + linked
 *             article titles and QIDs, mostread article/rank/views
 *      licence: Wikimedia API portal, keyless for anonymous use; content
 *               CC BY-SA / CC0 per Wikimedia terms.
 *  wikidata-recentchanges   GET https://www.wikidata.org/w/api.php?action=query
 *                               &list=recentchanges&rcnamespace=0&rclimit=50
 *      emits: type, ns, title (Q-id), user, timestamp, comment
 *      licence: MediaWiki Action API, keyless, CC0 data.
 *  wikimedia-meta-recentchanges  same shape on meta.wikimedia.org (no namespace
 *      filter): global renames, locks and steward actions; type='log' rows are
 *      the interesting ones.
 *      licence: MediaWiki Action API, keyless, CC BY-SA.
 *
 * No coordinates anywhere in these responses → has_geo stays 0 (R2).
 */
#include "lib/osintemit.h"
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Wikimedia's UA policy asks for a descriptive agent identifying the client. */
static const char *const WM_UA[] = {
  "User-Agent: JapanOSINT-native/1.0 (OSINT research collector; +https://github.com/)",
  "Accept: application/json", NULL
};

/* Wiki page URL for a page title: spaces become underscores. */
static void wiki_link(char *out, size_t n, const char *host, const char *title) {
  size_t j = 0;
  j += (size_t)snprintf(out, n, "https://%s/wiki/", host);
  for (const char *p = title; *p && j + 4 < n; p++)
    out[j++] = (*p == ' ') ? '_' : *p;
  out[j < n ? j : n - 1] = 0;
}

/* ---- wikipedia-top-pageviews -------------------------------------------- */

static int run_top_pageviews(const source_ctx *ctx, intel_sink *sink) {
  /* today-2 UTC: yesterday's aggregate is not always final yet. */
  time_t t = time(NULL) - 2 * 86400;
  struct tm g; gmtime_r(&t, &g);
  char url[300], daystr[16];
  strftime(daystr, sizeof daystr, "%Y-%m-%d", &g);
  snprintf(url, sizeof url,
      "https://wikimedia.org/api/rest_v1/metrics/pageviews/top/"
      "en.wikipedia/all-access/%04d/%02d/%02d",
      g.tm_year + 1900, g.tm_mon + 1, g.tm_mday);

  cJSON *doc = feed_get_json_h(ctx->http, url, WM_UA, 25000);
  if (!doc) { fprintf(stderr, "[wikipedia-top-pageviews] fetch failed\n"); return -1; }
  const cJSON *items = cJSON_GetObjectItem(doc, "items");
  const cJSON *it0 = cJSON_IsArray(items) ? cJSON_GetArrayItem(items, 0) : NULL;
  const cJSON *arts = cJSON_IsObject(it0) ? cJSON_GetObjectItem(it0, "articles") : NULL;
  if (!cJSON_IsArray(arts)) {
    fprintf(stderr, "[wikipedia-top-pageviews] unexpected shape\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *a;
  cJSON_ArrayForEach(a, arts) {
    if (n >= 200) break;                       /* top 200 of the 1,000 rows */
    const char *article = jo_sv(a, "article");
    if (!article) continue;
    const cJSON *vw = cJSON_GetObjectItem(a, "views");
    const cJSON *rk = cJSON_GetObjectItem(a, "rank");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "wikimedia.org REST");
    jcopy(p, "project", it0, "project");
    jcopy(p, "access", it0, "access");
    cJSON_AddStringToObject(p, "date", daystr);
    jcopy(p, "article", a, "article");
    jcopy(p, "views", a, "views");
    jcopy(p, "rank", a, "rank");
    char link[400], remote[300], summary[160];
    wiki_link(link, sizeof link, "en.wikipedia.org", article);
    snprintf(remote, sizeof remote, "%s|%s", daystr, article);
    snprintf(summary, sizeof summary, "rank %ld · %ld views on %s",
             cJSON_IsNumber(rk) ? (long)rk->valuedouble : 0L,
             cJSON_IsNumber(vw) ? (long)vw->valuedouble : 0L, daystr);
    n += soc_emit(sink, p, "wikipedia-pageviews",
                  "[\"wikipedia\",\"pageviews\",\"attention\"]",
                  remote, article, summary, link, NULL, "en");
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[wikipedia-top-pageviews] emitted %d for %s\n", n, daystr);
  return 0;
}

static const source_def soc_top_pageviews_def = {
  .id = "wikipedia-top-pageviews", .collector = "social",
  .name = "Wikimedia Top Pageviews (REST)",
  .update_interval_sec = 86400, .run = run_top_pageviews,
  .category = "media", .type = "api",
  .url = "https://wikimedia.org/api/rest_v1/metrics/pageviews/top/en.wikipedia/all-access",
  .description = "The most-read Wikipedia articles for a given day with exact view counts — attention telemetry where an unexplained spike on a person, company or place is itself a lead.",
  .license = "Wikimedia REST API; data CC0. Descriptive User-Agent sent per policy.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_top_pageviews_def)

/* ---- wikimedia-featured-feed -------------------------------------------- */

static int run_featured(const source_ctx *ctx, intel_sink *sink) {
  time_t t = time(NULL) - 86400;
  struct tm g; gmtime_r(&t, &g);
  char url[300], daystr[16];
  strftime(daystr, sizeof daystr, "%Y-%m-%d", &g);
  snprintf(url, sizeof url,
      "https://api.wikimedia.org/feed/v1/wikipedia/en/featured/%04d/%02d/%02d",
      g.tm_year + 1900, g.tm_mon + 1, g.tm_mday);

  cJSON *doc = feed_get_json_h(ctx->http, url, WM_UA, 25000);
  if (!cJSON_IsObject(doc)) {
    fprintf(stderr, "[wikimedia-featured-feed] fetch/parse failed\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;

  /* --- today's featured article --- */
  const cJSON *tfa = cJSON_GetObjectItem(doc, "tfa");
  if (cJSON_IsObject(tfa) && jo_sv(tfa, "title")) {
    const char *title = jo_sv(tfa, "title");
    cJSON *p = cJSON_CreateObject();
    if (p) {
      cJSON_AddStringToObject(p, "source", "api.wikimedia.org");
      cJSON_AddStringToObject(p, "section", "featured_article");
      cJSON_AddStringToObject(p, "date", daystr);
      jcopy(p, "title", tfa, "title");
      jcopy(p, "extract", tfa, "extract");
      jcopy(p, "pageid", tfa, "pageid");
      jcopy(p, "wikibase_item", tfa, "wikibase_item");
      jcopy(p, "lang", tfa, "lang");
      char link[400], rk[300];
      wiki_link(link, sizeof link, "en.wikipedia.org", title);
      snprintf(rk, sizeof rk, "tfa|%s|%s", daystr, title);
      n += soc_emit(sink, p, "wikipedia-featured",
                    "[\"wikipedia\",\"featured\",\"daily\"]",
                    rk, title, jo_sv(tfa, "extract"), link, NULL, "en");
    }
  }

  /* --- "In the news" — one row per item --- */
  const cJSON *news = cJSON_GetObjectItem(doc, "news"), *ni;
  if (cJSON_IsArray(news)) cJSON_ArrayForEach(ni, news) {
    const char *story = jo_sv(ni, "story");
    char *txt = story ? html_strip(story) : NULL;
    if (!txt || !txt[0]) { free(txt); continue; }
    const cJSON *links = cJSON_GetObjectItem(ni, "links");
    const cJSON *l0 = cJSON_IsArray(links) ? cJSON_GetArrayItem(links, 0) : NULL;
    cJSON *p = cJSON_CreateObject();
    if (!p) { free(txt); continue; }
    cJSON_AddStringToObject(p, "source", "api.wikimedia.org");
    cJSON_AddStringToObject(p, "section", "in_the_news");
    cJSON_AddStringToObject(p, "date", daystr);
    cJSON_AddStringToObject(p, "story", txt);
    cJSON *qids = cJSON_CreateArray();
    cJSON *titles = cJSON_CreateArray();
    const cJSON *l;
    if (cJSON_IsArray(links)) cJSON_ArrayForEach(l, links) {
      const char *lt = jo_sv(l, "title");
      const char *qid = jo_sv(l, "wikibase_item");
      if (lt && titles) cJSON_AddItemToArray(titles, cJSON_CreateString(lt));
      if (qid && qids) cJSON_AddItemToArray(qids, cJSON_CreateString(qid));
    }
    if (titles) cJSON_AddItemToObject(p, "linked_articles", titles);
    if (qids)   cJSON_AddItemToObject(p, "wikidata_qids", qids);
    char link[400], rk[400], title[400];
    if (cJSON_IsObject(l0) && jo_sv(l0, "title"))
      wiki_link(link, sizeof link, "en.wikipedia.org", jo_sv(l0, "title"));
    else
      snprintf(link, sizeof link, "https://en.wikipedia.org/wiki/Portal:Current_events");
    snprintf(title, sizeof title, "%.300s", txt);
    snprintf(rk, sizeof rk, "itn|%s|%.200s", daystr, txt);
    n += soc_emit(sink, p, "wikipedia-in-the-news",
                  "[\"wikipedia\",\"news\",\"curated\"]",
                  rk, title, NULL, link, NULL, "en");
    free(txt);
  }

  /* --- most-read list — one row per article --- */
  const cJSON *mr = cJSON_GetObjectItem(doc, "mostread");
  const cJSON *mra = cJSON_IsObject(mr) ? cJSON_GetObjectItem(mr, "articles") : NULL;
  int mrn = 0;
  const cJSON *a;
  if (cJSON_IsArray(mra)) cJSON_ArrayForEach(a, mra) {
    if (mrn >= 30) break;
    const char *title = jo_sv(a, "title");
    if (!title) continue;
    const cJSON *vw = cJSON_GetObjectItem(a, "views");
    const cJSON *rk = cJSON_GetObjectItem(a, "rank");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "api.wikimedia.org");
    cJSON_AddStringToObject(p, "section", "mostread");
    cJSON_AddStringToObject(p, "date", daystr);
    jcopy(p, "title", a, "title");
    jcopy(p, "extract", a, "extract");
    jcopy(p, "views", a, "views");
    jcopy(p, "rank", a, "rank");
    jcopy(p, "wikibase_item", a, "wikibase_item");
    jcopy(p, "pageid", a, "pageid");
    char link[400], rkey[300], summary[160];
    wiki_link(link, sizeof link, "en.wikipedia.org", title);
    snprintf(rkey, sizeof rkey, "mostread|%s|%s", daystr, title);
    snprintf(summary, sizeof summary, "rank %ld · %ld views",
             cJSON_IsNumber(rk) ? (long)rk->valuedouble : 0L,
             cJSON_IsNumber(vw) ? (long)vw->valuedouble : 0L);
    n += soc_emit(sink, p, "wikipedia-mostread",
                  "[\"wikipedia\",\"mostread\",\"attention\"]",
                  rkey, title, summary, link, NULL, "en");
    mrn++;
  }

  cJSON_Delete(doc);
  fprintf(stderr, "[wikimedia-featured-feed] emitted %d for %s\n", n, daystr);
  return 0;
}

static const source_def soc_featured_def = {
  .id = "wikimedia-featured-feed", .collector = "social",
  .name = "Wikimedia Featured Content Feed",
  .update_interval_sec = 86400, .run = run_featured,
  .category = "media", .type = "api",
  .url = "https://api.wikimedia.org/feed/v1/wikipedia/en/featured",
  .description = "Wikipedia's daily curated bundle — featured article, most-read list and the editor-selected 'In the news' items, each carrying its Wikidata QID for entity pivoting.",
  .license = "Wikimedia API portal, keyless anonymous use; content CC BY-SA / CC0.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_featured_def)

/* ---- recentchanges (Wikidata + Meta share one parser) ------------------- */

static int recentchanges(const source_ctx *ctx, intel_sink *sink,
                         const char *host, const char *url, const char *sid,
                         const char *rtype, const char *tags) {
  cJSON *doc = feed_get_json_h(ctx->http, url, WM_UA, 25000);
  if (!doc) { fprintf(stderr, "[%s] fetch failed\n", sid); return -1; }
  const cJSON *qy = cJSON_GetObjectItem(doc, "query");
  const cJSON *rc = cJSON_IsObject(qy) ? cJSON_GetObjectItem(qy, "recentchanges") : NULL;
  if (!cJSON_IsArray(rc)) {
    fprintf(stderr, "[%s] unexpected shape\n", sid);
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *c;
  cJSON_ArrayForEach(c, rc) {
    const char *title = jo_sv(c, "title");
    const char *ts    = jo_sv(c, "timestamp");
    if (!title) continue;
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", host);
    jcopy(p, "type", c, "type");
    jcopy(p, "ns", c, "ns");
    jcopy(p, "title", c, "title");
    jcopy(p, "user", c, "user");
    jcopy(p, "timestamp", c, "timestamp");
    jcopy(p, "comment", c, "comment");
    char link[500], rk[400], summary[400];
    wiki_link(link, sizeof link, host, title);
    snprintf(rk, sizeof rk, "%s|%s|%s", host, title, ts ? ts : "");
    snprintf(summary, sizeof summary, "%s by %s%s%.250s",
             jo_sv(c, "type") ? jo_sv(c, "type") : "edit",
             jo_sv(c, "user") ? jo_sv(c, "user") : "?",
             jo_sv(c, "comment") ? " — " : "",
             jo_sv(c, "comment") ? jo_sv(c, "comment") : "");
    n += soc_emit(sink, p, rtype, tags, rk, title, summary, link, ts, NULL);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

static int run_wikidata_rc(const source_ctx *ctx, intel_sink *sink) {
  return recentchanges(ctx, sink, "www.wikidata.org",
      "https://www.wikidata.org/w/api.php?action=query&list=recentchanges"
      "&rcnamespace=0&rclimit=50&rcprop=title%7Ctimestamp%7Cuser%7Ccomment"
      "&format=json",
      "wikidata-recentchanges", "wikidata-change",
      "[\"wikidata\",\"recentchanges\",\"knowledge-graph\"]");
}

static int run_meta_rc(const source_ctx *ctx, intel_sink *sink) {
  return recentchanges(ctx, sink, "meta.wikimedia.org",
      "https://meta.wikimedia.org/w/api.php?action=query&list=recentchanges"
      "&rclimit=50&rcprop=title%7Ctimestamp%7Cuser%7Ccomment&format=json",
      "wikimedia-meta-recentchanges", "wikimedia-global-action",
      "[\"wikimedia\",\"meta\",\"global-actions\",\"rename\"]");
}

static const source_def soc_wikidata_rc_def = {
  .id = "wikidata-recentchanges", .collector = "social",
  .name = "Wikidata Item Recent Changes",
  .update_interval_sec = 900, .run = run_wikidata_rc,
  .category = "media", .type = "api",
  .url = "https://www.wikidata.org/w/api.php?action=query&list=recentchanges",
  .description = "Live edit stream of the Wikidata knowledge graph restricted to the item namespace — who changed which entity and why, bot batches included.",
  .license = "MediaWiki Action API, keyless, CC0 data; descriptive User-Agent sent per Wikimedia policy.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_wikidata_rc_def)

static const source_def soc_meta_rc_def = {
  .id = "wikimedia-meta-recentchanges", .collector = "social",
  .name = "Meta-Wiki Global Actions Stream",
  .update_interval_sec = 3600, .run = run_meta_rc,
  .category = "media", .type = "api",
  .url = "https://meta.wikimedia.org/w/api.php?action=query&list=recentchanges",
  .description = "Cross-project Wikimedia governance layer: global account renames, locks and steward actions — an identity-pivot signal no per-project feed exposes.",
  .license = "MediaWiki Action API, keyless, CC BY-SA; descriptive User-Agent required by policy and sent.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_meta_rc_def)
