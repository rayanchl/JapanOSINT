/* collectors/sources/social_world2.c
 * OSINT service — YouTube Data API search (key-gated), pivot on ctx->entity
 * (keyword/handle). Honest-empty when YOUTUBE_API_KEY unset.
 *   YOUTUBE_SEARCH  googleapis.com/youtube/v3/search?part=snippet&q= */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *SO_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  if (strcmp(ctx->source_id ? ctx->source_id : "", "YOUTUBE_SEARCH") != 0) return 0;
  const char *k = jo_env("YOUTUBE_API_KEY");
  if (!k) { fprintf(stderr, "[YOUTUBE_SEARCH] no YOUTUBE_API_KEY\n"); return 0; }
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[640];
  snprintf(url, sizeof url,
    "https://www.googleapis.com/youtube/v3/search?part=snippet&type=video&maxResults=20&q=%s&key=%s", enc, k);
  free(enc);
  const char *hdrs[] = { "Accept: application/json", SO_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "YOUTUBE_SEARCH");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *items = cJSON_GetObjectItem(root, "items");
  int emitted = 0;
  if (cJSON_IsArray(items)) {
    const cJSON *it0;
    cJSON_ArrayForEach(it0, items) {
      const cJSON *idobj = cJSON_GetObjectItem(it0, "id");
      const char *vid = idobj ? jo_sv(idobj, "videoId") : NULL;
      const cJSON *sn = cJSON_GetObjectItem(it0, "snippet");
      const char *title = sn ? jo_sv(sn, "title") : NULL;
      const char *channel = sn ? jo_sv(sn, "channelTitle") : NULL;
      const char *pub = sn ? jo_sv(sn, "publishedAt") : NULL;
      const char *desc = sn ? jo_sv(sn, "description") : NULL;
      if (!vid || !title) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "title", title);
      if (channel) cJSON_AddStringToObject(d, "channel", channel);
      if (desc) cJSON_AddStringToObject(d, "description", desc);
      cJSON_AddStringToObject(d, "video_id", vid);
      cJSON_AddStringToObject(d, "source", "YouTube");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "YOUTUBE_SEARCH");
      cJSON_AddStringToObject(p, "video_id", vid);
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char link[128]; snprintf(link, sizeof link, "https://www.youtube.com/watch?v=%s", vid);
      intel_item it = {0};
      it.remote_key = vid; it.title = title; it.summary = channel; it.body = bj;
      it.author = channel; it.link = link; it.lang = "en"; it.published_at = pub;
      it.record_type = "youtube-video";
      it.properties_json = pj; it.tags_json = "[\"osint-search\",\"YOUTUBE_SEARCH\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[YOUTUBE_SEARCH] emitted %d\n", emitted);
  return 0;
}

static const source_def youtube_search_def = {
  .id = "YOUTUBE_SEARCH", .collector = "osint",
  .name = "YouTube Search", .name_ja = "YouTube 検索",
  .update_interval_sec = 0, .run = run,
  .category = "social", .type = "api",
  .url = "https://www.googleapis.com/youtube/v3",
  .description = "YouTube Data API (YOUTUBE_API_KEY): video search by keyword (title, channel, date).",
  .layer = NULL, .free_tier = 0,
};
REGISTER_SOURCE(youtube_search_def)
