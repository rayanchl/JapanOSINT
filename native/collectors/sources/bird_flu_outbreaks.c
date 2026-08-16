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
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
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

/* Prefecture whose JA name starts at p (longest-name-first is unnecessary:
 * the 47 names are mutually non-prefixed). */
static int pref_index_at(const char *p) {
  for (int i = 0; i < NPREF; i++) {
    size_t L = strlen(PREFS[i].ja);
    if (strncmp(p, PREFS[i].ja, L) == 0) return i;
  }
  return -1;
}

/* Scan BACKWARD from `to` (exclusive) over at most `span` bytes for a
 * prefecture name; returns its index and sets *at to where the name starts.
 * The MAFF case lines read "<pref><municipality>（国内N例目）", i.e. the
 * prefecture PRECEDES the case number. */
static int pref_before(const char *base, const char *to, size_t span,
                       const char **at) {
  const char *lo = (size_t)(to - base) > span ? to - span : base;
  for (const char *s = to; s >= lo; s--) {
    int pi = pref_index_at(s);
    if (pi >= 0) { *at = s; return pi; }
  }
  return -1;
}

/* Newest 令和 season page linked from the MAFF index.
 * The index links "<something>_hpai_kokunai.html" per season (r2, r3, r5, r6,
 * r7 …). We follow the highest r-number, which is the current season — the
 * index page ITSELF stopped carrying case entries, which is why this collector
 * used to return zero rows. Returns 1 and fills url[] on success. */
static int newest_season_url(const char *html, char *url, size_t cap) {
  const char *NEEDLE = "_hpai_kokunai.html";
  int best = -1; char bestpath[256] = {0};
  for (const char *p = html; (p = strstr(p, NEEDLE)) != NULL; p++) {
    /* walk back to the opening quote of the href value */
    const char *s = p;
    while (s > html && s[-1] != '"' && s[-1] != '\'') s--;
    size_t len = (size_t)(p + strlen(NEEDLE) - s);
    if (len == 0 || len >= sizeof bestpath) continue;
    /* the file component must be r<N>_hpai_kokunai.html */
    const char *f = p;
    while (f > s && f[-1] != '/') f--;
    if (f[0] != 'r') continue;
    int n = 0; const char *d = f + 1;
    if (*d < '0' || *d > '9') continue;
    while (*d >= '0' && *d <= '9') { n = n * 10 + (*d - '0'); d++; }
    if (d != p) continue;                       /* digits must run up to "_" */
    if (n > best) { best = n; memcpy(bestpath, s, len); bestpath[len] = 0; }
  }
  if (best < 0) return 0;
  if (!strncmp(bestpath, "http", 4))
    snprintf(url, cap, "%s", bestpath);
  else if (bestpath[0] == '/')
    snprintf(url, cap, "https://www.maff.go.jp%s", bestpath);
  else
    snprintf(url, cap, "https://www.maff.go.jp/j/syouan/douei/tori/%s",
             bestpath[0] == '.' && bestpath[1] == '/' ? bestpath + 2 : bestpath);
  return 1;
}

/* "令和8年4月22日" → "2026-04-22". Reiwa 1 == 2019. 0 if not present. */
static int reiwa_date(const char *s, const char *end, char *out, size_t cap) {
  const char *p = strstr(s, "令和");
  if (!p || p >= end) return 0;
  p += strlen("令和");
  int y = 0, m = 0, d = 0;
  while (*p >= '0' && *p <= '9') y = y * 10 + (*p++ - '0');
  if (!y || strncmp(p, "年", strlen("年"))) return 0;
  p += strlen("年");
  while (*p >= '0' && *p <= '9') m = m * 10 + (*p++ - '0');
  if (!m || strncmp(p, "月", strlen("月"))) return 0;
  p += strlen("月");
  while (*p >= '0' && *p <= '9') d = d * 10 + (*p++ - '0');
  if (!d || strncmp(p, "日", strlen("日"))) return 0;
  if (m > 12 || d > 31) return 0;
  snprintf(out, cap, "%04d-%02d-%02d", y + 2018, m, d);
  return 1;
}

#define MAX_CASES 512

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *idx = feed_get_text(ctx->http, MAFF_URL, 15000);
  if (!idx || strlen(idx) < 50) {
    free(idx); fprintf(stderr, "[bird-flu-outbreaks] index unavailable\n");
    return -1;
  }
  char season_url[512];
  int have = newest_season_url(idx, season_url, sizeof season_url);
  free(idx);
  if (!have) {
    fprintf(stderr, "[bird-flu-outbreaks] no season page linked from index\n");
    return 0;                       /* honest empty, not an error */
  }

  char *html = feed_get_text(ctx->http, season_url, 15000);
  if (!html || strlen(html) < 50) {
    free(html);
    fprintf(stderr, "[bird-flu-outbreaks] season page unavailable: %s\n", season_url);
    return -1;
  }
  char *text = html_strip(html);   /* strip tags, collapse whitespace */
  free(html);
  if (!text) { fprintf(stderr, "[bird-flu-outbreaks] strip failed\n"); return -1; }

  cJSON *features = cJSON_CreateArray();
  int seen[MAX_CASES]; int nseen = 0;

  /* One Feature per outbreak case: "<pref><municipality>（国内N例目）…". */
  const char *KEY = "例目";
  for (const char *q = text; (q = strstr(q, KEY)) != NULL; q++) {
    const char *de = q;
    const char *d = q;
    while (d > text && d[-1] >= '0' && d[-1] <= '9') d--;
    if (d == de) continue;
    int caseno = atoi(d);
    if (caseno <= 0) continue;

    int dup = 0;
    for (int i = 0; i < nseen; i++) if (seen[i] == caseno) { dup = 1; break; }
    if (dup) continue;

    const char *pat = NULL;
    int pi = pref_before(text, d, 120, &pat);
    if (pi < 0) continue;                    /* no prefecture → skip, never guess */
    if (nseen < MAX_CASES) seen[nseen++] = caseno;

    /* municipality = text between the prefecture name and the "（国内" marker */
    char muni[96] = {0};
    const char *ms = pat + strlen(PREFS[pi].ja);
    const char *me = d;
    while (me > ms && (me[-1] == ' ' || me[-1] == '(')) me--;
    /* trim the leading "（国内" / "（" that precedes the digits */
    static const char *OPENS[] = { "（国内", "（", "(国内", "(", NULL };
    for (int k = 0; OPENS[k]; k++) {
      size_t L = strlen(OPENS[k]);
      if ((size_t)(me - ms) >= L && !strncmp(me - L, OPENS[k], L)) { me -= L; break; }
    }
    if (me > ms && (size_t)(me - ms) < sizeof muni) {
      memcpy(muni, ms, (size_t)(me - ms)); muni[me - ms] = 0;
      size_t L = strlen(muni);
      while (L && muni[L-1] == ' ') muni[--L] = 0;
    }

    /* date of the line (令和X年Y月Z日) — the MAFF investigation date */
    char when[16] = {0};
    const char *lend = q + 200;
    if (lend > text + strlen(text)) lend = text + strlen(text);
    reiwa_date(q, lend, when, sizeof when);

    char oid[64];
    snprintf(oid, sizeof oid, "HPAI_%s_%d", PREFS[pi].code, caseno);
    char title[256];
    snprintf(title, sizeof title, "HPAI 国内%d例目 — %s%s", caseno,
             PREFS[pi].ja, muni);

    cJSON *f = gj_point_feature(PREFS[pi].lon, PREFS[pi].lat);

    cJSON *p = cJSON_CreateObject();
    /* outbreak_id is NOT in lib/geojson.c NATIVE_ID_KEYS, so without this the
     * uid fell back to a sha1 over {geometry, properties} — i.e. any change to
     * the title or a newly-parsed municipality re-identified the row and
     * inserted a duplicate instead of updating it. "uid" IS a native id key. */
    cJSON_AddStringToObject(p, "uid", oid);
    cJSON_AddStringToObject(p, "outbreak_id", oid);
    cJSON_AddStringToObject(p, "title", title);
    cJSON_AddNumberToObject(p, "case_no", caseno);
    cJSON_AddStringToObject(p, "prefecture", PREFS[pi].ja);
    cJSON_AddStringToObject(p, "prefecture_en", PREFS[pi].en);
    if (muni[0]) cJSON_AddStringToObject(p, "municipality", muni);
    else         cJSON_AddNullToObject(p, "municipality");
    if (when[0]) cJSON_AddStringToObject(p, "published_at", when);
    cJSON_AddStringToObject(p, "disease", "高病原性鳥インフルエンザ (HPAI)");
    /* The page names a municipality but publishes no coordinate; the point is
     * the PREFECTURE centroid, so say so rather than implying farm-level geo. */
    cJSON_AddStringToObject(p, "geo_precision", "prefecture_centroid");
    cJSON_AddStringToObject(p, "link", season_url);
    cJSON_AddStringToObject(p, "source", "maff_hpai");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  free(text);

  if (cJSON_GetArraySize(features) == 0) {
    cJSON_Delete(features);
    fprintf(stderr, "[bird-flu-outbreaks] no cases on %s\n", season_url);
    return 0;                       /* an empty season is honest, not an error */
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[bird-flu-outbreaks] emitted %d from %s\n", n, season_url);
  return 0;
}

static const source_def bird_flu_outbreaks_def = {
  .id = "bird-flu-outbreaks", .collector = "agriculture",
  .name = "Bird Flu Outbreaks", .name_ja = "高病原性鳥インフルエンザ",
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(bird_flu_outbreaks_def)
