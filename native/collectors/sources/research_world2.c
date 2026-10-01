/* collectors/sources/research_world2.c
 * OSINT services — net-new keyless mention/discussion search pivoting on
 * ctx->entity (a name, handle, domain, or keyword). Shared run() switches on
 * ctx->source_id. Real fetch or honest-empty. Keyless (CORE is key-gated).
 *
 *   WIKIPEDIA_SEARCH   en.wikipedia.org/w/api.php list=search        (JSON)
 *   HACKERNEWS_SEARCH  hn.algolia.com/api/v1/search?query=<e>         (JSON)
 *   REDDIT_PULLPUSH    api.pullpush.io/reddit/search/submission?q=<e> (JSON)
 *   CORE_SEARCH        api.core.ac.uk/v3/search/works (CORE_API_KEY)  (JSON) */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *RW_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

/* strip HTML tags in-place (Wikipedia snippet has <span> highlights). */
static void rw_striptags(char *s) {
  if (!s) return;
  char *w = s; int in = 0;
  for (char *r = s; *r; r++) {
    if (*r == '<') in = 1;
    else if (*r == '>') in = 0;
    else if (!in) *w++ = *r;
  }
  *w = 0;
}

static int run_wikipedia(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[640];
  snprintf(url, sizeof url,
    "https://en.wikipedia.org/w/api.php?action=query&list=search&format=json"
    "&srlimit=20&srsearch=%s", enc);
  const char *hdrs[] = { "Accept: application/json", RW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "WIKIPEDIA_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *query = cJSON_GetObjectItem(root, "query");
  const cJSON *search = query ? cJSON_GetObjectItem(query, "search") : NULL;
  int emitted = 0;
  if (cJSON_IsArray(search)) {
    const cJSON *s;
    cJSON_ArrayForEach(s, search) {
      const char *title = jo_sv(s, "title");
      const cJSON *pid = cJSON_GetObjectItem(s, "pageid");
      char *snippet = NULL;
      const cJSON *sn = cJSON_GetObjectItem(s, "snippet");
      if (cJSON_IsString(sn) && sn->valuestring) { snippet = strdup(sn->valuestring); rw_striptags(snippet); }
      if (!title) { free(snippet); continue; }
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "title", title);
      if (snippet) cJSON_AddStringToObject(d, "snippet", snippet);
      if (pid && cJSON_IsNumber(pid)) cJSON_AddNumberToObject(d, "pageid", pid->valuedouble);
      cJSON_AddStringToObject(d, "source", "Wikipedia");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "WIKIPEDIA_SEARCH");
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char link[512]; { char *e = jo_urlencode(title);
        snprintf(link, sizeof link, "https://en.wikipedia.org/wiki/%s", e ? e : ""); free(e); }
      intel_item it = {0};
      it.remote_key = title; it.title = title; it.summary = snippet;
      it.body = bj; it.link = link; it.lang = "en";
      it.record_type = "wikipedia-article";
      it.properties_json = pj; it.tags_json = "[\"osint-search\",\"WIKIPEDIA_SEARCH\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj); free(snippet);
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_hackernews(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://hn.algolia.com/api/v1/search?query=%s&hitsPerPage=20", enc);
  const char *hdrs[] = { "Accept: application/json", RW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "HACKERNEWS_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *hits = cJSON_GetObjectItem(root, "hits");
  int emitted = 0;
  if (cJSON_IsArray(hits)) {
    const cJSON *h;
    cJSON_ArrayForEach(h, hits) {
      const char *title = jo_sv(h, "title");
      const char *story = jo_sv(h, "story_title");
      const char *urlf = jo_sv(h, "url");
      const char *author = jo_sv(h, "author");
      const char *oid = jo_sv(h, "objectID");
      const char *created = jo_sv(h, "created_at");
      const cJSON *pts = cJSON_GetObjectItem(h, "points");
      const char *ttl = title ? title : story;
      if (!ttl && !oid) continue;
      cJSON *d = cJSON_CreateObject();
      if (ttl) cJSON_AddStringToObject(d, "title", ttl);
      if (author) cJSON_AddStringToObject(d, "author", author);
      if (urlf) cJSON_AddStringToObject(d, "url", urlf);
      if (pts && cJSON_IsNumber(pts)) cJSON_AddNumberToObject(d, "points", pts->valuedouble);
      cJSON_AddStringToObject(d, "source", "Hacker News");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "HACKERNEWS_SEARCH");
      if (oid) cJSON_AddStringToObject(p, "object_id", oid);
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char link[256];
      if (oid) snprintf(link, sizeof link, "https://news.ycombinator.com/item?id=%s", oid);
      else snprintf(link, sizeof link, "%s", urlf ? urlf : "");
      intel_item it = {0};
      it.remote_key = oid ? oid : ttl; it.title = ttl ? ttl : oid;
      it.summary = author; it.body = bj; it.author = author;
      it.link = link[0] ? link : NULL; it.lang = "en"; it.published_at = created;
      it.record_type = "hackernews-item";
      it.properties_json = pj; it.tags_json = "[\"osint-search\",\"HACKERNEWS_SEARCH\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_pullpush(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url,
    "https://api.pullpush.io/reddit/search/submission/?q=%s&size=20&sort=desc", enc);
  const char *hdrs[] = { "Accept: application/json", RW_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "REDDIT_PULLPUSH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *data = cJSON_GetObjectItem(root, "data");
  int emitted = 0;
  if (cJSON_IsArray(data)) {
    const cJSON *s;
    cJSON_ArrayForEach(s, data) {
      const char *title = jo_sv(s, "title");
      const char *author = jo_sv(s, "author");
      const char *sub = jo_sv(s, "subreddit");
      const char *full = jo_sv(s, "full_link");
      const char *perm = jo_sv(s, "permalink");
      const char *self = jo_sv(s, "selftext");
      const char *id = jo_sv(s, "id");
      if (!title && !id) continue;
      cJSON *d = cJSON_CreateObject();
      if (title) cJSON_AddStringToObject(d, "title", title);
      if (author) cJSON_AddStringToObject(d, "author", author);
      if (sub) cJSON_AddStringToObject(d, "subreddit", sub);
      if (self && self[0]) { char snip[281]; snprintf(snip, sizeof snip, "%.280s", self); cJSON_AddStringToObject(d, "selftext", snip); }
      cJSON_AddStringToObject(d, "source", "Reddit (PullPush)");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "REDDIT_PULLPUSH");
      if (sub) cJSON_AddStringToObject(p, "subreddit", sub);
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char link[512];
      if (full) snprintf(link, sizeof link, "%s", full);
      else if (perm) snprintf(link, sizeof link, "https://www.reddit.com%s", perm);
      else link[0] = 0;
      char summ[128]; snprintf(summ, sizeof summ, "r/%s · u/%s", sub ? sub : "?", author ? author : "?");
      intel_item it = {0};
      it.remote_key = id ? id : title; it.title = title ? title : id;
      it.summary = summ; it.body = bj; it.author = author;
      it.link = link[0] ? link : NULL; it.lang = "en";
      it.record_type = "reddit-submission";
      it.properties_json = pj; it.tags_json = "[\"osint-search\",\"REDDIT_PULLPUSH\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  return emitted;
}

/* CORE (open-access research aggregator) — key-gated: honest-empty w/o CORE_API_KEY. */
static int run_core(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *key = jo_env("CORE_API_KEY");
  if (!key) { fprintf(stderr, "[CORE_SEARCH] no CORE_API_KEY\n"); return 0; }
  char url[512];
  snprintf(url, sizeof url, "https://api.core.ac.uk/v3/search/works?q=%s&limit=20", enc);
  char auth[160]; snprintf(auth, sizeof auth, "Authorization: Bearer %s", key);
  const char *hdrs[] = { "Accept: application/json", RW_UA, auth, NULL };
  char *body = jo_get(ctx, url, hdrs, "CORE_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const char *title = jo_sv(r, "title");
      const char *doi = jo_sv(r, "doi");
      const char *dl = jo_sv(r, "downloadUrl");
      if (!title) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "title", title);
      if (doi) cJSON_AddStringToObject(d, "doi", doi);
      cJSON_AddStringToObject(d, "source", "CORE");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "CORE_SEARCH");
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      intel_item it = {0};
      it.remote_key = doi ? doi : title; it.title = title; it.body = bj;
      it.link = dl; it.lang = "en"; it.record_type = "core-work";
      it.properties_json = pj; it.tags_json = "[\"osint-search\",\"CORE_SEARCH\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
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
  if      (!strcmp(sid, "WIKIPEDIA_SEARCH"))  n = run_wikipedia(ctx, sink, enc);
  else if (!strcmp(sid, "HACKERNEWS_SEARCH")) n = run_hackernews(ctx, sink, enc);
  else if (!strcmp(sid, "REDDIT_PULLPUSH"))   n = run_pullpush(ctx, sink, enc);
  else if (!strcmp(sid, "CORE_SEARCH"))       n = run_core(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define RW_DEF(sym, ID, NM, FREE, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "investigation", \
    .type = "api", .url = "internal://osint/research", .description = DESC, \
    .layer = NULL, .free_tier = FREE }; \
  REGISTER_SOURCE(sym)

RW_DEF(rw_wikipedia_def, "WIKIPEDIA_SEARCH", "Wikipedia Search", 1,
  "Wikipedia keyless full-text search (title, snippet, pageid).");
RW_DEF(rw_hackernews_def, "HACKERNEWS_SEARCH", "Hacker News Search", 1,
  "Hacker News (Algolia) keyless search of stories/comments by keyword.");
RW_DEF(rw_pullpush_def, "REDDIT_PULLPUSH", "Reddit PullPush Search", 1,
  "PullPush (Pushshift successor) keyless Reddit submission search.");
RW_DEF(rw_core_def, "CORE_SEARCH", "CORE Research Search", 1,
  "CORE open-access research aggregator (requires CORE_API_KEY).");
