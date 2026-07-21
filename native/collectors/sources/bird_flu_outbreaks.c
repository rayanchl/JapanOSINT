/* collectors/agriculture/sources/bird_flu_outbreaks.c
 * Port of server/src/collectors/birdFluOutbreaks.js — MAFF domestic HPAI
 * poultry-outbreak page. fetchText → strip tags → scan "N例目 … <県>"
 * case patterns, group distinct case numbers per prefecture, geocode each
 * to its prefecture centroid (inlined _jpPrefectures table). Fallback:
 * prefecture mention near 発生/確認/防疫措置. Honest empty on fetch/parse
 * failure or no rows (rule 8). Property key order
 * (outbreak_id, prefecture, prefecture_en, case_count, disease, link,
 * source) mirrors the JS mapFn for featureUid hash parity.
 * (No native wildlife/ dir — filed under agriculture per task rule.) */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/htmlparse.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAFF_URL "https://www.maff.go.jp/j/syouan/douei/tori/index.html"

struct pref { const char *code, *en, *ja; double lat, lon; };
static const struct pref PREFS[] = {
  {"01","Hokkaido","北海道",43.0628,141.3478},{"02","Aomori","青森県",40.8244,140.7400},
  {"03","Iwate","岩手県",39.7036,141.1525},{"04","Miyagi","宮城県",38.2683,140.8719},
  {"05","Akita","秋田県",39.7186,140.1024},{"06","Yamagata","山形県",38.2403,140.3633},
  {"07","Fukushima","福島県",37.7503,140.4675},{"08","Ibaraki","茨城県",36.3658,140.4711},
  {"09","Tochigi","栃木県",36.5658,139.8836},{"10","Gunma","群馬県",36.3911,139.0608},
  {"11","Saitama","埼玉県",35.8617,139.6455},{"12","Chiba","千葉県",35.6083,140.1233},
  {"13","Tokyo","東京都",35.6895,139.6917},{"14","Kanagawa","神奈川県",35.4437,139.6380},
  {"15","Niigata","新潟県",37.9161,139.0364},{"16","Toyama","富山県",36.6953,137.2113},
  {"17","Ishikawa","石川県",36.5946,136.6256},{"18","Fukui","福井県",36.0652,136.2216},
  {"19","Yamanashi","山梨県",35.6642,138.5683},{"20","Nagano","長野県",36.6489,138.1944},
  {"21","Gifu","岐阜県",35.4233,136.7606},{"22","Shizuoka","静岡県",34.9756,138.3828},
  {"23","Aichi","愛知県",35.1814,136.9069},{"24","Mie","三重県",34.7184,136.5067},
  {"25","Shiga","滋賀県",35.0044,135.8686},{"26","Kyoto","京都府",35.0116,135.7681},
  {"27","Osaka","大阪府",34.6864,135.5197},{"28","Hyogo","兵庫県",34.6913,135.1830},
  {"29","Nara","奈良県",34.6850,135.8048},{"30","Wakayama","和歌山県",34.2261,135.1675},
  {"31","Tottori","鳥取県",35.5039,134.2378},{"32","Shimane","島根県",35.4722,133.0506},
  {"33","Okayama","岡山県",34.6628,133.9197},{"34","Hiroshima","広島県",34.3853,132.4553},
  {"35","Yamaguchi","山口県",34.1856,131.4714},{"36","Tokushima","徳島県",34.0658,134.5594},
  {"37","Kagawa","香川県",34.3401,134.0434},{"38","Ehime","愛媛県",33.8392,132.7656},
  {"39","Kochi","高知県",33.5594,133.5311},{"40","Fukuoka","福岡県",33.5904,130.4017},
  {"41","Saga","佐賀県",33.2494,130.2989},{"42","Nagasaki","長崎県",32.7503,129.8775},
  {"43","Kumamoto","熊本県",32.8019,130.7256},{"44","Oita","大分県",33.2381,131.6126},
  {"45","Miyazaki","宮崎県",31.9111,131.4239},{"46","Kagoshima","鹿児島県",31.5963,130.5571},
  {"47","Okinawa","沖縄県",26.2125,127.6809},
};
#define NPREF ((int)(sizeof PREFS / sizeof PREFS[0]))

#define MAX_CASES 256
struct acc { int used; int unknown; int ncase; char cases[MAX_CASES][16]; };

static int pref_index_at(const char *p) {
  for (int i = 0; i < NPREF; i++) {
    size_t L = strlen(PREFS[i].ja);
    if (strncmp(p, PREFS[i].ja, L) == 0) return i;
  }
  return -1;
}

static void add_case(struct acc *a, const char *cn) {
  for (int i = 0; i < a->ncase; i++) if (strcmp(a->cases[i], cn) == 0) return;
  if (a->ncase < MAX_CASES) { snprintf(a->cases[a->ncase], 16, "%s", cn); a->ncase++; }
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *html = feed_get_text(ctx->http, MAFF_URL, 15000);
  if (!html || strlen(html) < 50) { free(html); fprintf(stderr, "[bird-flu-outbreaks] unavailable\n"); return -1; }

  char *text = html_strip(html);   /* strip tags, collapse whitespace */
  free(html);
  if (!text) { fprintf(stderr, "[bird-flu-outbreaks] unavailable\n"); return -1; }

  static struct acc accs[NPREF];
  memset(accs, 0, sizeof accs);
  int any = 0;

  /* Primary: /(\d+)\s*例目[^。]*?(<県>)/  — "N例目" then nearest pref
   * before the next "。". 例目 = E4BE8B E79BAE ; 。 = E38082. */
  const char *KEY = "例目";
  for (const char *q = text; (q = strstr(q, KEY)) != NULL; q++) {
    /* read digits immediately before (allow trailing spaces) */
    const char *d = q;
    while (d > text && d[-1] == ' ') d--;
    const char *de = d;
    while (d > text && d[-1] >= '0' && d[-1] <= '9') d--;
    if (d == de) continue;
    char caseno[16]; size_t cl = (size_t)(de - d);
    if (cl >= sizeof caseno) cl = sizeof caseno - 1;
    memcpy(caseno, d, cl); caseno[cl] = 0;

    /* search forward until next "。" for a prefecture name */
    const char *scan = q + strlen(KEY);
    const char *stop = strstr(scan, "。");
    for (const char *s = scan; *s && (!stop || s < stop); s++) {
      int pi = pref_index_at(s);
      if (pi >= 0) {
        accs[pi].used = 1;
        add_case(&accs[pi], caseno);
        any = 1;
        break;
      }
    }
  }

  /* Fallback: if nothing matched, pref name then 発生|確認|防疫措置 within
   * ~40 chars (Node `${pn}[^。]{0,40}(発生|確認|防疫措置)`). */
  if (!any) {
    for (int i = 0; i < NPREF; i++) {
      const char *hit = strstr(text, PREFS[i].ja);
      while (hit) {
        const char *win_end = hit + strlen(PREFS[i].ja);
        /* ~40 JP chars ≈ 120 bytes; stop at 。 */
        const char *stop = strstr(win_end, "。");
        size_t span = 140;
        char buf[160];
        size_t avail = strlen(win_end);
        if (stop && (size_t)(stop - win_end) < span) span = (size_t)(stop - win_end);
        if (span > avail) span = avail;
        if (span > sizeof buf - 1) span = sizeof buf - 1;
        memcpy(buf, win_end, span); buf[span] = 0;
        if (strstr(buf, "発生") || strstr(buf, "確認") || strstr(buf, "防疫措置")) {
          accs[i].used = 1; accs[i].unknown = 1;
          break;
        }
        hit = strstr(win_end, PREFS[i].ja);
      }
    }
  }

  cJSON *features = cJSON_CreateArray();
  for (int i = 0; i < NPREF; i++) {
    if (!accs[i].used) continue;
    int unknown = accs[i].unknown || accs[i].ncase == 0;
    int caseCount = unknown ? -1 : accs[i].ncase;

    char hk[24]; char ccbuf[16];
    if (unknown) ccbuf[0] = 0;
    else snprintf(ccbuf, sizeof ccbuf, "%d", caseCount);
    const char *parts[] = { PREFS[i].ja, unknown ? NULL : ccbuf };
    feed_hash_key(hk, parts, 2);
    char oid[64];
    snprintf(oid, sizeof oid, "HPAI_%s_%s", PREFS[i].code, hk);

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *c = cJSON_CreateArray();
    cJSON_AddItemToArray(c, cJSON_CreateNumber(PREFS[i].lon));
    cJSON_AddItemToArray(c, cJSON_CreateNumber(PREFS[i].lat));
    cJSON_AddItemToObject(g, "coordinates", c);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
    cJSON_AddStringToObject(p, "outbreak_id", oid);
    cJSON_AddStringToObject(p, "prefecture", PREFS[i].ja);
    cJSON_AddStringToObject(p, "prefecture_en", PREFS[i].en);
    if (unknown) cJSON_AddNullToObject(p, "case_count");
    else         cJSON_AddNumberToObject(p, "case_count", caseCount);
    cJSON_AddStringToObject(p, "disease", "高病原性鳥インフルエンザ (HPAI)");
    cJSON_AddStringToObject(p, "link", MAFF_URL);
    cJSON_AddStringToObject(p, "source", "maff_hpai");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  free(text);

  if (cJSON_GetArraySize(features) == 0) {
    cJSON_Delete(features);
    fprintf(stderr, "[bird-flu-outbreaks] unavailable (no rows)\n");
    return -1;
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[bird-flu-outbreaks] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def bird_flu_outbreaks_def = {
  .id = "bird-flu-outbreaks", .collector = "agriculture",
  .name = "Bird Flu Outbreaks", .name_ja = "高病原性鳥インフルエンザ",
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(bird_flu_outbreaks_def)
