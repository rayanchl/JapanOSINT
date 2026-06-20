/* collectors/classifieds/sources/mercari_trending.c
 * Port of server/src/collectors/mercariTrending.js. Mercari internal trending
 * keywords endpoint → intel items. uid = mercari-trending|<keyword>
 * (== intelUid(SOURCE_ID, keyword, `rank_${i+1}`)). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRENDS_URL "https://jp.mercari.com/v1/web/suggest/trends"

/* encodeURIComponent: keep A-Za-z0-9 and - _ . ! ~ * ' ( ); %-encode rest. */
static void urlenc(const char *s, char *o, size_t n) {
  static const char hex[] = "0123456789ABCDEF";
  size_t j = 0;
  for (; *s && j + 4 < n; s++) {
    unsigned char c = (unsigned char)*s;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')') {
      o[j++] = (char)c;
    } else {
      o[j++] = '%'; o[j++] = hex[c >> 4]; o[j++] = hex[c & 15];
    }
  }
  o[j] = '\0';
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "User-Agent: JapanOSINT/1.0", NULL };
  cJSON *data = feed_get_json_h(ctx->http, TRENDS_URL, hdrs, 12000);
  if (!data) return -1;

  /* list = Array.isArray(data?.trends) ? data.trends
   *      : Array.isArray(data) ? data : [] */
  cJSON *list = NULL;
  cJSON *tr = cJSON_GetObjectItem(data, "trends");
  if (cJSON_IsArray(tr)) list = tr;
  else if (cJSON_IsArray(data)) list = data;

  int n = 0, i = 0;
  if (list) {
    cJSON *it;
    cJSON_ArrayForEach(it, list) {
      if (i >= 50) break;                        /* slice(0,50) */
      int rank = i + 1;
      i++;

      /* keyword = typeof it==='string' ? it : (it.keyword || it.name || null) */
      const char *keyword = NULL;
      if (cJSON_IsString(it)) keyword = it->valuestring;
      else {
        cJSON *kw = cJSON_GetObjectItem(it, "keyword");
        if (kw && cJSON_IsString(kw) && kw->valuestring[0]) keyword = kw->valuestring;
        else {
          cJSON *nm = cJSON_GetObjectItem(it, "name");
          if (nm && cJSON_IsString(nm) && nm->valuestring[0]) keyword = nm->valuestring;
        }
      }
      if (!keyword || !*keyword) continue;        /* .filter(Boolean) */

      char enc[512]; urlenc(keyword, enc, sizeof enc);
      char link[640];
      snprintf(link, sizeof link, "https://jp.mercari.com/search?keyword=%s", enc);
      char summ[64];
      snprintf(summ, sizeof summ, "Trending #%d", rank);
      char tags[96];
      snprintf(tags, sizeof tags, "[\"mercari\",\"trending\",\"rank:%d\"]", rank);

      cJSON *pj = cJSON_CreateObject();           /* {keyword, rank} */
      cJSON_AddStringToObject(pj, "keyword", keyword);
      cJSON_AddNumberToObject(pj, "rank", rank);
      char *pjs = cJSON_PrintUnformatted(pj);

      intel_item item = {0};
      item.remote_key = keyword;                  /* uid = mercari-trending|<kw> */
      item.title = keyword;
      item.summary = summ;
      item.link = link;
      item.lang = "ja";
      item.record_type = "mercari-trending";
      item.properties_json = pjs;
      item.tags_json = tags;
      if (sink->emit(sink, &item) >= 0) n++;
      free(pjs); cJSON_Delete(pj);
    }
  }
  cJSON_Delete(data);
  fprintf(stderr, "[mercari-trending] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def mercari_trending_def = {
  .id = "mercari-trending", .collector = "classifieds",
  .name = "Mercari Trending", .name_ja = "メルカリ トレンド",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(mercari_trending_def)
