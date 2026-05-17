/* collectors/marketplace/sources/classifieds.c
 * Port of server/src/collectors/classifieds.js (fetchText + regex HTML scrape).
 * Jmty (ジモティー) nationwide: 12 prefecture slugs x up to 5 pages of
 * https://jmty.jp/{slug}/sale?page={N}. Split body on "p-articles-list-item",
 * extract title / price / area / href per block, geocode by JAPAN_AREAS.
 * generateSeedData() + _meta NOT ported (live-only; 0 rows when upstream down).
 * Feature has no `id` member → featureUid = sha1(JSON.stringify{g,p}); the JS
 * applies random lat/lon jitter so upstream uids are non-deterministic too. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { const char *area, *pref; double lat, lon; } jp_area;

/* JAPAN_AREAS — verbatim, same order as the JS array. */
static const jp_area AREAS[] = {
  { "世田谷区", "東京都", 35.6461, 139.6531 },
  { "練馬区",   "東京都", 35.7356, 139.6518 },
  { "大田区",   "東京都", 35.5613, 139.7161 },
  { "足立区",   "東京都", 35.7752, 139.8046 },
  { "江戸川区", "東京都", 35.6928, 139.8682 },
  { "杉並区",   "東京都", 35.6994, 139.6365 },
  { "板橋区",   "東京都", 35.7512, 139.7090 },
  { "新宿区",   "東京都", 35.6938, 139.7036 },
  { "渋谷区",   "東京都", 35.6640, 139.6982 },
  { "中野区",   "東京都", 35.7073, 139.6638 },
  { "豊島区",   "東京都", 35.7263, 139.7171 },
  { "目黒区",   "東京都", 35.6338, 139.6980 },
  { "横浜市",   "神奈川県", 35.4437, 139.6380 },
  { "川崎市",   "神奈川県", 35.5309, 139.7030 },
  { "相模原市", "神奈川県", 35.5710, 139.3731 },
  { "大阪市北区",   "大阪府", 34.7055, 135.4983 },
  { "大阪市中央区", "大阪府", 34.6813, 135.5133 },
  { "大阪市天王寺区","大阪府", 34.6532, 135.5186 },
  { "堺市",     "大阪府", 34.5733, 135.4832 },
  { "名古屋市", "愛知県", 35.1815, 136.9066 },
  { "札幌市",   "北海道", 43.0618, 141.3545 },
  { "福岡市",   "福岡県", 33.5902, 130.4017 },
  { "神戸市",   "兵庫県", 34.6913, 135.1830 },
  { "京都市",   "京都府", 35.0116, 135.7681 },
  { "広島市",   "広島県", 34.3853, 132.4553 },
  { "仙台市",   "宮城県", 38.2682, 140.8694 },
  { "千葉市",   "千葉県", 35.6073, 140.1063 },
  { "北九州市", "福岡県", 33.8834, 130.8752 },
  { "新潟市",   "新潟県", 37.9161, 139.0364 },
  { "浜松市",   "静岡県", 34.7108, 137.7261 },
  { "熊本市",   "熊本県", 32.8032, 130.7079 },
  { "岡山市",   "岡山県", 34.6551, 133.9195 },
  { "那覇市",   "沖縄県", 26.3344, 127.6809 },
};
#define NAREA ((int)(sizeof(AREAS) / sizeof(AREAS[0])))

/* JS findArea(text): first area whose .area substring matches, else first
 * whose .pref substring matches, else null. */
static const jp_area *find_area(const char *text) {
  if (!text || !text[0]) return NULL;
  for (int i = 0; i < NAREA; i++)
    if (strstr(text, AREAS[i].area)) return &AREAS[i];
  for (int i = 0; i < NAREA; i++)
    if (strstr(text, AREAS[i].pref)) return &AREAS[i];
  return NULL;
}

/* JS AREA_BY_PREF = Object.fromEntries(map([pref,area])): on duplicate pref
 * the LAST array entry wins. */
static const jp_area *area_by_pref(const char *text) {
  if (!text || !text[0]) return NULL;
  const jp_area *hit = NULL;
  for (int i = 0; i < NAREA; i++)
    if (strcmp(AREAS[i].pref, text) == 0) hit = &AREAS[i];
  return hit;
}

/* Mirror block.match(/<needle>[^>]*>([^<]+)</): find needle, then the next
 * '>' (skipping a [^>]* run — no '>' inside), capture up to next '<', trim.
 * Returns malloc'd captured text or NULL. */
static char *cap_after(const char *block, const char *needle) {
  const char *h = strstr(block, needle);
  if (!h) return NULL;
  const char *p = h + strlen(needle);
  /* [^>]* then '>' */
  while (*p && *p != '>') p++;
  if (*p != '>') return NULL;
  p++;
  /* ([^<]+) */
  const char *s = p;
  while (*p && *p != '<') p++;
  if (p == s) return NULL;
  size_t len = (size_t)(p - s);
  /* trim() */
  const char *a = s;
  const char *b = s + len;
  while (a < b && (unsigned char)*a <= ' ') a++;
  while (b > a && (unsigned char)b[-1] <= ' ') b--;
  if (b == a) return NULL;
  size_t n = (size_t)(b - a);
  char *out = malloc(n + 1);
  memcpy(out, a, n);
  out[n] = 0;
  return out;
}

/* Mirror block.match(/href="([^"]+)"/): first href value in the block. */
static char *cap_href(const char *block) {
  const char *h = strstr(block, "href=\"");
  if (!h) return NULL;
  const char *s = h + 6;
  const char *p = strchr(s, '"');
  if (!p || p == s) return NULL;
  size_t n = (size_t)(p - s);
  char *out = malloc(n + 1);
  memcpy(out, s, n);
  out[n] = 0;
  return out;
}

/* new URL(href, 'https://jmty.jp').href — resolve relative refs. Enough of
 * URL resolution for the shapes Jmty emits (absolute, scheme-relative,
 * root-relative, or path-relative). */
static char *resolve_url(const char *href) {
  if (!href || !href[0]) return NULL;
  if (strncmp(href, "http://", 7) == 0 || strncmp(href, "https://", 8) == 0)
    return strdup(href);
  char *out;
  if (strncmp(href, "//", 2) == 0) {
    size_t n = strlen(href) + 8;
    out = malloc(n);
    snprintf(out, n, "https:%s", href);
  } else if (href[0] == '/') {
    size_t n = strlen(href) + 24;
    out = malloc(n);
    snprintf(out, n, "https://jmty.jp%s", href);
  } else {
    size_t n = strlen(href) + 24;
    out = malloc(n);
    snprintf(out, n, "https://jmty.jp/%s", href);
  }
  return out;
}

static const char *SLUGS[] = {
  "tokyo", "kanagawa", "osaka", "aichi", "hokkaido", "fukuoka",
  "hyogo", "chiba", "saitama", "kyoto", "hiroshima", "miyagi"
};
#define NSLUG ((int)(sizeof(SLUGS) / sizeof(SLUGS[0])))

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *features = cJSON_CreateArray();
  int idx = 0;

  for (int s = 0; s < NSLUG; s++) {
    const char *slug = SLUGS[s];
    for (int page = 1; page <= 5; page++) {
      char url[160];
      snprintf(url, sizeof url,
               "https://jmty.jp/%s/sale?page=%d", slug, page);
      char *html = feed_get_text(ctx->http, url, 12000);
      if (!html) break;                          /* if(!html) break */

      /* html.split(/p-articles-list-item/).slice(1) — iterate blocks after
       * each occurrence; a block ends at the next occurrence (or EOF). */
      const char *MARK = "p-articles-list-item";
      size_t ml = strlen(MARK);
      const char *first = strstr(html, MARK);
      if (!first) { free(html); break; }         /* items.length===0 → break */

      int page_matched = 0;
      const char *cur = first + ml;
      while (cur) {
        const char *next = strstr(cur, MARK);
        size_t blen = next ? (size_t)(next - cur) : strlen(cur);
        char *block = malloc(blen + 1);
        memcpy(block, cur, blen);
        block[blen] = 0;

        char *title  = cap_after(block, "p-item-title");
        char *price  = cap_after(block, "p-item-most-important");
        char *areatx = cap_after(block, "p-item-area");
        if (!areatx) areatx = cap_after(block, "データから");
        char *href   = cap_href(block);

        if (title && title[0]) {
          const char *areatxt = areatx ? areatx : "";
          const jp_area *area = find_area(areatxt);
          if (!area) area = find_area(slug);
          if (!area) area = area_by_pref(areatxt);
          if (area) {
            double jLat = ((double)rand() / RAND_MAX - 0.5) * 0.03;
            double jLon = ((double)rand() / RAND_MAX - 0.5) * 0.04;
            idx++;

            cJSON *f = cJSON_CreateObject();
            cJSON_AddStringToObject(f, "type", "Feature");
            cJSON *g = cJSON_CreateObject();
            cJSON_AddStringToObject(g, "type", "Point");
            cJSON *co = cJSON_CreateArray();
            cJSON_AddItemToArray(co, cJSON_CreateNumber(area->lon + jLon));
            cJSON_AddItemToArray(co, cJSON_CreateNumber(area->lat + jLat));
            cJSON_AddItemToObject(g, "coordinates", co);
            cJSON_AddItemToObject(f, "geometry", g);

            cJSON *p = cJSON_CreateObject();   /* EXACT JS key order */
            char id[32];
            snprintf(id, sizeof id, "JMTY_%d", idx);
            cJSON_AddStringToObject(p, "id", id);
            cJSON_AddStringToObject(p, "platform", "jmty");
            cJSON_AddStringToObject(p, "title", title);
            if (price && price[0])
              cJSON_AddStringToObject(p, "price_display", price);
            else
              cJSON_AddItemToObject(p, "price_display", cJSON_CreateNull());
            cJSON_AddStringToObject(p, "area", area->area);
            cJSON_AddStringToObject(p, "prefecture", area->pref);
            char *rurl = (href && href[0]) ? resolve_url(href) : NULL;
            if (rurl)
              cJSON_AddStringToObject(p, "url", rurl);
            else
              cJSON_AddItemToObject(p, "url", cJSON_CreateNull());
            free(rurl);
            cJSON_AddStringToObject(p, "source", "jmty_live");
            cJSON_AddItemToObject(f, "properties", p);
            cJSON_AddItemToArray(features, f);
            page_matched++;
          }
        }

        free(title); free(price); free(areatx); free(href);
        free(block);
        cur = next ? next + ml : NULL;
      }
      free(html);
      if (page_matched == 0) break;              /* if(pageMatched===0) break */
    }
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[classifieds] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def classifieds_def = {
  .id = "classifieds", .collector = "marketplace",
  .name = "Jmty / Mercari / Yahoo Auctions",
  .name_ja = "ジモティー・メルカリ・ヤフオク",
   .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(classifieds_def)
