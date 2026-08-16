/* soc_aggregators.c — link aggregators and open forums (six scheduled feeds).
 *
 *  hn-frontpage-firebase   GET https://hacker-news.firebaseio.com/v0/topstories.json
 *                          then /v0/item/<id>.json for the first 30 ranked ids
 *      emits: rank, id, title, url, by, time (→ ISO), score, descendants, type
 *      licence: official HN API (github.com/HackerNews/API), public, keyless.
 *  hn-new-stories          GET https://hn.algolia.com/api/v1/search_by_date?tags=story
 *      emits: objectID, title, url, author, created_at, points, num_comments
 *      licence: Algolia-hosted HN search API, keyless, publicly documented.
 *  lobsters-newest         GET https://lobste.rs/newest.json
 *      emits: short_id, title, url, created_at, score, tags[], submitter_user,
 *             comment_count, comments_url
 *      licence: Lobsters serves .json for every index page as a documented
 *               public interface; the site asks for a descriptive User-Agent,
 *               which we send.
 *  chan-4chan-news         GET https://a.4cdn.org/news/catalog.json
 *      emits: thread no, subject, opening-post text (HTML stripped), time
 *             (→ ISO), replies, images, sticky, closed, last_modified
 *      licence: official 4chan read-only API (github.com/4chan/4chan-API).
 *               Terms: max 1 request/second (this collector makes exactly one
 *               request per run) and If-Modified-Since must be sent — we send
 *               it from the previous successful fetch and treat 304 as an
 *               honest empty (return 0), never an error.
 *  discourse-meta-latest   GET https://meta.discourse.org/latest.json
 *  discourse-python-latest GET https://discuss.python.org/latest.json
 *      emits: topic id, title, slug, created_at, last_posted_at, posts_count,
 *             views, category_id, tags[], and the poster usernames resolved
 *             through the response's own users[] array
 *      licence: Discourse ships /latest.json as an unauthenticated public read
 *               endpoint on all public instances; keyless.
 *
 * None of these upstreams returns coordinates → has_geo stays 0 (R2).
 */
#include "lib/osintemit.h"
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "core/httpclient.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *const SOC_UA[] = {
  "User-Agent: JapanOSINT-native/1.0 (OSINT research collector; +https://github.com/)",
  "Accept: application/json", NULL
};

static inline void soc_iso(time_t t, char *out, size_t n) {
  struct tm g;
  gmtime_r(&t, &g);
  strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &g);
}

/* ---- 1. Hacker News ranked front page (Firebase) ------------------------ */

static int run_hn_front(const source_ctx *ctx, intel_sink *sink) {
  cJSON *ids = feed_get_json_h(ctx->http,
      "https://hacker-news.firebaseio.com/v0/topstories.json", SOC_UA, 20000);
  if (!cJSON_IsArray(ids)) {
    fprintf(stderr, "[hn-frontpage-firebase] fetch/parse failed\n");
    cJSON_Delete(ids); return -1;
  }
  int n = 0, rank = 0;
  const cJSON *idv;
  cJSON_ArrayForEach(idv, ids) {                 /* budget the fan-out: 30 */
    if (rank >= 30) break;
    if (!cJSON_IsNumber(idv)) continue;
    long id = (long)idv->valuedouble;
    rank++;
    char url[160];
    snprintf(url, sizeof url,
             "https://hacker-news.firebaseio.com/v0/item/%ld.json", id);
    cJSON *item = feed_get_json_h(ctx->http, url, SOC_UA, 15000);
    if (!cJSON_IsObject(item)) { cJSON_Delete(item); continue; }
    const char *title = jo_sv(item, "title");
    if (!title) { cJSON_Delete(item); continue; }
    const cJSON *tv = cJSON_GetObjectItem(item, "time");
    char iso[40]; iso[0] = 0;
    if (cJSON_IsNumber(tv)) soc_iso((time_t)tv->valuedouble, iso, sizeof iso);
    cJSON *p = cJSON_CreateObject();
    if (!p) { cJSON_Delete(item); continue; }
    cJSON_AddStringToObject(p, "source", "hacker-news.firebaseio.com");
    cJSON_AddNumberToObject(p, "rank", rank);
    jcopy(p, "id", item, "id");
    jcopy(p, "title", item, "title");
    jcopy(p, "url", item, "url");
    jcopy(p, "by", item, "by");
    jcopy(p, "score", item, "score");
    jcopy(p, "descendants", item, "descendants");
    jcopy(p, "type", item, "type");
    if (iso[0]) cJSON_AddStringToObject(p, "time", iso);
    const cJSON *sc = cJSON_GetObjectItem(item, "score");
    const cJSON *de = cJSON_GetObjectItem(item, "descendants");
    char summary[240], hn[120], rk[40];
    snprintf(hn, sizeof hn, "https://news.ycombinator.com/item?id=%ld", id);
    snprintf(rk, sizeof rk, "hn-top-%ld", id);
    snprintf(summary, sizeof summary, "#%d · %ld points · %ld comments · by %s",
             rank, cJSON_IsNumber(sc) ? (long)sc->valuedouble : 0L,
             cJSON_IsNumber(de) ? (long)de->valuedouble : 0L,
             jo_sv(item, "by") ? jo_sv(item, "by") : "?");
    cJSON_AddStringToObject(p, "hn_url", hn);
    n += soc_emit(sink, p, "hn-story",
                  "[\"hackernews\",\"frontpage\",\"tech\"]",
                  rk, title, summary,
                  jo_sv(item, "url") ? jo_sv(item, "url") : hn,
                  iso[0] ? iso : NULL, "en");
    cJSON_Delete(item);
  }
  cJSON_Delete(ids);
  fprintf(stderr, "[hn-frontpage-firebase] emitted %d\n", n);
  return 0;
}

static const source_def soc_hn_front_def = {
  .id = "hn-frontpage-firebase", .collector = "social",
  .name = "Hacker News Front Page (Firebase API)",
  .update_interval_sec = 900, .run = run_hn_front,
  .category = "media", .type = "api",
  .url = "https://hacker-news.firebaseio.com/v0/topstories.json",
  .description = "The canonical ranked Hacker News front page from the official Firebase API — rank, title, submitter, external URL, score and comment count.",
  .license = "Official HN API (github.com/HackerNews/API), public and keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_hn_front_def)

/* ---- 2. Hacker News chronological new stories (Algolia) ----------------- */

static int run_hn_new(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http,
      "https://hn.algolia.com/api/v1/search_by_date?tags=story&hitsPerPage=20",
      SOC_UA, 20000);
  if (!doc) { fprintf(stderr, "[hn-new-stories] fetch failed\n"); return -1; }
  const cJSON *hits = cJSON_GetObjectItem(doc, "hits");
  if (!cJSON_IsArray(hits)) {
    fprintf(stderr, "[hn-new-stories] unexpected shape\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *h;
  cJSON_ArrayForEach(h, hits) {
    const char *title = jo_sv(h, "title");
    const char *oid   = jo_sv(h, "objectID");
    if (!title || !oid) continue;
    const cJSON *pts = cJSON_GetObjectItem(h, "points");
    const cJSON *nc  = cJSON_GetObjectItem(h, "num_comments");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "hn.algolia.com");
    jcopy(p, "object_id", h, "objectID");
    jcopy(p, "title", h, "title");
    jcopy(p, "url", h, "url");
    jcopy(p, "author", h, "author");
    jcopy(p, "created_at", h, "created_at");
    jcopy(p, "points", h, "points");
    jcopy(p, "num_comments", h, "num_comments");
    jcopy(p, "tags", h, "_tags");
    char hn[120], summary[240];
    snprintf(hn, sizeof hn, "https://news.ycombinator.com/item?id=%s", oid);
    snprintf(summary, sizeof summary, "by %s · %ld points · %ld comments",
             jo_sv(h, "author") ? jo_sv(h, "author") : "?",
             cJSON_IsNumber(pts) ? (long)pts->valuedouble : 0L,
             cJSON_IsNumber(nc) ? (long)nc->valuedouble : 0L);
    cJSON_AddStringToObject(p, "hn_url", hn);
    n += soc_emit(sink, p, "hn-story",
                  "[\"hackernews\",\"new\",\"tech\"]",
                  oid, title, summary,
                  jo_sv(h, "url") ? jo_sv(h, "url") : hn,
                  jo_sv(h, "created_at"), "en");
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[hn-new-stories] emitted %d\n", n);
  return 0;
}

static const source_def soc_hn_new_def = {
  .id = "hn-new-stories", .collector = "social",
  .name = "Hacker News New-Stories Stream (Algolia by date)",
  .update_interval_sec = 900, .run = run_hn_new,
  .category = "media", .type = "api",
  .url = "https://hn.algolia.com/api/v1/search_by_date?tags=story",
  .description = "Chronological firehose of every new Hacker News submission, unfiltered by ranking — catches stories flagged or buried before they reach the front page.",
  .license = "Algolia-hosted HN search API, keyless and publicly documented.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_hn_new_def)

/* ---- 3. Lobsters newest ------------------------------------------------- */

static int run_lobsters(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json_h(ctx->http, "https://lobste.rs/newest.json",
                               SOC_UA, 20000);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[lobsters-newest] fetch/parse failed\n");
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *it;
  cJSON_ArrayForEach(it, doc) {
    const char *title = jo_sv(it, "title");
    const char *sid   = jo_sv(it, "short_id");
    if (!title || !sid) continue;
    const cJSON *sc = cJSON_GetObjectItem(it, "score");
    const cJSON *cc = cJSON_GetObjectItem(it, "comment_count");
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", "lobste.rs");
    jcopy(p, "short_id", it, "short_id");
    jcopy(p, "title", it, "title");
    jcopy(p, "url", it, "url");
    jcopy(p, "created_at", it, "created_at");
    jcopy(p, "score", it, "score");
    jcopy(p, "flags", it, "flags");
    jcopy(p, "tags", it, "tags");
    jcopy(p, "comment_count", it, "comment_count");
    jcopy(p, "comments_url", it, "comments_url");
    /* submitter_user is a username string on the modern API */
    const cJSON *su = cJSON_GetObjectItem(it, "submitter_user");
    const char *submitter = NULL;
    if (cJSON_IsString(su) && su->valuestring && su->valuestring[0])
      submitter = su->valuestring;
    else if (cJSON_IsObject(su)) submitter = jo_sv(su, "username");
    if (submitter) cJSON_AddStringToObject(p, "submitter_user", submitter);
    char summary[240];
    snprintf(summary, sizeof summary, "by %s · %ld points · %ld comments",
             submitter ? submitter : "?",
             cJSON_IsNumber(sc) ? (long)sc->valuedouble : 0L,
             cJSON_IsNumber(cc) ? (long)cc->valuedouble : 0L);
    n += soc_emit(sink, p, "lobsters-story",
                  "[\"lobsters\",\"tech\",\"security\"]",
                  sid, title, summary,
                  jo_sv(it, "url") ? jo_sv(it, "url") : jo_sv(it, "comments_url"),
                  jo_sv(it, "created_at"), "en");
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[lobsters-newest] emitted %d\n", n);
  return 0;
}

static const source_def soc_lobsters_def = {
  .id = "lobsters-newest", .collector = "social",
  .name = "Lobsters Newest Stories",
  .update_interval_sec = 3600, .run = run_lobsters,
  .category = "media", .type = "api",
  .url = "https://lobste.rs/newest.json",
  .description = "Invite-only technical link aggregator with a security/infrastructure tag taxonomy — a high signal-to-noise early-warning feed for technical disclosure.",
  .license = "Lobsters publishes .json for every index page; keyless, descriptive User-Agent sent as the site asks.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_lobsters_def)

/* ---- 4. 4chan /news/ catalog -------------------------------------------- */

/* Decode the handful of XML entities 4chan escapes in `sub`/`com`, in place. */
static void unescape_ents(char *s) {
  char *w = s;
  for (char *r = s; *r; ) {
    if (*r == '&') {
      if (!strncmp(r, "&amp;", 5))       { *w++ = '&';  r += 5; continue; }
      if (!strncmp(r, "&lt;", 4))        { *w++ = '<';  r += 4; continue; }
      if (!strncmp(r, "&gt;", 4))        { *w++ = '>';  r += 4; continue; }
      if (!strncmp(r, "&quot;", 6))      { *w++ = '"';  r += 6; continue; }
      if (!strncmp(r, "&#039;", 6))      { *w++ = '\''; r += 6; continue; }
      if (!strncmp(r, "&#39;", 5))       { *w++ = '\''; r += 5; continue; }
    }
    *w++ = *r++;
  }
  *w = 0;
}

static int run_4chan_news(const source_ctx *ctx, intel_sink *sink) {
  /* 4chan's terms require If-Modified-Since. The shared http client does not
   * surface response headers, so we send the time of our previous successful
   * fetch (kept in-process) and honour a 304 as "nothing changed". */
  static time_t last_ok = 0;
  char ims[128];
  const char *hdrs[4];
  int hi = 0;
  hdrs[hi++] = "User-Agent: JapanOSINT-native/1.0 (OSINT research collector; +https://github.com/)";
  hdrs[hi++] = "Accept: application/json";
  if (last_ok) {
    struct tm g; gmtime_r(&last_ok, &g);
    strftime(ims, sizeof ims, "If-Modified-Since: %a, %d %b %Y %H:%M:%S GMT", &g);
    hdrs[hi++] = ims;
  }
  hdrs[hi] = NULL;

  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", "https://a.4cdn.org/news/catalog.json",
                        hdrs, NULL, 0, 25000, 1, &hr);
  if (rc == 0 && hr.status == 304) {
    http_response_free(&hr);
    fprintf(stderr, "[chan-4chan-news] 304 not modified\n");
    return 0;                                     /* honest empty, not error */
  }
  if (rc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[chan-4chan-news] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return -1;
  }
  cJSON *doc = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!cJSON_IsArray(doc)) {
    fprintf(stderr, "[chan-4chan-news] parse failed\n");
    cJSON_Delete(doc); return -1;
  }
  last_ok = time(NULL);

  int n = 0;
  const cJSON *page;
  cJSON_ArrayForEach(page, doc) {                 /* pages → threads: flatten */
    const cJSON *threads = cJSON_GetObjectItem(page, "threads"), *t;
    if (!cJSON_IsArray(threads)) continue;
    cJSON_ArrayForEach(t, threads) {
      const cJSON *nov = cJSON_GetObjectItem(t, "no");
      if (!cJSON_IsNumber(nov)) continue;
      long no = (long)nov->valuedouble;
      const char *sub = jo_sv(t, "sub");
      const char *com = jo_sv(t, "com");
      char *subtxt = sub ? html_strip(sub) : NULL;
      char *comtxt = com ? html_strip(com) : NULL;
      if (subtxt) unescape_ents(subtxt);
      if (comtxt) unescape_ents(comtxt);
      const char *title = (subtxt && subtxt[0]) ? subtxt
                        : ((comtxt && comtxt[0]) ? comtxt : NULL);
      if (!title) { free(subtxt); free(comtxt); continue; }
      const cJSON *tv = cJSON_GetObjectItem(t, "time");
      char iso[40]; iso[0] = 0;
      if (cJSON_IsNumber(tv)) soc_iso((time_t)tv->valuedouble, iso, sizeof iso);
      const cJSON *rep = cJSON_GetObjectItem(t, "replies");
      cJSON *p = cJSON_CreateObject();
      if (!p) { free(subtxt); free(comtxt); continue; }
      cJSON_AddStringToObject(p, "source", "a.4cdn.org");
      cJSON_AddStringToObject(p, "board", "news");
      jcopy(p, "no", t, "no");
      if (subtxt && subtxt[0]) cJSON_AddStringToObject(p, "subject", subtxt);
      if (comtxt && comtxt[0]) cJSON_AddStringToObject(p, "comment", comtxt);
      jcopy(p, "now", t, "now");
      jcopy(p, "replies", t, "replies");
      jcopy(p, "images", t, "images");
      jcopy(p, "last_modified", t, "last_modified");
      jcopy(p, "sticky", t, "sticky");
      jcopy(p, "closed", t, "closed");
      if (iso[0]) cJSON_AddStringToObject(p, "time", iso);
      char link[120], rk[64], summary[300];
      snprintf(link, sizeof link, "https://boards.4chan.org/news/thread/%ld", no);
      snprintf(rk, sizeof rk, "4chan-news-%ld", no);
      snprintf(summary, sizeof summary, "%ld replies%s%s",
               cJSON_IsNumber(rep) ? (long)rep->valuedouble : 0L,
               (comtxt && comtxt[0] && subtxt && subtxt[0]) ? " · " : "",
               (comtxt && comtxt[0] && subtxt && subtxt[0]) ? comtxt : "");
      char short_title[300];
      snprintf(short_title, sizeof short_title, "%.240s", title);
      n += soc_emit(sink, p, "imageboard-thread",
                    "[\"4chan\",\"imageboard\",\"news\"]",
                    rk, short_title, summary, link, iso[0] ? iso : NULL, NULL);
      free(subtxt); free(comtxt);
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[chan-4chan-news] emitted %d\n", n);
  return 0;
}

static const source_def soc_4chan_news_def = {
  .id = "chan-4chan-news", .collector = "social",
  .name = "4chan /news/ Thread Catalog",
  .update_interval_sec = 1800, .run = run_4chan_news,
  .category = "social", .type = "api",
  .url = "https://a.4cdn.org/news/catalog.json",
  .description = "Full thread catalog for 4chan's text-only /news/ board: every live thread with subject, opening post, post time and reply/image counts.",
  .license = "Official 4chan read-only API; terms require <=1 req/s and If-Modified-Since, both honoured here.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_4chan_news_def)

/* ---- 5/6. Discourse /latest.json (one parser, two hosts) ---------------- */

/* Resolve a poster user_id through the response's own users[] array. */
static const char *discourse_user(const cJSON *users, double id) {
  const cJSON *u;
  if (!cJSON_IsArray(users)) return NULL;
  cJSON_ArrayForEach(u, users) {
    const cJSON *uid = cJSON_GetObjectItem(u, "id");
    if (cJSON_IsNumber(uid) && uid->valuedouble == id) return jo_sv(u, "username");
  }
  return NULL;
}

static int discourse_latest(const source_ctx *ctx, intel_sink *sink,
                            const char *host, const char *sid) {
  char url[256];
  snprintf(url, sizeof url, "https://%s/latest.json", host);
  cJSON *doc = feed_get_json_h(ctx->http, url, SOC_UA, 20000);
  if (!doc) { fprintf(stderr, "[%s] fetch failed\n", sid); return -1; }
  const cJSON *users = cJSON_GetObjectItem(doc, "users");
  const cJSON *tl    = cJSON_GetObjectItem(doc, "topic_list");
  const cJSON *tops  = cJSON_IsObject(tl) ? cJSON_GetObjectItem(tl, "topics") : NULL;
  if (!cJSON_IsArray(tops)) {
    fprintf(stderr, "[%s] unexpected shape\n", sid);
    cJSON_Delete(doc); return -1;
  }
  int n = 0;
  const cJSON *t;
  cJSON_ArrayForEach(t, tops) {
    const char *title = jo_sv(t, "title");
    const cJSON *idv = cJSON_GetObjectItem(t, "id");
    if (!title || !cJSON_IsNumber(idv)) continue;
    long id = (long)idv->valuedouble;
    cJSON *p = cJSON_CreateObject();
    if (!p) continue;
    cJSON_AddStringToObject(p, "source", host);
    jcopy(p, "id", t, "id");
    jcopy(p, "title", t, "title");
    jcopy(p, "slug", t, "slug");
    jcopy(p, "created_at", t, "created_at");
    jcopy(p, "last_posted_at", t, "last_posted_at");
    jcopy(p, "posts_count", t, "posts_count");
    jcopy(p, "reply_count", t, "reply_count");
    jcopy(p, "views", t, "views");
    jcopy(p, "category_id", t, "category_id");
    jcopy(p, "tags", t, "tags");
    /* posters[].user_id → users[].username */
    cJSON *plist = cJSON_CreateArray();
    const cJSON *posters = cJSON_GetObjectItem(t, "posters"), *po;
    if (plist && cJSON_IsArray(posters)) cJSON_ArrayForEach(po, posters) {
      const cJSON *uid = cJSON_GetObjectItem(po, "user_id");
      if (!cJSON_IsNumber(uid)) continue;
      const char *un = discourse_user(users, uid->valuedouble);
      if (un) cJSON_AddItemToArray(plist, cJSON_CreateString(un));
    }
    if (plist) cJSON_AddItemToObject(p, "posters", plist);
    const cJSON *pc = cJSON_GetObjectItem(t, "posts_count");
    const cJSON *vw = cJSON_GetObjectItem(t, "views");
    char link[400], rk[128], summary[220];
    snprintf(link, sizeof link, "https://%s/t/%s/%ld", host,
             jo_sv(t, "slug") ? jo_sv(t, "slug") : "topic", id);
    snprintf(rk, sizeof rk, "%s:%ld", host, id);
    snprintf(summary, sizeof summary, "%ld posts · %ld views",
             cJSON_IsNumber(pc) ? (long)pc->valuedouble : 0L,
             cJSON_IsNumber(vw) ? (long)vw->valuedouble : 0L);
    n += soc_emit(sink, p, "forum-topic",
                  "[\"discourse\",\"forum\",\"topic\"]",
                  rk, title, summary, link, jo_sv(t, "created_at"), "en");
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

static int run_discourse_meta(const source_ctx *ctx, intel_sink *sink) {
  return discourse_latest(ctx, sink, "meta.discourse.org", "discourse-meta-latest");
}
static int run_discourse_python(const source_ctx *ctx, intel_sink *sink) {
  return discourse_latest(ctx, sink, "discuss.python.org", "discourse-python-latest");
}

static const source_def soc_discourse_meta_def = {
  .id = "discourse-meta-latest", .collector = "social",
  .name = "Discourse Meta Latest Topics",
  .update_interval_sec = 3600, .run = run_discourse_meta,
  .category = "social", .type = "api",
  .url = "https://meta.discourse.org/latest.json",
  .description = "Latest-topics feed of the reference Discourse forum — title, author, post/view counts and tags. The same parser works against any public Discourse install.",
  .license = "Discourse ships /latest.json as an unauthenticated public read endpoint; keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_discourse_meta_def)

static const source_def soc_discourse_python_def = {
  .id = "discourse-python-latest", .collector = "social",
  .name = "discuss.python.org Latest Topics",
  .update_interval_sec = 3600, .run = run_discourse_python,
  .category = "social", .type = "api",
  .url = "https://discuss.python.org/latest.json",
  .description = "Official Python governance and packaging forum — PEP debates, PyPI policy and supply-chain incidents surface here before they reach news feeds.",
  .license = "Public Discourse instance run by the PSF; /latest.json is unauthenticated.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(soc_discourse_python_def)
