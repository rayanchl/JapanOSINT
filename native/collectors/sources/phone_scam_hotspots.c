/* collectors/crime/sources/phone_scam_hotspots.c
 * Port of server/src/collectors/phoneScamHotspots.js, REPAIRED 2026-11-14.
 *
 * What it used to do: GET ten prefectural police 特殊詐欺 pages at guessed
 * paths and regex 認知件数 / 被害額 out of each. Measured 2026-11-14: nine of
 * the ten paths now answer HTTP 404 (and Osaka's redirects to the site root),
 * and feed_get_text() hands back an error page's body, so the row fetched
 * ~227 KB over 12 requests and stored nothing at rc=0 — the invisible nothing.
 *
 * What the upstream really publishes: the NPA's SOS47 landing page
 * (https://www.npa.go.jp/bureau/safetylife/sos47/) carries a 発生状況 block —
 *   期間：2025/1～2025/12  認知件数合計 43,000 件  合計被害額 約3,257 億
 *   4,258 万 7,000 円  被害件数上位の厳重注意地域 1位 東京 5,879 件 2位 …
 * Every figure here is read from that block and nothing is invented: one
 * national record plus one record per ranked prefecture, placed at the
 * prefecture centroid (geo_precision says so). If the page was fetched and the
 * block did not parse, the run returns -1 so the scheduler files a
 * fetch-failure record instead of a silent rc=0. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NPA_URL "https://www.npa.go.jp/bureau/safetylife/sos47/"

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

static const char *skip_ws(const char *p) {
  while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
  return p;
}

/* digits with commas starting exactly at p; -1 if none. Advances *pp. */
static long read_num(const char **pp) {
  const char *p = *pp; long v = 0; int any = 0;
  while ((*p >= '0' && *p <= '9') || *p == ',') {
    if (*p != ',') { v = v * 10 + (*p - '0'); any = 1; }
    p++;
    if (v > 100000000000000L) return -1;
  }
  if (!any) return -1;
  *pp = p; return v;
}

/* After `label`, read "<n> 億 <n> 万 <n> 円" (any subset, optional leading 約)
 * into yen. Returns 1 if a 円-terminated amount parsed. */
static int read_yen(const char *text, const char *label, double *yen) {
  const char *p = strstr(text, label);
  if (!p) return 0;
  p = skip_ws(p + strlen(label));
  if (!strncmp(p, "約", strlen("約"))) p = skip_ws(p + strlen("約"));
  double total = 0; int got = 0;
  for (int guard = 0; guard < 4; guard++) {
    long n = read_num(&p);
    if (n < 0) break;
    p = skip_ws(p);
    if (!strncmp(p, "億", strlen("億"))) { total += (double)n * 1e8; p = skip_ws(p + strlen("億")); got = 1; }
    else if (!strncmp(p, "万", strlen("万"))) { total += (double)n * 1e4; p = skip_ws(p + strlen("万")); got = 1; }
    else if (!strncmp(p, "円", strlen("円"))) { total += (double)n; got = 1; break; }
    else break;
  }
  if (!got) return 0;
  *yen = total; return 1;
}

static int pref_lookup(const char *name, size_t len) {
  if (len < 6) return -1;                       /* two CJK chars at least */
  for (int i = 0; i < NPREF; i++)
    if (!strncmp(PREFS[i].ja, name, len)) return i;
  return -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *html = feed_get_text(ctx->http, NPA_URL, 15000);
  if (!html || strlen(html) < 50) {
    free(html);
    fprintf(stderr, "[phone-scam-hotspots] NPA SOS47 page unavailable: %s\n", NPA_URL);
    return -1;
  }
  char *text = html_strip(html);
  free(html);
  if (!text) return -1;

  const char *blk = strstr(text, "認知件数合計");
  if (!blk) {
    fprintf(stderr, "[phone-scam-hotspots] SOS47 fetched but no 認知件数合計 block — layout changed\n");
    free(text); return -1;
  }
  const char *q = skip_ws(blk + strlen("認知件数合計"));
  long incidents = read_num(&q);
  double damage = 0; int has_dmg = read_yen(blk, "合計被害額", &damage);

  /* period: text after "期間：" up to the label, trimmed */
  char period[64] = {0};
  {
    const char *ps = NULL;
    for (const char *s = blk; s > text && blk - s < 120; s--)
      if (!strncmp(s, "期間：", strlen("期間："))) { ps = s + strlen("期間："); break; }
    if (ps && blk > ps && (size_t)(blk - ps) < sizeof period) {
      memcpy(period, ps, (size_t)(blk - ps));
      size_t L = strlen(period);
      while (L && (period[L-1] == ' ' || period[L-1] == '\n')) period[--L] = 0;
    }
  }

  cJSON *features = cJSON_CreateArray();
  if (incidents >= 0) {
    cJSON *f = gj_point_feature(138.2529, 36.2048);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "uid", "phone-scam:npa-sos47:national");
    cJSON_AddStringToObject(p, "title", "特殊詐欺 認知件数 全国 (警察庁 SOS47)");
    cJSON_AddStringToObject(p, "ward", "全国");
    cJSON_AddStringToObject(p, "prefecture", "全国");
    cJSON_AddNumberToObject(p, "incidents_yr", (double)incidents);
    if (has_dmg) cJSON_AddNumberToObject(p, "damage_yen", damage);
    else cJSON_AddNullToObject(p, "damage_yen");
    if (period[0]) cJSON_AddStringToObject(p, "period", period);
    cJSON_AddStringToObject(p, "geo_precision", "national_centroid");
    cJSON_AddStringToObject(p, "provisional", "数値は全て暫定値 (as published)");
    cJSON_AddStringToObject(p, "source_url", NPA_URL);
    cJSON_AddStringToObject(p, "country", "JP");
    cJSON_AddStringToObject(p, "source", "npa_sos47");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }

  /* "1位 東京 5,879 件 2位 大阪 4,830 件 …" after the ranking heading */
  const char *rk = strstr(blk, "厳重注意地域");
  if (rk) {
    rk += strlen("厳重注意地域");
    for (int rank = 1; rank <= 10; rank++) {
      rk = skip_ws(rk);
      long r = read_num(&rk);
      if (r != rank || strncmp(rk, "位", strlen("位"))) break;
      rk = skip_ws(rk + strlen("位"));
      const char *ns = rk;
      while (*rk && *rk != ' ' && !(*rk >= '0' && *rk <= '9')) rk++;
      size_t nl = (size_t)(rk - ns);
      rk = skip_ws(rk);
      long cnt = read_num(&rk);
      if (cnt < 0 || nl == 0 || nl > 24) break;
      rk = skip_ws(rk);
      if (!strncmp(rk, "件", strlen("件"))) rk += strlen("件");
      int pi = pref_lookup(ns, nl);
      if (pi < 0) { fprintf(stderr, "[phone-scam-hotspots] rank %d: unmapped prefecture name\n", rank); continue; }
      cJSON *f = gj_point_feature(PREFS[pi].lon, PREFS[pi].lat);
      cJSON *p = cJSON_CreateObject();
      char uidb[64]; snprintf(uidb, sizeof uidb, "phone-scam:npa-sos47:pref:%s", PREFS[pi].code);
      char title[160]; snprintf(title, sizeof title, "特殊詐欺 認知件数 %s %d位 (警察庁 SOS47)", PREFS[pi].ja, rank);
      cJSON_AddStringToObject(p, "uid", uidb);
      cJSON_AddStringToObject(p, "title", title);
      cJSON_AddStringToObject(p, "ward", PREFS[pi].ja);
      cJSON_AddStringToObject(p, "prefecture", PREFS[pi].ja);
      cJSON_AddStringToObject(p, "prefecture_en", PREFS[pi].en);
      cJSON_AddNumberToObject(p, "rank", rank);
      cJSON_AddNumberToObject(p, "incidents_yr", (double)cnt);
      if (period[0]) cJSON_AddStringToObject(p, "period", period);
      cJSON_AddStringToObject(p, "geo_precision", "prefecture_centroid");
      cJSON_AddStringToObject(p, "source_url", NPA_URL);
      cJSON_AddStringToObject(p, "country", "JP");
      cJSON_AddStringToObject(p, "source", "npa_sos47");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
  }
  free(text);

  if (cJSON_GetArraySize(features) == 0) {
    cJSON_Delete(features);
    fprintf(stderr, "[phone-scam-hotspots] SOS47 fetched, nothing parsed from the 発生状況 block\n");
    return -1;
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[phone-scam-hotspots] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def phone_scam_hotspots_def = {
  .id = "phone-scam-hotspots", .collector = "crime",
  .name = "Phone Scam Hotspots", .name_ja = "特殊詐欺発生地点",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(phone_scam_hotspots_def)
