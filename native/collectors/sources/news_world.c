/* collectors/sources/news_world.c — WORLD_NEWS_RSS
 * Aggregates ~30 major global news RSS/Atom feeds (Reuters, AP, AFP, BBC, DW,
 * Al Jazeera, Guardian, France24, CNA, SCMP, TASS, Xinhua, Kyodo-en, NHK-World,
 * NPR, CNN, ABC, CBC, euronews, Times of India, etc.). For each feed we REAL-
 * fetch the XML and parse every <item>/<entry>, extracting the real title, link
 * and publish date — nothing is synthesized. When ctx->entity is set (OSINT
 * pivot) we emit only items whose title contains the entity substring (case-
 * insensitive); on a scheduled run (entity == NULL) we emit all items. A feed
 * that fails to fetch, or yields no matching items, simply contributes 0 (honest
 * empty). One intel_item per article, keyed by its link. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* case-insensitive substring (ASCII fold; UTF-8 bytes compared verbatim). */
static int nw_icontains(const char *hay, const char *needle) {
  if (!hay || !needle || !*needle) return 1;
  size_t nl = strlen(needle);
  for (const char *p = hay; *p; p++) {
    size_t i = 0;
    while (i < nl && p[i] &&
           tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i]))
      i++;
    if (i == nl) return 1;
  }
  return 0;
}

/* Extract the link for one <item>/<entry> block that spans [start,end).
 * Handles RSS <link>URL</link> and Atom <link href="URL" .../> (preferring
 * rel="alternate"/no-rel over rel="self"). Writes into out (size cap); returns
 * 1 on success. */
static int nw_extract_link(const char *start, const char *end, char *out, size_t cap) {
  out[0] = 0;
  /* Atom: look for <link ... href="..." within the block. */
  const char *p = start;
  const char *best = NULL; size_t bestlen = 0;
  while (p < end) {
    const char *lt = strstr(p, "<link");
    if (!lt || lt >= end) break;
    const char *tagend = strchr(lt, '>');
    if (!tagend || tagend >= end) break;
    const char *href = strstr(lt, "href=\"");
    if (href && href < tagend) {
      href += 6;
      const char *he = strchr(href, '"');
      if (he && he <= tagend) {
        size_t len = (size_t)(he - href);
        int is_self = 0;
        const char *rel = strstr(lt, "rel=\"");
        if (rel && rel < tagend && strncmp(rel + 5, "self", 4) == 0) is_self = 1;
        if (len && len < cap && (!best || (!is_self))) { best = href; bestlen = len; if (!is_self) break; }
      }
    } else {
      /* RSS <link>URL</link> */
      const char *s = tagend + 1;
      const char *c = strstr(s, "</link>");
      if (c && c <= end) {
        size_t len = (size_t)(c - s);
        while (len && (s[0] == '\n' || s[0] == ' ' || s[0] == '\t')) { s++; len--; }
        while (len && (s[len-1] == '\n' || s[len-1] == ' ' || s[len-1] == '\t')) len--;
        if (len && len < cap) { best = s; bestlen = len; break; }
      }
    }
    p = tagend + 1;
  }
  if (!best) return 0;
  if (bestlen >= cap) bestlen = cap - 1;
  memcpy(out, best, bestlen);
  out[bestlen] = 0;
  return out[0] ? 1 : 0;
}

/* Collect one feed. Returns #emitted. */
static int nw_collect_feed(const source_ctx *ctx, intel_sink *sink,
                           const char *url, const char *publisher,
                           const char *lang, const char *query, int max) {
  const char *hdrs[] = {
    "User-Agent: Mozilla/5.0 (JapanOSINT WorldNews)",
    "Accept: application/rss+xml, application/atom+xml, application/xml, text/xml, */*",
    NULL
  };
  char *xml = jo_get(ctx, url, hdrs, "world-news-rss");
  if (!xml) return 0;

  int emitted = 0;
  const char *cur = xml;
  const char *entry;
  while (emitted < max && (entry = jo_next_entry(&cur)) != NULL) {
    /* bound this entry: from `entry` up to the next entry (or EOF) */
    const char *after = entry + 5;
    const char *nxt_it = strstr(after, "<item");
    const char *nxt_en = strstr(after, "<entry");
    const char *bend = nxt_it;
    if (!bend || (nxt_en && nxt_en < bend)) bend = nxt_en;
    if (!bend) bend = entry + strlen(entry);

    /* title */
    const char *tcur = entry;
    char *title = jo_tag_inner(&tcur, "title");
    if (title && (const char *)tcur > bend) { /* title belonged to next entry */
      free(title); title = NULL;
    }

    /* date: try common tags */
    const char *dcur = entry; char *date = jo_tag_inner(&dcur, "pubDate");
    if (date && (const char *)dcur > bend) { free(date); date = NULL; }
    if (!date) { dcur = entry; date = jo_tag_inner(&dcur, "published");
      if (date && (const char *)dcur > bend) { free(date); date = NULL; } }
    if (!date) { dcur = entry; date = jo_tag_inner(&dcur, "updated");
      if (date && (const char *)dcur > bend) { free(date); date = NULL; } }
    if (!date) { dcur = entry; date = jo_tag_inner(&dcur, "dc:date");
      if (date && (const char *)dcur > bend) { free(date); date = NULL; } }

    char link[900];
    int haslink = nw_extract_link(entry, bend, link, sizeof link);

    /* advance cur to the next entry regardless */
    cur = bend;

    if (!title || !title[0]) { free(title); free(date); continue; }
    if (query && *query && !nw_icontains(title, query)) { free(title); free(date); continue; }

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "WORLD_NEWS_RSS");
    cJSON_AddStringToObject(props, "publisher", publisher);
    if (haslink) cJSON_AddStringToObject(props, "link", link);
    if (query && *query) cJSON_AddStringToObject(props, "query", query);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    char tags[128];
    snprintf(tags, sizeof tags, "[\"news\",\"world-news\",\"publisher:%s\"]", publisher);

    intel_item it = {0};
    it.remote_key      = haslink ? link : title;
    it.title           = title;
    it.summary         = publisher;
    it.link            = haslink ? link : NULL;
    it.author          = publisher;
    it.lang            = lang;
    it.published_at    = date;
    it.record_type     = "world-news-item";
    it.properties_json = pj;
    it.tags_json       = tags;
    if (sink->emit(sink, &it) >= 0) emitted++;

    free(pj);
    free(title);
    free(date);
  }
  free(xml);
  return emitted;
}

struct nw_feed { const char *url; const char *pub; const char *lang; };

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const struct nw_feed FEEDS[] = {
    { "https://feeds.reuters.com/reuters/topNews",                          "Reuters",          "en" },
    { "https://www.reutersagency.com/feed/?best-topics=top-news&post_type=best", "Reuters Agency", "en" },
    { "https://apnews.com/index.rss",                                       "AP",               "en" },
    { "https://www.afp.com/en/rss.xml",                                     "AFP",              "en" },
    { "https://feeds.bbci.co.uk/news/world/rss.xml",                        "BBC",              "en" },
    { "https://rss.dw.com/rdf/rss-en-all",                                  "DW",               "en" },
    { "https://www.aljazeera.com/xml/rss/all.xml",                          "Al Jazeera",       "en" },
    { "https://www.theguardian.com/world/rss",                              "The Guardian",     "en" },
    { "https://www.france24.com/en/rss",                                    "France 24",        "en" },
    { "https://www.channelnewsasia.com/rssfeeds/8395986",                   "CNA",              "en" },
    { "https://www.scmp.com/rss/91/feed",                                   "SCMP",             "en" },
    { "https://tass.com/rss/v2.xml",                                        "TASS",             "en" },
    { "https://english.news.cn/rss/worldrss.xml",                           "Xinhua",           "en" },
    /* AUDIT NOTE (slice a3): five of the feeds in this table are dead and were
     * silently contributing zero rows on every run —
     *   feeds.reuters.com/reuters/topNews            DNS gone (Reuters retired
     *                                                 public RSS in 2020)
     *   www.reutersagency.com/feed/?best-topics=…    404
     *   apnews.com/index.rss                          401 (AP retired it)
     *   english.kyodonews.net/rss/news.xml            404, no working Kyodo
     *                                                 RSS path found
     *   www3.nhk.or.jp/nhkworld/en/news/feeds/        404
     * NHK still publishes a live feed at the domestic path below (verified 200
     * with real <item>s), so it is restored here as a ja feed. Reuters, AP and
     * Kyodo have no key-free replacement — they are left in place and reported
     * rather than swapped for an invented endpoint. */
    { "https://english.kyodonews.net/rss/news.xml",                         "Kyodo News",       "en" },
    { "https://www3.nhk.or.jp/rss/news/cat0.xml",                           "NHK",              "ja" },
    { "https://feeds.npr.org/1004/rss.xml",                                 "NPR",              "en" },
    { "http://rss.cnn.com/rss/edition_world.rss",                           "CNN",              "en" },
    { "https://abcnews.go.com/abcnews/internationalheadlines",             "ABC News",         "en" },
    { "https://www.cbc.ca/webfeed/rss/rss-world",                           "CBC",              "en" },
    { "https://www.euronews.com/rss?level=theme&name=news",                 "Euronews",         "en" },
    { "https://timesofindia.indiatimes.com/rssfeeds/296589292.cms",         "Times of India",   "en" },
    { "https://www.cbsnews.com/latest/rss/world",                           "CBS News",         "en" },
    { "https://moxie.foxnews.com/google-publisher/world.xml",               "Fox News",         "en" },
    { "https://www.independent.co.uk/news/world/rss",                       "The Independent",  "en" },
    { "https://www.thelocal.com/feeds/rss.php",                             "The Local",        "en" },
    { "https://feeds.a.dj.com/rss/RSSWorldNews.xml",                        "WSJ",              "en" },
    { "https://www.lemonde.fr/en/rss/une.xml",                              "Le Monde (EN)",    "en" },
    { "https://www.spiegel.de/international/index.rss",                      "Der Spiegel (EN)", "en" },
    { "https://sputnikglobe.com/export/rss2/archive/index.xml",             "Sputnik",          "en" },
    { "https://www.rt.com/rss/news/",                                       "RT",               "en" },
  };
  const int NFEEDS = (int)(sizeof(FEEDS) / sizeof(FEEDS[0]));
  const char *query = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;

  int total = 0;
  for (int i = 0; i < NFEEDS; i++) {
    if (ctx->cancel && *ctx->cancel) break;
    total += nw_collect_feed(ctx, sink, FEEDS[i].url, FEEDS[i].pub,
                             FEEDS[i].lang, query, 40);
  }
  fprintf(stderr, "[world-news-rss] emitted %d across %d feeds\n", total, NFEEDS);
  return 0;   /* honest empty is not an error */
}

static const source_def news_world_def = {
  .id = "WORLD_NEWS_RSS", .collector = "osint",
  .name = "World News RSS", .name_ja = "世界ニュース RSS",
  .update_interval_sec = 1800, .run = run,
  .category = "news", .type = "scraped",
  .url = "internal://osint/world-news-rss",
  .description = "Aggregated global news RSS/Atom feeds (Reuters, AP, AFP, BBC, DW, Al Jazeera, Guardian, Xinhua, Kyodo, TASS, etc.); entity filters titles",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(news_world_def)
