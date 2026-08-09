/* collectors/economy/sources/lifull_homes.c
 * Port of server/src/collectors/lifullHomes.js — best-effort scrape of LIFULL
 * HOME'S used-apartment for-sale section
 * (https://www.homes.co.jp/mansion/chuko/<slug>/), read first
 * /([0-9,]{2,})\s*件/ total as a for-sale-market-size proxy. Pref with no
 * count is OMITTED (honest, possibly empty). Never fabricated. Non-spatial.
 * uid = lifull-homes|<slug> (mirrors intelUid). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct pe { const char *ja, *slug; };

/* HOMES_SLUGS — JS Object insertion order (reduced for-sale set). */
static const struct pe PREFS[] = {
  {"北海道","hokkaido"},{"宮城県","miyagi"},{"埼玉県","saitama"},{"千葉県","chiba"},
  {"東京都","tokyo"},{"神奈川県","kanagawa"},{"愛知県","aichi"},{"京都府","kyoto"},
  {"大阪府","osaka"},{"兵庫県","hyogo"},{"広島県","hiroshima"},{"福岡県","fukuoka"},
  {"新潟県","niigata"},{"静岡県","shizuoka"},{"岡山県","okayama"},{"熊本県","kumamoto"},
  {"沖縄県","okinawa"},
};
#define NPREF ((int)(sizeof PREFS / sizeof PREFS[0]))

static int parse_count(const char *html, long *out) {
  const char *p = html;
  while (*p) {
    if (*p >= '0' && *p <= '9') {
      const char *s = p;
      while (*p && ((*p >= '0' && *p <= '9') || *p == ',')) p++;
      int runlen = (int)(p - s);
      const char *u = p;
      while (*u==' '||*u=='\t'||*u=='\n'||*u=='\r'||*u=='\f'||*u=='\v') u++;
      if (runlen >= 2 && (unsigned char)u[0]==0xE4
          && (unsigned char)u[1]==0xBB && (unsigned char)u[2]==0xB6) {
        long v = 0; int any = 0;
        for (const char *d = s; d < p; d++) {
          if (*d == ',') continue;
          v = v * 10 + (*d - '0'); any = 1;
        }
        if (any) { *out = v; return 1; }
      }
    } else {
      p++;
    }
  }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char now[32];
  { time_t t = time(NULL); struct tm tm; gmtime_r(&t, &tm);
    strftime(now, sizeof now, "%Y-%m-%dT%H:%M:%S.000Z", &tm); }

  int n = 0, fetched = 0;
  for (int i = 0; i < NPREF; i++) {
    char url[160];
    snprintf(url, sizeof url,
      "https://www.homes.co.jp/mansion/chuko/%s/", PREFS[i].slug);
    char *html = feed_get_text(ctx->http, url, 15000);
    long count = 0; int have = 0;
    if (html) {
      if (*html) fetched++;              /* body actually came back */
      have = parse_count(html, &count);
      free(html);
    }
    if (!have) continue;                 /* omit pref — honest empty */

    char title[192];
    snprintf(title, sizeof title,
      "LIFULL HOME'S used-apartment listings — %s", PREFS[i].ja);
    char summary[64];
    snprintf(summary, sizeof summary, "%ld for-sale used apartments", count);
    char body[224];
    snprintf(body, sizeof body,
      "LIFULL HOME'S used-apartment (中古マンション) section for %s reported %ld listings.",
      PREFS[i].ja, count);

    cJSON *p = cJSON_CreateObject();   /* EXACT JS key order */
    cJSON_AddStringToObject(p, "prefecture_ja", PREFS[i].ja);
    cJSON_AddStringToObject(p, "prefecture_slug", PREFS[i].slug);
    cJSON_AddNumberToObject(p, "forsale_listings", (double)count);
    cJSON_AddStringToObject(p, "segment", "used_apartment");
    char *pj = cJSON_PrintUnformatted(p);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("economy"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("real-estate"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("for-sale"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("landprice"));
    char *tj = cJSON_PrintUnformatted(tags);

    intel_item it = {0};
    it.remote_key   = PREFS[i].slug;          /* → uid lifull-homes|<slug> */
    it.title        = title;
    it.summary      = summary;
    it.body         = body;
    it.link         = url;
    it.lang         = "ja";
    it.published_at = now;
    it.record_type  = "lifull-homes";
    it.tags_json    = tj;
    it.properties_json = pj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags);
  }

  fprintf(stderr, "[lifull-homes] emitted %d (pages fetched %d/%d)\n",
          n, fetched, NPREF);
  /* `n > 0 ? 0 : -1` conflated "no listings parsed" with "the site refused
   * us". Only a total fetch failure is an error; a page that came back but
   * carried no NN件 total is an honest empty and must return 0, or the
   * scheduler's anomaly detector quarantines the source. */
  if (fetched == 0) {
    fprintf(stderr, "[lifull-homes] no page body returned for any prefecture\n");
    return -1;
  }
  return 0;
}

static const source_def lifull_homes_def = {
  .id = "lifull-homes", .collector = "economy",
  .name = "LIFULL HOME'S", .name_ja = "LIFULL HOME'S 物件",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(lifull_homes_def)
