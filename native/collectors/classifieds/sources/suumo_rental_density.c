/* collectors/classifieds/sources/suumo_rental_density.c
 * Port of server/src/collectors/suumoRentalDensity.js.
 * For each of 47 prefectures: GET https://suumo.jp/chintai/<slug>/ , parse
 * first /([0-9,]{2,})\s*件/ → rental_listings count, pin at pref centroid.
 * ALWAYS emits one Feature per pref (count null → source 'suumo_unavailable'),
 * so 47 rows regardless of upstream — not a SEED fallback (HARD RULE 8 N/A).
 * Feature props (exact JS order): id, prefecture_ja, prefecture_slug,
 *   rental_listings, source.  id="SUUMO_<slug>" → uid via NATIVE_ID "id". */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct pe { const char *ja, *slug; double lat, lon; };

/* SUUMO_SLUGS iteration order (JS Object insertion order) with the matching
 * PREFECTURE_CENTROID lat/lon. */
static const struct pe PREFS[] = {
  {"北海道","hokkaido",43.0642,141.3469},{"青森県","aomori",40.8243,140.7400},
  {"岩手県","iwate",39.7036,141.1527},{"宮城県","miyagi",38.2688,140.8721},
  {"秋田県","akita",39.7186,140.1024},{"山形県","yamagata",38.2404,140.3634},
  {"福島県","fukushima",37.7503,140.4676},{"茨城県","ibaraki",36.3418,140.4468},
  {"栃木県","tochigi",36.5657,139.8836},{"群馬県","gumma",36.3912,139.0608},
  {"埼玉県","saitama",35.8570,139.6489},{"千葉県","chiba",35.6047,140.1233},
  {"東京都","tokyo",35.6895,139.6917},{"神奈川県","kanagawa",35.4478,139.6425},
  {"新潟県","nigata",37.9161,139.0364},{"富山県","toyama",36.6953,137.2113},
  {"石川県","ishikawa",36.5946,136.6256},{"福井県","fukui",36.0652,136.2216},
  {"山梨県","yamanashi",35.6640,138.5683},{"長野県","nagano",36.6513,138.1811},
  {"岐阜県","gifu",35.3912,136.7223},{"静岡県","shizuoka",34.9770,138.3830},
  {"愛知県","aichi",35.1802,136.9066},{"三重県","mie",34.7303,136.5086},
  {"滋賀県","shiga",35.0045,135.8686},{"京都府","kyoto",35.0212,135.7556},
  {"大阪府","osaka",34.6863,135.5200},{"兵庫県","hyogo",34.6913,135.1830},
  {"奈良県","nara",34.6851,135.8050},{"和歌山県","wakayama",34.2261,135.1675},
  {"鳥取県","tottori",35.5036,134.2378},{"島根県","shimane",35.4723,133.0505},
  {"岡山県","okayama",34.6618,133.9350},{"広島県","hiroshima",34.3963,132.4596},
  {"山口県","yamaguchi",34.1859,131.4707},{"徳島県","tokushima",34.0657,134.5592},
  {"香川県","kagawa",34.3401,134.0434},{"愛媛県","ehime",33.8416,132.7657},
  {"高知県","kochi",33.5597,133.5311},{"福岡県","fukuoka",33.6064,130.4181},
  {"佐賀県","saga",33.2494,130.2988},{"長崎県","nagasaki",32.7503,129.8779},
  {"熊本県","kumamoto",32.7898,130.7417},{"大分県","oita",33.2382,131.6126},
  {"宮崎県","miyazaki",31.9111,131.4239},{"鹿児島県","kagoshima",31.5602,130.5581},
  {"沖縄県","okinawa",26.2124,127.6809},
};
#define NPREF ((int)(sizeof PREFS / sizeof PREFS[0]))

/* JS: html.match(/([0-9,]{2,})\s*件/); n = parseInt(g1.replace(/,/,''),10);
 * Number.isFinite(n)?n:null. The {2,} means the (digit|comma) run is >=2 chars.
 * Returns 1 + sets *out if a match yields a finite integer, else 0. */
static int parse_count(const char *html, long *out) {
  const char *p = html;
  while (*p) {
    if ((*p >= '0' && *p <= '9')) {
      const char *s = p;
      while (*p && ((*p >= '0' && *p <= '9') || *p == ',')) p++;
      int runlen = (int)(p - s);
      const char *u = p;
      while (*u == ' ' || *u == '\t' || *u == '\n' || *u == '\r' || *u == '\f' || *u == '\v') u++;
      /* "件" = E4 BB B6 */
      if (runlen >= 2 && (unsigned char)u[0] == 0xE4
          && (unsigned char)u[1] == 0xBB && (unsigned char)u[2] == 0xB6) {
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
  cJSON *features = cJSON_CreateArray();
  for (int i = 0; i < NPREF; i++) {
    char url[128];
    snprintf(url, sizeof url, "https://suumo.jp/chintai/%s/", PREFS[i].slug);
    char *html = feed_get_text(ctx->http, url, 15000);

    long count = 0; int have = 0;
    if (html) { have = parse_count(html, &count); free(html); }

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(PREFS[i].lon));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(PREFS[i].lat));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *p = cJSON_CreateObject();   /* EXACT JS key order */
    char id[64]; snprintf(id, sizeof id, "SUUMO_%s", PREFS[i].slug);
    cJSON_AddStringToObject(p, "id", id);
    cJSON_AddStringToObject(p, "prefecture_ja", PREFS[i].ja);
    cJSON_AddStringToObject(p, "prefecture_slug", PREFS[i].slug);
    cJSON_AddItemToObject(p, "rental_listings",
      have ? cJSON_CreateNumber((double)count) : cJSON_CreateNull());
    cJSON_AddStringToObject(p, "source",
      have ? "suumo_live" : "suumo_unavailable");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[suumo-rental-density] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def suumo_rental_density_def = {
  .id = "suumo-rental-density", .collector = "classifieds",
  .name = "Suumo Rental Density", .name_ja = "Suumo 賃貸物件数",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(suumo_rental_density_def)
