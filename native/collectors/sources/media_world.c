/* collectors/sources/media_world.c
 * OSINT services — global media / reference search. On-demand entity pivot
 * (ctx->entity) against four keyless, LIVE public endpoints. One run()
 * dispatches on ctx->source_id; every branch REAL-fetches and emits only
 * parsed real records, else honest-empty (return 0).
 *
 *   WIKINEWS         en.wikinews.org OpenSearch — matched article titles+URLs
 *   GDELT_TV         api.gdeltproject.org/api/v2/tv — TV news clip mentions
 *   COMMONCRAWL_CDX  index.commoncrawl.org — CDX crawl records for a domain
 *   GOOGLE_BOOKS     googleapis.com/books/v1/volumes — matched book volumes
 *
 * All keyless. Nothing synthesized: titles, URLs, dates, authors are extracted
 * from the live JSON. Fetch failure / no match → honest empty. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- WIKINEWS: OpenSearch --------------------------------------------- *
 * Response is a 4-element array: [ query, [titles], [descriptions], [urls] ].
 * Emit one item per title, linked to its real article URL. */
static int mw_wikinews(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://en.wikinews.org/w/api.php?action=opensearch&limit=20&namespace=0"
    "&format=json&search=%s", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (osint@example.org)", NULL };
  char *body = jo_get(ctx, url, hdrs, "WIKINEWS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!cJSON_IsArray(root)) { cJSON_Delete(root); return 0; }

  cJSON *titles = cJSON_GetArrayItem(root, 1);
  cJSON *descs  = cJSON_GetArrayItem(root, 2);
  cJSON *urls   = cJSON_GetArrayItem(root, 3);
  int emitted = 0;
  int n = cJSON_IsArray(titles) ? cJSON_GetArraySize(titles) : 0;
  for (int i = 0; i < n; i++) {
    cJSON *t = cJSON_GetArrayItem(titles, i);
    if (!cJSON_IsString(t) || !t->valuestring[0]) continue;
    cJSON *d = descs ? cJSON_GetArrayItem(descs, i) : NULL;
    cJSON *u = urls  ? cJSON_GetArrayItem(urls, i)  : NULL;
    const char *desc = (d && cJSON_IsString(d) && d->valuestring[0]) ? d->valuestring : NULL;
    const char *link = (u && cJSON_IsString(u) && u->valuestring[0]) ? u->valuestring : NULL;

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "WIKINEWS");
    cJSON_AddStringToObject(props, "query", q);
    if (link) cJSON_AddStringToObject(props, "url", link);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);

    intel_item it = {0};
    it.remote_key      = link ? link : t->valuestring;
    it.title           = t->valuestring;
    it.summary         = desc;
    it.link            = link;
    it.lang            = "en";
    it.record_type     = "wikinews-article";
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"WIKINEWS\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(pj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[WIKINEWS] emitted %d\n", emitted);
  return emitted;
}

/* ---- GDELT TV: clip gallery ------------------------------------------- *
 * /api/v2/tv/tv?query=<q>&mode=clipgallery&format=json → { clips: [ { show,
 * station, date, preview_url, snippet, ... } ] } (field names per the public
 * GDELT TV 2.0 API). Emit one item per clip. */
static int mw_gdelt_tv(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  /* The TV 2.0 API refuses any query without a station/market scope — it
   * answers 200 with the plain-text body "Your query must contain at least
   * one station.", which is not JSON, so the parse failed silently and the
   * service looked empty for every entity. market:"National" is the API's
   * own group for the national networks. */
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.gdeltproject.org/api/v2/tv/tv"
    "?query=%s%%20market%%3A%%22National%%22&mode=clipgallery"
    "&format=json&maxrecords=25", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (osint@example.org)", NULL };
  /* GDELT scans the whole Internet Archive TV corpus for a clipgallery and
   * reliably takes 14-18s; jo_get()'s shared 20s budget was losing the race
   * (status=0, transport timeout) so the service always looked empty. Do the
   * request here with a budget that fits the real upstream latency. */
  char *body = NULL;
  { http_response hr = {0};
    int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 45000, 0, &hr);
    if (rc != 0 || hr.status != 200 || !hr.body) {
      fprintf(stderr, "[GDELT_TV] http status=%ld\n", hr.status);
      http_response_free(&hr);
    } else { body = hr.body; hr.body = NULL; http_response_free(&hr); } }
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *clips = cJSON_GetObjectItem(root, "clips");
  int emitted = 0;
  if (cJSON_IsArray(clips)) {
    cJSON *c;
    cJSON_ArrayForEach(c, clips) {
      const char *show    = jo_sv(c, "show");
      const char *station = jo_sv(c, "station");
      const char *date    = jo_sv(c, "date");
      const char *prev    = jo_sv(c, "preview_url");
      if (!prev) prev = jo_sv(c, "preview_thumb");
      const char *snip    = jo_sv(c, "snippet");
      const char *link    = jo_sv(c, "show_url");
      if (!link) link = prev;
      if (!show && !station && !snip) continue;

      char title[512];
      snprintf(title, sizeof title, "%s%s%s",
               station ? station : "TV",
               show ? " — " : "", show ? show : "");

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "GDELT_TV");
      cJSON_AddStringToObject(props, "query", q);
      if (station) cJSON_AddStringToObject(props, "station", station);
      if (show)    cJSON_AddStringToObject(props, "show", show);
      if (date)    cJSON_AddStringToObject(props, "date", date);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      char key[600];
      snprintf(key, sizeof key, "%s|%s|%s", station ? station : "",
               show ? show : "", date ? date : (link ? link : title));

      intel_item it = {0};
      it.remote_key      = key;
      it.title           = title;
      it.summary         = snip;
      it.link            = link;
      it.published_at    = date;
      it.lang            = "en";
      it.record_type     = "gdelt-tv-clip";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"GDELT_TV\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(pj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[GDELT_TV] emitted %d\n", emitted);
  return emitted;
}

/* ---- COMMONCRAWL CDX --------------------------------------------------- *
 * index.commoncrawl.org/<INDEX>-index?url=<domain>/[wildcard]&output=json returns one
 * JSON object per line (JSONL): { url, timestamp, mime, status, digest,
 * length, ... }. entity = a domain (or host). Emit one item per crawl record.
 * We treat the entity literally as the host to query. */
static int mw_commoncrawl(const source_ctx *ctx, intel_sink *sink, const char *q) {
  /* strip any scheme/path from the entity so it's a bare host+wildcard */
  const char *host = q;
  if (strncmp(host, "http://", 7) == 0)  host += 7;
  else if (strncmp(host, "https://", 8) == 0) host += 8;
  char hb[512]; size_t hj = 0;
  for (const char *p = host; *p && *p != '/' && hj < sizeof hb - 1; p++) hb[hj++] = *p;
  hb[hj] = 0;
  if (!hb[0] || !strchr(hb, '.')) {
    fprintf(stderr, "[COMMONCRAWL_CDX] entity not a domain, skipping\n");
    return 0;
  }
  char pat[600];
  snprintf(pat, sizeof pat, "%s/*", hb);
  char *enc = jo_urlencode(pat);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://index.commoncrawl.org/CC-MAIN-2024-51-index?url=%s&output=json&limit=50",
    enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (osint@example.org)", NULL };
  char *body = jo_get(ctx, url, hdrs, "COMMONCRAWL_CDX");
  if (!body) return 0;

  int emitted = 0;
  char *line = body;
  while (line && *line) {
    char *nl = strchr(line, '\n');
    if (nl) *nl = 0;
    if (*line) {
      cJSON *o = cJSON_Parse(line);
      if (o) {
        const char *rurl = jo_sv(o, "url");
        const char *ts   = jo_sv(o, "timestamp");
        const char *mime = jo_sv(o, "mime");
        const char *stat = jo_sv(o, "status");
        const char *dig  = jo_sv(o, "digest");
        if (rurl) {
          cJSON *props = cJSON_CreateObject();
          cJSON_AddStringToObject(props, "service", "COMMONCRAWL_CDX");
          cJSON_AddStringToObject(props, "query", hb);
          if (mime) cJSON_AddStringToObject(props, "mime", mime);
          if (stat) cJSON_AddStringToObject(props, "status", stat);
          if (ts)   cJSON_AddStringToObject(props, "timestamp", ts);
          cJSON_AddBoolToObject(props, "success", 1);
          char *pj = cJSON_PrintUnformatted(props);
          cJSON_Delete(props);

          char key[900];
          snprintf(key, sizeof key, "%s|%s", ts ? ts : "", dig ? dig : rurl);

          intel_item it = {0};
          it.remote_key      = key;
          it.title           = rurl;
          it.summary         = mime;
          it.link            = rurl;
          it.published_at    = ts;
          it.lang            = "en";
          it.record_type     = "commoncrawl-record";
          it.properties_json = pj;
          it.tags_json       = "[\"osint-search\",\"COMMONCRAWL_CDX\"]";
          if (sink->emit(sink, &it) >= 0) emitted++;
          free(pj);
        }
        cJSON_Delete(o);
      }
    }
    if (!nl) break;
    line = nl + 1;
  }
  free(body);
  fprintf(stderr, "[COMMONCRAWL_CDX] emitted %d\n", emitted);
  return emitted;
}

/* ---- GOOGLE BOOKS ------------------------------------------------------ *
 * volumes?q=<entity> → { items: [ { id, volumeInfo: { title, authors[],
 * publishedDate, description, infoLink, ... } } ] }. Keyless. */
static int mw_google_books(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://www.googleapis.com/books/v1/volumes?q=%s&maxResults=20&country=US", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (osint@example.org)", NULL };
  char *body = jo_get(ctx, url, hdrs, "GOOGLE_BOOKS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *items = cJSON_GetObjectItem(root, "items");
  int emitted = 0;
  if (cJSON_IsArray(items)) {
    cJSON *item;
    cJSON_ArrayForEach(item, items) {
      const char *id = jo_sv(item, "id");
      cJSON *vi = cJSON_GetObjectItem(item, "volumeInfo");
      if (!vi) continue;
      const char *title = jo_sv(vi, "title");
      if (!title) continue;
      const char *pub   = jo_sv(vi, "publishedDate");
      const char *desc  = jo_sv(vi, "description");
      const char *link  = jo_sv(vi, "infoLink");
      const char *pubr  = jo_sv(vi, "publisher");

      /* authors[] → "A, B, C" */
      char authors[512]; authors[0] = 0; size_t aj = 0;
      cJSON *auths = cJSON_GetObjectItem(vi, "authors");
      if (cJSON_IsArray(auths)) {
        cJSON *a; int na = 0;
        cJSON_ArrayForEach(a, auths) {
          if (!cJSON_IsString(a) || !a->valuestring[0]) continue;
          size_t need = aj + strlen(a->valuestring) + 3;
          if (need >= sizeof authors) break;
          if (na) { authors[aj++] = ','; authors[aj++] = ' '; }
          strcpy(authors + aj, a->valuestring); aj += strlen(a->valuestring);
          na++;
        }
        authors[aj] = 0;
      }

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "title", title);
      if (authors[0]) cJSON_AddStringToObject(data, "authors", authors);
      if (pub)  cJSON_AddStringToObject(data, "published", pub);
      if (pubr) cJSON_AddStringToObject(data, "publisher", pubr);
      if (id)   cJSON_AddStringToObject(data, "volume_id", id);
      cJSON_AddStringToObject(data, "source", "Google Books");
      char *bj = cJSON_PrintUnformatted(data);
      cJSON_Delete(data);

      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "service", "GOOGLE_BOOKS");
      cJSON_AddStringToObject(props, "query", q);
      if (authors[0]) cJSON_AddStringToObject(props, "authors", authors);
      if (pub) cJSON_AddStringToObject(props, "published", pub);
      cJSON_AddBoolToObject(props, "success", 1);
      char *pj = cJSON_PrintUnformatted(props);
      cJSON_Delete(props);

      intel_item it = {0};
      it.remote_key      = id ? id : title;
      it.title           = title;
      it.summary         = authors[0] ? authors : desc;
      it.body            = bj;
      it.author          = authors[0] ? authors : NULL;
      it.link            = link;
      it.published_at    = pub;
      it.lang            = "en";
      it.record_type     = "google-books-volume";
      it.properties_json = pj;
      it.tags_json       = "[\"osint-search\",\"GOOGLE_BOOKS\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[GOOGLE_BOOKS] emitted %d\n", emitted);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  if (!q) return -1;

  if (strcmp(ctx->source_id, "WIKINEWS") == 0)        mw_wikinews(ctx, sink, q);
  else if (strcmp(ctx->source_id, "GDELT_TV") == 0)   mw_gdelt_tv(ctx, sink, q);
  else if (strcmp(ctx->source_id, "COMMONCRAWL_CDX") == 0) mw_commoncrawl(ctx, sink, q);
  else if (strcmp(ctx->source_id, "GOOGLE_BOOKS") == 0) mw_google_books(ctx, sink, q);
  else return -1;
  return 0;   /* honest empty is not an error */
}

#define MW_DEF(SYM, ID, NAME, NAMEJA, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = run, \
    .category = "news", .type = "api", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

MW_DEF(mw_wikinews_def, "WIKINEWS", "Wikinews Search", "ウィキニュース検索",
  "https://en.wikinews.org",
  "Wikinews OpenSearch (keyless) — matched article titles + URLs for an entity");
MW_DEF(mw_gdelt_tv_def, "GDELT_TV", "GDELT TV News", "GDELT テレビニュース",
  "https://api.gdeltproject.org/api/v2/tv/tv",
  "GDELT TV 2.0 (keyless) — US TV news clip mentions of an entity");
MW_DEF(mw_cc_def, "COMMONCRAWL_CDX", "Common Crawl Index", "Common Crawl インデックス",
  "https://index.commoncrawl.org",
  "Common Crawl CDX (keyless) — crawl records (url, timestamp, mime) for a domain");
MW_DEF(mw_gbooks_def, "GOOGLE_BOOKS", "Google Books Search", "Google ブックス検索",
  "https://www.googleapis.com/books/v1/volumes",
  "Google Books volumes (keyless) — matched book title, authors, publish date");
