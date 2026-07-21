/* collectors/marketplace/sources/real_estate.c
 * Port of server/src/collectors/realEstate.js — trySuumoScrape() only.
 * generateSeedData()/_meta NOT ported (HARD RULE 8). When the live scrape
 * yields 0 rows JS falls back to SEED; we emit 0 (correctness-neutral).
 *
 * For slug in [13 slugs], page 1..5:
 *   GET https://suumo.jp/jj/chintai/ichiran/FR301FC001/?ar=030&bs=040&ta=<slug>&page=<p>
 *   items = html.split(/cassetteitem"/).slice(1); break page if 0; per block:
 *     title = /cassetteitem_content-title[^>]*>([^<]+)</
 *     addr  = /cassetteitem_detail-col1[^>]*>([^<]+)</
 *     rent  = /cassetteitem_other-emphasis[^>]*>([^<]+)</
 *     size  = /cassetteitem_madori[^>]*>([^<]+)</
 *   need title && addr; area = findArea(addr) (AREA names first, then PREF
 *   names; substring; iteration = JS Object.keys order). jitter ±0.01 random.
 * Feature props (exact JS order): id, platform, title, address, rent_display,
 *   size, area, prefecture, listing_type, source.  id="SUUMO_<idx>" 1-based
 *   over matched rows → uid via NATIVE_ID "id". */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/htmlparse.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* REAL_ESTATE_AREAS, in array order. AREA_BY_NAME keys = .area in this order
 * (areas are all unique). PREF_BY_NAME: Object.fromEntries → first-seen pref
 * defines Object.keys order, value = LAST array entry with that pref. */
struct ra { const char *area, *pref; double lat, lon; };
static const struct ra AREAS[] = {
  {"港区","東京都",35.6584,139.7516},      {"千代田区","東京都",35.6940,139.7536},
  {"渋谷区","東京都",35.6640,139.6982},     {"新宿区","東京都",35.6938,139.7036},
  {"目黒区","東京都",35.6338,139.6980},     {"世田谷区","東京都",35.6461,139.6531},
  {"中央区","東京都",35.6712,139.7719},     {"品川区","東京都",35.6090,139.7300},
  {"文京区","東京都",35.7179,139.7522},     {"杉並区","東京都",35.6994,139.6365},
  {"豊島区","東京都",35.7263,139.7171},     {"練馬区","東京都",35.7356,139.6518},
  {"大田区","東京都",35.5613,139.7161},     {"足立区","東京都",35.7752,139.8046},
  {"江戸川区","東京都",35.6928,139.8682},   {"横浜市中区","神奈川県",35.4437,139.6380},
  {"川崎市","神奈川県",35.5309,139.7030},   {"浦和区","埼玉県",35.8617,139.6455},
  {"船橋市","千葉県",35.6946,139.9828},     {"大阪市北区","大阪府",34.7055,135.4983},
  {"大阪市中央区","大阪府",34.6813,135.5133},{"大阪市天王寺区","大阪府",34.6532,135.5186},
  {"名古屋市中区","愛知県",35.1692,136.9084},{"福岡市中央区","福岡県",33.5898,130.3987},
  {"札幌市中央区","北海道",43.0618,141.3545},{"京都市中京区","京都府",35.0116,135.7681},
  {"神戸市中央区","兵庫県",34.6913,135.1830},{"広島市中区","広島県",34.3920,132.4580},
  {"仙台市青葉区","宮城県",38.2682,140.8694},{"那覇市","沖縄県",26.3344,127.6809},
};
#define NAREA ((int)(sizeof AREAS / sizeof AREAS[0]))

/* PREF_BY_NAME: distinct prefs in first-seen order; value index = LAST AREAS
 * entry with that pref (Object.fromEntries last-write-wins). */
struct pe { const char *pref; int last_idx; };
static const struct pe PREFS[] = {
  {"東京都",14},   /* last 東京都 = 江戸川区 (idx 14) */
  {"神奈川県",16}, /* last = 川崎市 (idx 16) */
  {"埼玉県",17},   /* 浦和区 */
  {"千葉県",18},   /* 船橋市 */
  {"大阪府",21},   /* last = 大阪市天王寺区 */
  {"愛知県",22},   /* 名古屋市中区 */
  {"福岡県",23},   /* 福岡市中央区 */
  {"北海道",24},   /* 札幌市中央区 */
  {"京都府",25},   /* 京都市中京区 */
  {"兵庫県",26},   /* 神戸市中央区 */
  {"広島県",27},   /* 広島市中区 */
  {"宮城県",28},   /* 仙台市青葉区 */
  {"沖縄県",29},   /* 那覇市 */
};
#define NPREFK ((int)(sizeof PREFS / sizeof PREFS[0]))

static const struct ra *find_area(const char *text) {
  if (!text || !*text) return NULL;
  for (int i = 0; i < NAREA; i++)
    if (strstr(text, AREAS[i].area)) return &AREAS[i];
  for (int i = 0; i < NPREFK; i++)
    if (strstr(text, PREFS[i].pref)) return &AREAS[PREFS[i].last_idx];
  return NULL;
}

/* JS: block.match(/<cls>[^>]*>([^<]+)</) — first occurrence of class token
 * `cls`, then the next '>' , then [^<]+ up to '<'. Trim (JS .trim()).
 * Returns malloc'd trimmed string or NULL. */
static char *match_cls(const char *block, const char *cls) {
  const char *p = strstr(block, cls);
  if (!p) return NULL;
  const char *gt = strchr(p, '>');
  if (!gt) return NULL;
  const char *s = gt + 1;
  if (*s == '<' || !*s) return NULL;          /* [^<]+ requires >=1 char */
  const char *e = s;
  while (*e && *e != '<') e++;
  /* trim */
  while (s < e && (*s==' '||*s=='\t'||*s=='\n'||*s=='\r')) s++;
  const char *te = e;
  while (te > s && (te[-1]==' '||te[-1]=='\t'||te[-1]=='\n'||te[-1]=='\r')) te--;
  if (te <= s) return NULL;                    /* trimmed empty == falsy */
  return strndup(s, (size_t)(te - s));
}

static const char *SLUGS[] = {
  "tokyo","kanagawa","osaka","aichi","hokkaido","fukuoka","hyogo",
  "chiba","saitama","kyoto","hiroshima","miyagi","shizuoka",
};
#define NSLUG ((int)(sizeof SLUGS / sizeof SLUGS[0]))

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *features = cJSON_CreateArray();
  int idx = 0;

  for (int si = 0; si < NSLUG; si++) {
    for (int page = 1; page <= 5; page++) {
      char url[256];
      snprintf(url, sizeof url,
        "https://suumo.jp/jj/chintai/ichiran/FR301FC001/?ar=030&bs=040&ta=%s&page=%d",
        SLUGS[si], page);
      char *html = feed_get_text(ctx->http, url, 12000);
      if (!html) break;                       /* if (!html) break (next slug) */

      /* items = html.split(/cassetteitem"/).slice(1) — iterate occurrences
       * of `cassetteitem"`; block = text up to next occurrence. */
      const char *first = strstr(html, "cassetteitem\"");
      if (!first) { free(html); break; }      /* items.length===0 → break */
      int page_matched = 0;
      const char *bs = first + strlen("cassetteitem\"");
      while (bs) {
        const char *nxt = strstr(bs, "cassetteitem\"");
        size_t blen = nxt ? (size_t)(nxt - bs) : strlen(bs);
        char *block = strndup(bs, blen);
        bs = nxt ? nxt + strlen("cassetteitem\"") : NULL;
        if (!block) continue;

        char *title = match_cls(block, "cassetteitem_content-title");
        char *addr  = match_cls(block, "cassetteitem_detail-col1");
        char *rent  = match_cls(block, "cassetteitem_other-emphasis");
        char *size  = match_cls(block, "cassetteitem_madori");
        if (!title || !addr) {
          free(title); free(addr); free(rent); free(size); free(block);
          continue;
        }
        const struct ra *a = find_area(addr);
        if (!a) { free(title); free(addr); free(rent); free(size); free(block); continue; }

        idx++;

        /* NO-FABRICATION: SUUMO listing pages give no per-listing lat/lon, so
         * the JS random ±0.01 jitter invented precise coordinates. Drop it and
         * place the listing at the REAL area/ward centroid (a true location of
         * the administrative area parsed from the address), flagged approximate
         * via coord_source/location_approximate — no invented precision. */
        cJSON *f = cJSON_CreateObject();
        cJSON_AddStringToObject(f, "type", "Feature");
        cJSON *g = cJSON_CreateObject();
        cJSON_AddStringToObject(g, "type", "Point");
        cJSON *co = cJSON_CreateArray();
        cJSON_AddItemToArray(co, cJSON_CreateNumber(a->lon));
        cJSON_AddItemToArray(co, cJSON_CreateNumber(a->lat));
        cJSON_AddItemToObject(g, "coordinates", co);
        cJSON_AddItemToObject(f, "geometry", g);

        cJSON *p = cJSON_CreateObject();      /* EXACT JS key order */
        char id[32]; snprintf(id, sizeof id, "SUUMO_%d", idx);
        cJSON_AddStringToObject(p, "id", id);
        cJSON_AddStringToObject(p, "platform", "suumo");
        cJSON_AddStringToObject(p, "title", title);
        cJSON_AddStringToObject(p, "address", addr);
        cJSON_AddItemToObject(p, "rent_display",
          rent ? cJSON_CreateString(rent) : cJSON_CreateNull());
        cJSON_AddItemToObject(p, "size",
          size ? cJSON_CreateString(size) : cJSON_CreateNull());
        cJSON_AddStringToObject(p, "area", a->area);
        cJSON_AddStringToObject(p, "prefecture", a->pref);
        cJSON_AddStringToObject(p, "listing_type", "rent");
        cJSON_AddStringToObject(p, "coord_source", "area_centroid");
        cJSON_AddBoolToObject(p, "location_approximate", 1);
        cJSON_AddStringToObject(p, "source", "suumo_live");
        cJSON_AddItemToObject(f, "properties", p);
        cJSON_AddItemToArray(features, f);
        page_matched++;

        free(title); free(addr); free(rent); free(size); free(block);
      }
      free(html);
      if (page_matched == 0) break;           /* if (pageMatched===0) break */
    }
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[real-estate] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def real_estate_def = {
  .id = "real-estate", .collector = "marketplace",
  .name = "Suumo / Homes / AtHome", .name_ja = "スーモ・ホームズ・アットホーム",
   .update_interval_sec = 3600, .run = run,
};
REGISTER_SOURCE(real_estate_def)
