/* collectors/economy/sources/suumo.c
 * Port of server/src/collectors/suumo.js — best-effort scrape of Suumo
 * prefecture rental landing pages (https://suumo.jp/chintai/<slug>/), read
 * the first /([0-9,]{2,})\s*件/ total as a rental-market-size proxy. A pref
 * with no parseable count is OMITTED (honest, possibly empty). Never
 * fabricated counts. Non-spatial: emit intel_item (has_geo=0).
 * uid = suumo|<slug> (mirrors intelUid). */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct pe { const char *ja, *slug; };

/* SUUMO_SLUGS — JS Object insertion order. */
static const struct pe PREFS[] = {
  {"北海道","hokkaido"},{"青森県","aomori"},{"岩手県","iwate"},{"宮城県","miyagi"},
  {"秋田県","akita"},{"山形県","yamagata"},{"福島県","fukushima"},
  {"茨城県","ibaraki"},{"栃木県","tochigi"},{"群馬県","gumma"},{"埼玉県","saitama"},
  {"千葉県","chiba"},{"東京都","tokyo"},{"神奈川県","kanagawa"},
  {"新潟県","nigata"},{"富山県","toyama"},{"石川県","ishikawa"},{"福井県","fukui"},
  {"山梨県","yamanashi"},{"長野県","nagano"},{"岐阜県","gifu"},{"静岡県","shizuoka"},
  {"愛知県","aichi"},{"三重県","mie"},
  {"滋賀県","shiga"},{"京都府","kyoto"},{"大阪府","osaka"},{"兵庫県","hyogo"},
  {"奈良県","nara"},{"和歌山県","wakayama"},
  {"鳥取県","tottori"},{"島根県","shimane"},{"岡山県","okayama"},{"広島県","hiroshima"},{"山口県","yamaguchi"},
  {"徳島県","tokushima"},{"香川県","kagawa"},{"愛媛県","ehime"},{"高知県","kochi"},
  {"福岡県","fukuoka"},{"佐賀県","saga"},{"長崎県","nagasaki"},{"熊本県","kumamoto"},
  {"大分県","oita"},{"宮崎県","miyazaki"},{"鹿児島県","kagoshima"},{"沖縄県","okinawa"},
};
#define NPREF ((int)(sizeof PREFS / sizeof PREFS[0]))

/* JS: html.match(/([0-9,]{2,})\s*件/); n=parseInt(g1.replace(/,/,''),10);
 * Number.isFinite(n)?n:null. {2,} → (digit|comma) run >= 2 chars. */
static int parse_count(const char *html, long *out) {
  const char *p = html;
  while (*p) {
    if (*p >= '0' && *p <= '9') {
      const char *s = p;
      while (*p && ((*p >= '0' && *p <= '9') || *p == ',')) p++;
      int runlen = (int)(p - s);
      const char *u = p;
      while (*u==' '||*u=='\t'||*u=='\n'||*u=='\r'||*u=='\f'||*u=='\v') u++;
      /* "件" = E4 BB B6 */
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

  int n = 0;
  for (int i = 0; i < NPREF; i++) {
    char url[128];
    snprintf(url, sizeof url, "https://suumo.jp/chintai/%s/", PREFS[i].slug);
    char *html = feed_get_text(ctx->http, url, 15000);
    long count = 0; int have = 0;
    if (html) { have = parse_count(html, &count); free(html); }
    if (!have) continue;                 /* omit pref — honest empty */

    char title[128];
    snprintf(title, sizeof title, "Suumo rental listings — %s", PREFS[i].ja);
    char summary[64];
    snprintf(summary, sizeof summary, "%ld active rental listings", count);
    char body[160];
    snprintf(body, sizeof body,
      "Suumo prefecture rental landing page (%s) reported %ld listings.",
      PREFS[i].slug, count);

    cJSON *p = cJSON_CreateObject();   /* EXACT JS key order */
    cJSON_AddStringToObject(p, "prefecture_ja", PREFS[i].ja);
    cJSON_AddStringToObject(p, "prefecture_slug", PREFS[i].slug);
    cJSON_AddNumberToObject(p, "rental_listings", (double)count);
    char *pj = cJSON_PrintUnformatted(p);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("economy"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("real-estate"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("rental"));
    char *tj = cJSON_PrintUnformatted(tags);

    intel_item it = {0};
    it.remote_key   = PREFS[i].slug;           /* → uid suumo|<slug> */
    it.title        = title;
    it.summary      = summary;
    it.body         = body;
    it.link         = url;
    it.lang         = "ja";
    it.published_at = now;
    it.record_type  = "suumo";
    it.tags_json    = tj;
    it.properties_json = pj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags);
  }

  fprintf(stderr, "[suumo] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def suumo_def = {
  .id = "suumo", .collector = "economy",
  .name = "Suumo Rental Prices", .name_ja = "SUUMO 賃料情報",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(suumo_def)
