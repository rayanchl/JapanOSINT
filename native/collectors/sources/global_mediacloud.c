/* collectors/sources/global_mediacloud.c
 * OSINT service — Media Cloud (search.mediacloud.org). On-demand entity pivot
 * (person / company / any term) against Media Cloud's global news corpus.
 *
 * GET https://search.mediacloud.org/api/search/story-list?q=<entity>
 * Requires a Media Cloud API key sent as "Authorization: Token <key>" —
 * gated on MEDIACLOUD_API_KEY (no key → honest empty, return 0, like
 * gbizinfo.c). One intel_item per matched story: title, media, url, date.
 * Non-200 / no matches → honest empty. Never synthesizes records. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* one story → one intel_item. Returns 1 if emitted. */
static int emit_story(intel_sink *sink, const cJSON *s) {
  if (!s) return 0;
  const char *title = jo_sv(s, "title");
  const char *url   = jo_sv(s, "url");
  if (!url) url = jo_sv(s, "story_url");
  if (!title && !url) return 0;

  const char *media = jo_sv(s, "media_name");
  if (!media) media = jo_sv(s, "media_url");
  const char *date  = jo_sv(s, "publish_date");
  if (!date) date = jo_sv(s, "published_date");
  const char *lang  = jo_sv(s, "language");

  /* stable key: prefer numeric story id if present */
  const cJSON *idv = cJSON_GetObjectItem(s, "id");
  char idbuf[64] = {0};
  const char *rkey = url ? url : title;
  if (cJSON_IsNumber(idv)) {
    snprintf(idbuf, sizeof idbuf, "mediacloud:%lld", (long long)idv->valuedouble);
    rkey = idbuf;
  } else {
    const char *ids = jo_sv(s, "id");
    if (ids) rkey = ids;
  }

  cJSON *data = cJSON_CreateObject();
  if (title) cJSON_AddStringToObject(data, "title", title);
  if (media) cJSON_AddStringToObject(data, "media", media);
  if (date)  cJSON_AddStringToObject(data, "publish_date", date);
  if (url)   cJSON_AddStringToObject(data, "url", url);
  if (lang)  cJSON_AddStringToObject(data, "language", lang);
  cJSON_AddStringToObject(data, "source", "Media Cloud");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "MEDIACLOUD");
  cJSON_AddStringToObject(props, "source", "Media Cloud");
  if (media) cJSON_AddStringToObject(props, "media", media);
  if (date)  cJSON_AddStringToObject(props, "publish_date", date);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = rkey;
  it.title           = title ? title : url;
  it.summary         = media;
  it.body            = bj;
  it.lang            = lang;
  it.published_at    = date;
  it.link            = url;
  it.record_type     = "mediacloud-story";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"MEDIACLOUD\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  const char *key = jo_env("MEDIACLOUD_API_KEY");
  if (!key) {
    fprintf(stderr, "[mediacloud] gated (no MEDIACLOUD_API_KEY)\n");
    return 0;
  }

  char *enc = jo_urlencode(q);
  if (!enc) return -1;
  char url[1024];
  snprintf(url, sizeof url,
           "https://search.mediacloud.org/api/search/story-list?q=%s", enc);
  free(enc);

  char authhdr[256];
  snprintf(authhdr, sizeof authhdr, "Authorization: Token %s", key);
  const char *hdrs[] = { authhdr, "Accept: application/json", NULL };

  char *body = jo_get(ctx, url, hdrs, "mediacloud");
  if (!body) return 0;                 /* non-200 → honest empty */
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  /* story-list responses put the array under "stories" (fallback "results"). */
  cJSON *stories = cJSON_GetObjectItem(root, "stories");
  if (!cJSON_IsArray(stories)) stories = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(stories)) {
    cJSON *s; cJSON_ArrayForEach(s, stories) emitted += emit_story(sink, s);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[mediacloud] emitted %d\n", emitted);
  return 0;
}

static const source_def global_mediacloud_def = {
  .id = "MEDIACLOUD", .collector = "osint",
  .name = "Media Cloud News Search", .name_ja = "Media Cloud ニュース検索",
  .update_interval_sec = 0, .run = run,
  .category = "news", .type = "api",
  .url = "https://search.mediacloud.org/api/search/story-list",
  .description = "Media Cloud — global news corpus story search (needs MEDIACLOUD_API_KEY)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(global_mediacloud_def)
