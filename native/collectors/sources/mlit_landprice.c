/* collectors/economy/sources/mlit_landprice.c
 * Port of server/src/collectors/mlitLandprice.js (fetchJson, no api key).
 * Iterates prefectures p=01..47 over the most recent 6 quarters (from=Q1,
 * to=Q6) of the MLIT 不動産取引価格 TradeListSearch API, geocoding every
 * trade to its municipality centroid (embedded table; falls back to the
 * prefecture-capital entry keyed by the 2-digit prefecture code).
 * SEED_PRICES / _meta dropped (port guide rule 8). Feature uid/title are
 * derived by the geojson sink. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#define API_URL "https://www.land.mlit.go.jp/webland/api/TradeListSearch"

/* MUNICIPALITY_CENTROIDS embedded verbatim: PREFECTURE_CAPITALS (key
 * '01'..'47') first, then MUNICIPALITIES (5-digit JIS) — exactly the JS
 * { ...PREFECTURE_CAPITALS, ...MUNICIPALITIES } merge (no key collisions).
 * The municipalityCenter() lookup is centroid[String(cityCode)] ||
 * centroid[prefCode] so a 5-digit hit wins, else the 2-digit capital. */
typedef struct { const char *code; double lat, lon; } centroid_t;
static const centroid_t CENTROIDS[] = {
  {"01",43.0642,141.3469},{"02",40.8244,140.7400},{"03",39.7036,141.1525},
  {"04",38.2682,140.8721},{"05",39.7186,140.1024},{"06",38.2406,140.3631},
  {"07",37.7503,140.4675},{"08",36.3418,140.4468},{"09",36.5658,139.8836},
  {"10",36.3911,139.0608},{"11",35.8569,139.6489},{"12",35.6047,140.1233},
  {"13",35.6895,139.6917},{"14",35.4478,139.6425},{"15",37.9028,139.0234},
  {"16",36.6953,137.2113},{"17",36.5947,136.6256},{"18",36.0652,136.2216},
  {"19",35.6642,138.5683},{"20",36.6513,138.1810},{"21",35.3912,136.7223},
  {"22",34.9756,138.3828},{"23",35.1815,136.9066},{"24",34.7303,136.5086},
  {"25",35.0045,135.8686},{"26",35.0116,135.7681},{"27",34.6937,135.5023},
  {"28",34.6913,135.1830},{"29",34.6851,135.8050},{"30",34.2261,135.1675},
  {"31",35.5036,134.2383},{"32",35.4723,133.0505},{"33",34.6618,133.9344},
  {"34",34.3853,132.4553},{"35",34.1858,131.4706},{"36",34.0658,134.5594},
  {"37",34.3401,134.0434},{"38",33.8417,132.7657},{"39",33.5597,133.5311},
  {"40",33.6064,130.4181},{"41",33.2494,130.2989},{"42",32.7448,129.8737},
  {"43",32.7898,130.7417},{"44",33.2382,131.6126},{"45",31.9077,131.4202},
  {"46",31.5602,130.5581},{"47",26.2125,127.6809},
  /* MUNICIPALITIES — Hokkaido */
  {"01100",43.0642,141.3469},{"01202",41.7686,140.7289},
  {"01203",43.7706,142.3650},{"01204",42.3344,140.9747},
  {"01205",43.1907,140.9947},{"01206",42.9849,144.3819},
  {"01207",42.9039,143.2042},{"01208",43.8211,144.0944},
  {"01210",42.6444,141.5942},
  /* Tohoku */
  {"02201",40.8222,140.7475},{"02202",40.6111,141.4886},
  {"03201",39.7036,141.1525},{"03203",38.4344,141.3036},
  {"04100",38.2682,140.8721},{"04202",38.4344,141.3036},
  {"05201",39.7186,140.1024},{"06201",38.2406,140.3631},
  {"07201",37.7608,140.4736},{"07203",37.4006,140.3597},
  {"07204",36.9447,140.8881},
  /* Kanto */
  {"08201",36.3658,140.4714},{"08202",36.3418,140.4468},
  {"08220",36.0837,140.0764},{"09201",36.5552,139.8828},
  {"10201",36.3895,139.0635},{"10202",36.3220,139.0030},
  {"11100",35.8617,139.6455},{"11202",35.8252,139.6889},
  {"11203",35.9911,139.4806},{"11217",35.9242,139.5697},
  {"12100",35.6075,140.1064},{"12203",35.7350,140.0211},
  {"12217",35.7780,140.0303},{"12219",35.6500,139.9486},
  {"12220",35.7333,139.9028},{"13101",35.6940,139.7536},
  {"13102",35.6705,139.7720},{"13103",35.6580,139.7515},
  {"13104",35.6939,139.7036},{"13105",35.7080,139.7525},
  {"13106",35.7128,139.7800},{"13107",35.7106,139.8014},
  {"13108",35.6731,139.8175},{"13109",35.6092,139.7300},
  {"13110",35.6411,139.6982},{"13111",35.5614,139.7161},
  {"13112",35.6464,139.6533},{"13113",35.6614,139.6975},
  {"13114",35.7077,139.6650},{"13115",35.6996,139.6363},
  {"13116",35.7263,139.7165},{"13117",35.7531,139.7339},
  {"13118",35.7361,139.7831},{"13119",35.7511,139.7092},
  {"13120",35.7358,139.6517},{"13121",35.7758,139.8047},
  {"13122",35.7434,139.8475},{"13123",35.7064,139.8683},
  {"13201",35.6580,139.4011},{"13202",35.7050,139.4694},
  {"13203",35.7038,139.5806},{"13207",35.6986,139.5141},
  {"14100",35.4478,139.6425},{"14130",35.5308,139.7028},
  {"14150",35.5713,139.3729},{"14201",35.4395,139.4400},
  {"14203",35.3192,139.5469},
  /* Chubu */
  {"15100",37.9161,139.0364},{"15202",37.9047,139.0228},
  {"16201",36.6953,137.2113},{"17201",36.5613,136.6562},
  {"18201",36.0648,136.2222},{"19201",35.6622,138.5683},
  {"20201",36.6485,138.1948},{"20202",36.3433,138.0000},
  {"21201",35.4233,136.7606},{"22100",34.9756,138.3828},
  {"22130",34.7108,137.7261},{"22203",35.1067,138.8636},
  {"22220",35.1633,138.6961},{"23100",35.1814,136.9067},
  {"23202",34.7689,137.3914},{"23207",34.9583,137.1517},
  {"23211",35.0828,137.1556},{"24201",34.7186,136.5057},
  {"24202",34.9658,136.6244},
  /* Kansai */
  {"25201",35.0048,135.8686},{"26100",35.0114,135.7681},
  {"27100",34.6937,135.5023},{"27140",34.5733,135.4828},
  {"27203",34.7369,135.4061},{"27205",34.7700,135.3600},
  {"27207",34.7444,135.3589},{"28100",34.6913,135.1830},
  {"28201",34.8170,134.6916},{"28203",34.7367,135.3422},
  {"28204",34.7383,135.4194},{"29201",34.6852,135.8050},
  {"30201",34.2261,135.1675},
  /* Chugoku/Shikoku */
  {"31201",35.5011,134.2350},{"32201",35.4683,133.0481},
  {"33100",34.6553,133.9192},{"33202",34.5860,133.7700},
  {"34100",34.3963,132.4596},{"34202",34.4002,132.7064},
  {"34207",34.3987,132.4750},{"35201",34.1858,131.4706},
  {"36201",34.0707,134.5547},{"37201",34.3431,134.0467},
  {"38201",33.8392,132.7656},{"39201",33.5597,133.5311},
  /* Kyushu */
  {"40100",33.5904,130.4017},{"40130",33.8836,130.8814},
  {"40203",33.3203,130.5078},{"41201",33.2494,130.2989},
  {"42201",32.7497,129.8775},{"42202",32.7989,129.8742},
  {"43100",32.8033,130.7081},{"44201",33.2382,131.6126},
  {"45201",31.9077,131.4202},{"46201",31.5602,130.5581},
  {"47201",26.2125,127.6809},
};
#define NCENT ((int)(sizeof CENTROIDS / sizeof CENTROIDS[0]))

static const centroid_t *centroid_by_code(const char *code) {
  if (!code || !code[0]) return NULL;
  for (int i = 0; i < NCENT; i++)
    if (strcmp(CENTROIDS[i].code, code) == 0) return &CENTROIDS[i];
  return NULL;
}

/* municipalityCenter(prefCode, cityCode):
 *   CENTROIDS[String(cityCode)] || CENTROIDS[prefCode.padStart(2,'0')] */
static const centroid_t *municipality_center(const char *pref,
                                             const char *cityCode) {
  const centroid_t *c = centroid_by_code(cityCode);
  if (c) return c;
  return centroid_by_code(pref);   /* pref is already 2-digit zero-padded */
}

/* JSON value → string (cJSON string passes through; numbers stringified
 * like JS String()). Returns NULL for missing/null. */
static const char *jstr(cJSON *o, const char *k, char *buf, size_t n) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v || cJSON_IsNull(v)) return NULL;
  if (cJSON_IsString(v)) return v->valuestring[0] ? v->valuestring : NULL;
  if (cJSON_IsNumber(v)) {
    double d = v->valuedouble;
    if (d == (double)(long long)d)
      snprintf(buf, n, "%lld", (long long)d);
    else
      snprintf(buf, n, "%g", d);
    return buf;
  }
  return NULL;
}

/* parseFloat semantics: leading numeric prefix; NaN if none. The MLIT API
 * returns these as plain numeric strings. */
static int parse_float(const char *s, double *out) {
  if (!s) return 0;
  char *end = NULL;
  double v = strtod(s, &end);
  if (end == s) return 0;
  *out = v;
  return 1;
}

/* deterministic stand-in for Math.random()-0.5 jitter (JS is non-
 * deterministic here too; the geojson uid hashes geometry+props so parity
 * is best-effort, exactly as for the JS run). */
static double jit(unsigned *st) {
  *st = *st * 1103515245u + 12345u;
  return ((double)((*st >> 16) & 0x7fff) / 32767.0 - 0.5) * 0.02;
}

/* recentQuarters(6): walk back from current UTC quarter, then reverse, so
 * out[0] = oldest, out[5] = current. We only need from=out[0], to=out[5]. */
static void recent_quarters(char *fromQ, char *toQ) {
  time_t now = time(NULL);
  struct tm tmv; gmtime_r(&now, &tmv);
  int y = tmv.tm_year + 1900;
  int q = tmv.tm_mon / 3 + 1;
  /* current quarter is the last (newest) element */
  snprintf(toQ, 8, "%d%d", y, q);
  /* walk back 5 more to reach the oldest (first) element */
  for (int i = 0; i < 5; i++) {
    q -= 1;
    if (q < 1) { q = 4; y -= 1; }
  }
  snprintf(fromQ, 8, "%d%d", y, q);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char fromQ[8], toQ[8];
  recent_quarters(fromQ, toQ);

  cJSON *features = cJSON_CreateArray();
  unsigned rng = 0x9e3779b9u;

  for (int p = 1; p <= 47; p++) {
    char pref[4];
    snprintf(pref, sizeof pref, "%02d", p);
    char url[256];
    snprintf(url, sizeof url, "%s?from=%s&to=%s&area=%s",
             API_URL, fromQ, toQ, pref);
    cJSON *data = feed_get_json(ctx->http, url, 20000);
    if (!data) continue;
    cJSON *trades = cJSON_GetObjectItem(data, "data");
    if (!cJSON_IsArray(trades)) { cJSON_Delete(data); continue; }

    int tn = cJSON_GetArraySize(trades);
    for (int i = 0; i < tn; i++) {
      cJSON *t = cJSON_GetArrayItem(trades, i);
      char b1[64], b2[64];
      double price, area;
      if (!parse_float(jstr(t, "TradePrice", b1, sizeof b1), &price)) continue;
      if (!parse_float(jstr(t, "Area", b2, sizeof b2), &area)) continue;
      if (!isfinite(price) || !isfinite(area) || area <= 0) continue;

      char mb[64];
      const char *muniCode = jstr(t, "MunicipalityCode", mb, sizeof mb);
      const centroid_t *center = municipality_center(pref, muniCode);
      if (!center) continue;

      double jitterLat = jit(&rng);
      double jitterLon = jit(&rng);

      /* point_id = MLIT_<pref>_<intelHashKey(MunicipalityCode, DistrictName,
       *   TradePrice, Area, Period, Type, Purpose)> */
      char hb[5][64];
      const char *muni = jstr(t, "MunicipalityCode", hb[0], sizeof hb[0]);
      const char *district = cJSON_IsString(
        cJSON_GetObjectItem(t, "DistrictName"))
        ? cJSON_GetObjectItem(t, "DistrictName")->valuestring : NULL;
      const char *tpStr = jstr(t, "TradePrice", hb[1], sizeof hb[1]);
      const char *arStr = jstr(t, "Area", hb[2], sizeof hb[2]);
      const char *period = cJSON_IsString(cJSON_GetObjectItem(t, "Period"))
        ? cJSON_GetObjectItem(t, "Period")->valuestring : NULL;
      const char *typ = cJSON_IsString(cJSON_GetObjectItem(t, "Type"))
        ? cJSON_GetObjectItem(t, "Type")->valuestring : NULL;
      const char *purpose = cJSON_IsString(cJSON_GetObjectItem(t, "Purpose"))
        ? cJSON_GetObjectItem(t, "Purpose")->valuestring : NULL;
      const char *parts[7] = { muni, district, tpStr, arStr,
                               period, typ, purpose };
      char hk[21];
      feed_hash_key(hk, parts, 7);
      char pid[64];
      snprintf(pid, sizeof pid, "MLIT_%s_%s", pref, hk);

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(center->lon + jitterLon));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(center->lat + jitterLat));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *pr = cJSON_CreateObject();      /* EXACT JS key order */
      cJSON_AddStringToObject(pr, "point_id", pid);
      cJSON_AddNumberToObject(pr, "price_total_yen", round(price));
      cJSON_AddNumberToObject(pr, "area_sqm", area);
      cJSON_AddNumberToObject(pr, "price_per_sqm", round(price / area));
      /* t?.Type || null  (string-or-null) */
      cJSON_AddItemToObject(pr, "land_use",
        (typ && typ[0]) ? cJSON_CreateString(typ) : cJSON_CreateNull());
      cJSON_AddItemToObject(pr, "purpose",
        (purpose && purpose[0]) ? cJSON_CreateString(purpose)
                                : cJSON_CreateNull());
      {
        cJSON *st = cJSON_GetObjectItem(t, "Structure");
        const char *sv = (st && cJSON_IsString(st) && st->valuestring[0])
          ? st->valuestring : NULL;
        cJSON_AddItemToObject(pr, "structure",
          sv ? cJSON_CreateString(sv) : cJSON_CreateNull());
      }
      {
        cJSON *pf = cJSON_GetObjectItem(t, "Prefecture");
        const char *pv = (pf && cJSON_IsString(pf) && pf->valuestring[0])
          ? pf->valuestring : NULL;
        cJSON_AddItemToObject(pr, "prefecture",
          pv ? cJSON_CreateString(pv) : cJSON_CreateNull());
      }
      {
        cJSON *mu = cJSON_GetObjectItem(t, "Municipality");
        const char *mv = (mu && cJSON_IsString(mu) && mu->valuestring[0])
          ? mu->valuestring : NULL;
        cJSON_AddItemToObject(pr, "municipality",
          mv ? cJSON_CreateString(mv) : cJSON_CreateNull());
      }
      cJSON_AddItemToObject(pr, "district",
        (district && district[0]) ? cJSON_CreateString(district)
                                  : cJSON_CreateNull());
      cJSON_AddItemToObject(pr, "period",
        (period && period[0]) ? cJSON_CreateString(period)
                              : cJSON_CreateNull());
      cJSON_AddStringToObject(pr, "source", "mlit_webland_live");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
    }
    cJSON_Delete(data);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[mlit-landprice] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def mlit_landprice_def = {
  .id = "mlit-landprice", .collector = "economy",
  .name = "MLIT Land Price Survey",
  .name_ja = "\xe5\x9b\xbd\xe4\xba\xa4\xe7\x9c\x81 \xe5\x9c\xb0\xe4\xbe\xa1\xe5\x85\xac\xe7\xa4\xba",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(mlit_landprice_def)
