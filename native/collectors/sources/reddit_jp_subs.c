/* collectors/social/sources/reddit_jp_subs.c
 * Port of server/src/collectors/redditJpSubs.js (intelEnvelope).
 * 5 fixed subs, one /new.json?limit=25 GET each, slice(0,25) → intel rows.
 * uid = reddit-jp-subs|<p.id> (intelUid first non-empty). */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *SUBS[] = { "japan", "japanlife", "Tokyo",
                              "newsokur", "JapanFinance" };
#define NSUB ((int)(sizeof(SUBS) / sizeof(SUBS[0])))

/* new Date(sec*1000).toISOString() → YYYY-MM-DDTHH:MM:SS.mmmZ */
static void epoch_iso(double sec, char *o, size_t n) {
  double ms = (sec - (double)(long long)sec) * 1000.0;
  int msi = (int)(ms + 0.5);
  time_t t = (time_t)(long long)sec;
  if (msi >= 1000) { msi -= 1000; t += 1; }
  struct tm tm;
  gmtime_r(&t, &tm);
  char base[32];
  strftime(base, sizeof base, "%Y-%m-%dT%H:%M:%S", &tm);
  snprintf(o, n, "%s.%03dZ", base, msi);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "User-Agent: JapanOSINT/1.0 (research)", NULL };
  int n = 0;
  for (int s = 0; s < NSUB; s++) {
    char url[160];
    snprintf(url, sizeof url,
      "https://www.reddit.com/r/%s/new.json?limit=25", SUBS[s]);
    cJSON *r = feed_get_json_h(ctx->http, url, hdrs, 10000);
    cJSON *data = r ? cJSON_GetObjectItem(r, "data") : NULL;
    cJSON *children = data ? cJSON_GetObjectItem(data, "children") : NULL;
    if (cJSON_IsArray(children)) {
      int i = 0;
      cJSON *c;
      cJSON_ArrayForEach(c, children) {
        if (i++ >= 25) break;                       /* slice(0,25) */
        cJSON *p = cJSON_GetObjectItem(c, "data");
        if (!p) continue;

        const char *pid = jo_sv(p, "id");
        if (!pid) continue;                          /* intelUid → key */

        const char *ptitle = jo_sv(p, "title");
        char titbuf[64];
        const char *title = ptitle;
        if (!title) {
          snprintf(titbuf, sizeof titbuf, "r/%s post", SUBS[s]);
          title = titbuf;
        }

        /* summary = (p.selftext||'').slice(0,240) || null */
        char *summary = NULL;
        const char *st = jo_sv(p, "selftext");
        if (st) {
          size_t L = strlen(st);
          if (L > 240) L = 240;
          summary = malloc(L + 1);
          memcpy(summary, st, L);
          summary[L] = 0;
          if (!summary[0]) { free(summary); summary = NULL; }
        }

        /* link = p.url || `https://www.reddit.com${p.permalink}` */
        char *link = NULL;
        const char *purl = jo_sv(p, "url");
        if (purl) {
          link = strdup(purl);
        } else {
          const cJSON *pm = cJSON_GetObjectItem(p, "permalink");
          const char *pms = (pm && cJSON_IsString(pm)) ? pm->valuestring : "";
          size_t need = strlen(pms) + 32;
          link = malloc(need);
          snprintf(link, need, "https://www.reddit.com%s", pms);
        }

        /* published_at: created_utc ? new Date(*1000).toISO() : null */
        char isobuf[40];
        const char *pub = NULL;
        const cJSON *cu = cJSON_GetObjectItem(p, "created_utc");
        if (cu && cJSON_IsNumber(cu) && cu->valuedouble) {
          epoch_iso(cu->valuedouble, isobuf, sizeof isobuf);
          pub = isobuf;
        }

        /* properties — EXACT JS key order */
        cJSON *props = cJSON_CreateObject();
        cJSON_AddStringToObject(props, "subreddit", SUBS[s]);
        const cJSON *sc = cJSON_GetObjectItem(p, "score");
        cJSON_AddItemToObject(props, "score",
          sc ? cJSON_Duplicate(sc, 1) : cJSON_CreateNull());
        const cJSON *nc = cJSON_GetObjectItem(p, "num_comments");
        cJSON_AddItemToObject(props, "comments",
          nc ? cJSON_Duplicate(nc, 1) : cJSON_CreateNull());
        const cJSON *au = cJSON_GetObjectItem(p, "author");
        cJSON_AddItemToObject(props, "author",
          au ? cJSON_Duplicate(au, 1) : cJSON_CreateNull());
        char *pj = cJSON_PrintUnformatted(props);

        char tags[96];
        snprintf(tags, sizeof tags, "[\"reddit\",\"%s\",\"social\"]", SUBS[s]);

        intel_item it = {0};
        it.remote_key     = pid;
        it.title          = title;
        it.summary        = summary;
        it.link           = link;
        it.lang           = "en";
        it.published_at   = pub;
        it.record_type    = "reddit-jp-subs";
        it.properties_json = pj;
        it.tags_json      = tags;
        if (sink->emit(sink, &it) >= 0) n++;

        free(pj); free(summary); free(link);
        cJSON_Delete(props);
      }
    }
    if (r) cJSON_Delete(r);
  }
  fprintf(stderr, "[reddit-jp-subs] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def reddit_jp_subs_def = {
  .id = "reddit-jp-subs", .collector = "social",
  .name = "Reddit r/japan + JP subs", .name_ja = "Reddit 日本サブレディット",
   .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(reddit_jp_subs_def)
