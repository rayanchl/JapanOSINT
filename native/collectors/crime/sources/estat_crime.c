/* collectors/crime/sources/estat_crime.c
 * Port of server/src/collectors/eStatCrime.js. e-Stat getStatsData API
 * (NPA crime totals, statsDataId default 0003433388). Always emits one
 * directory intel row (uid estat-crime|portal); with ESTAT_APP_ID set it
 * also emits one map feature per (prefecture, year). The _meta envelope is
 * dropped (rule 7) — the baseIntel directory row is the intel-status row
 * the collector exists to publish, so it is kept. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SOURCE_ID  "estat-crime"
#define PORTAL_URL "https://www.e-stat.go.jp/stat-search/database?toukei=00130001"
#define REGISTER_URL "https://www.e-stat.go.jp/api/"
#define API_BASE   "https://api.e-stat.go.jp/rest/3.0/app/json/getStatsData"
#define DEFAULT_STATS_DATA_ID "0003433388"

typedef struct { const char *code, *en, *ja; double lat, lon; } pref_t;
static const pref_t PREFS[] = {
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
#define NPREF ((int)(sizeof(PREFS)/sizeof(PREFS[0])))

/* JS s.endsWith(都|道|府|県) → strip that one trailing 3-byte UTF-8 char */
static int has_pref_suffix(const char *s, size_t L) {
  if (L < 3) return 0;
  const unsigned char *t = (const unsigned char *)(s + L - 3);
  static const char *suf[] = { "都","道","府","県" };
  for (int i = 0; i < 4; i++)
    if (!memcmp(t, suf[i], 3)) return 1;
  return 0;
}

/* port of resolvePrefecture(input): code | ja | en(lower) | ja-stripped */
static const pref_t *resolve_pref(const char *in) {
  if (!in) return NULL;
  while (*in == ' ' || *in == '\t') in++;
  if (!*in) return NULL;
  size_t L = strlen(in);
  while (L && (in[L-1]==' '||in[L-1]=='\t')) L--;
  char s[128];
  if (L >= sizeof s) L = sizeof s - 1;
  memcpy(s, in, L); s[L] = 0;

  for (int i = 0; i < NPREF; i++) if (!strcmp(s, PREFS[i].code)) return &PREFS[i];
  for (int i = 0; i < NPREF; i++) if (!strcmp(s, PREFS[i].ja)) return &PREFS[i];
  char lo[128]; snprintf(lo, sizeof lo, "%s", s);
  for (char *q = lo; *q; q++) if (*q>='A'&&*q<='Z') *q += 32;
  for (int i = 0; i < NPREF; i++) {
    char el[64]; snprintf(el, sizeof el, "%s", PREFS[i].en);
    for (char *q = el; *q; q++) if (*q>='A'&&*q<='Z') *q += 32;
    if (!strcmp(lo, el)) return &PREFS[i];
  }
  /* stripped: drop one trailing 都/道/府/県 from s, compare to stripped ja */
  size_t sl = strlen(s);
  size_t scut = (sl >= 3 && has_pref_suffix(s, sl)) ? sl - 3 : sl;
  for (int i = 0; i < NPREF; i++) {
    const char *ja = PREFS[i].ja; size_t jl = strlen(ja);
    size_t jcut = (jl >= 3 && has_pref_suffix(ja, jl)) ? jl - 3 : jl;
    if (jcut == scut && !memcmp(ja, s, jcut)) return &PREFS[i];
  }
  return NULL;
}

static const char *at(const cJSON *o, const char *k) {
  const cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

/* timeToYearMonth: '2023年12月'→'2023-12', '2023年'/'2023'→'2023-12' */
static int time_ym(const char *label, char *out, size_t n) {
  if (!label) return 0;
  /* find 4-digit year */
  const char *y = NULL;
  for (const char *p = label; *p; p++) {
    if (p[0]>='0'&&p[0]<='9'&&p[1]>='0'&&p[1]<='9'&&p[2]>='0'&&p[2]<='9'&&p[3]>='0'&&p[3]<='9') { y = p; break; }
  }
  if (!y) return 0;
  char ys[5]; memcpy(ys, y, 4); ys[4]=0;
  /* look for 年 <digits> 月 after the year for a month */
  const char *nen = strstr(y, "年");
  if (nen) {
    const char *m = nen + 3;
    while (*m==' ') m++;
    if (*m>='0'&&*m<='9') {
      int mo = atoi(m);
      const char *gt = m; while (*gt>='0'&&*gt<='9') gt++;
      while (*gt==' ') gt++;
      if (!strncmp(gt, "月", 3) && mo >= 1 && mo <= 99) {
        snprintf(out, n, "%s-%02d", ys, mo);
        return 1;
      }
    }
  }
  snprintf(out, n, "%s-12", ys);
  return 1;
}

/* build {@code:@name} map for one CLASS_OBJ into a cJSON object */
static cJSON *class_map(const cJSON *obj) {
  cJSON *m = cJSON_CreateObject();
  const cJSON *cls = cJSON_GetObjectItem(obj, "CLASS");
  if (cJSON_IsArray(cls)) {
    const cJSON *c;
    cJSON_ArrayForEach(c, cls) {
      const char *cd = at(c, "@code"), *nm = at(c, "@name");
      if (cd) cJSON_AddStringToObject(m, cd, nm ? nm : "");
    }
  } else if (cJSON_IsObject(cls)) {
    const char *cd = at(cls, "@code"), *nm = at(cls, "@name");
    if (cd) cJSON_AddStringToObject(m, cd, nm ? nm : "");
  }
  return m;
}

static const char *map_get(cJSON *maps, const char *id, const char *code) {
  if (!id || !code) return NULL;
  cJSON *m = cJSON_GetObjectItem(maps, id);
  const cJSON *v = m ? cJSON_GetObjectItem(m, code) : NULL;
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char now[32];
  time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
  strftime(now, sizeof now, "%Y-%m-%dT%H:%M:%S", &g);
  size_t nl = strlen(now); snprintf(now + nl, sizeof now - nl, ".000Z");

  const char *id = getenv("ESTAT_APP_ID");
  int configured = id && *id;

  /* ---- baseIntel directory row (always) ---- */
  cJSON *bt = cJSON_CreateArray();
  cJSON_AddItemToArray(bt, cJSON_CreateString("crime"));
  cJSON_AddItemToArray(bt, cJSON_CreateString("statistics"));
  cJSON_AddItemToArray(bt, cJSON_CreateString("estat"));
  cJSON_AddItemToArray(bt, cJSON_CreateString("national"));
  char *btj = cJSON_PrintUnformatted(bt);

  cJSON *bp = cJSON_CreateObject();            /* EXACT JS key order */
  cJSON_AddStringToObject(bp, "portal_url", PORTAL_URL);
  cJSON_AddStringToObject(bp, "register_url", REGISTER_URL);
  cJSON_AddStringToObject(bp, "stats_code", "00130001");
  cJSON_AddBoolToObject(bp, "configured", configured ? 1 : 0);
  cJSON_AddStringToObject(bp, "env_hint",
    "Set ESTAT_APP_ID and (optionally) ESTAT_CRIME_STATS_DATA_ID to populate map features.");
  char *bpj = cJSON_PrintUnformatted(bp);

  intel_item bi = {0};
  bi.remote_key   = "portal";                  /* uid estat-crime|portal */
  bi.title        = "e-Stat 犯罪統計 (Crime Statistics)";
  bi.summary      = "Government Statistics portal — per-prefecture annual crime totals from NPA. Requires free ESTAT_APP_ID.";
  bi.link         = PORTAL_URL;
  bi.lang         = "ja";
  bi.published_at = now;
  bi.tags_json    = btj;
  bi.properties_json = bpj;
  sink->emit(sink, &bi);
  free(btj); free(bpj); cJSON_Delete(bt); cJSON_Delete(bp);

  if (!configured) {                           /* JS: no app ID → done */
    fprintf(stderr, "[estat-crime] no ESTAT_APP_ID; directory only\n");
    return 0;
  }

  const char *sdid = getenv("ESTAT_CRIME_STATS_DATA_ID");
  if (!sdid || !*sdid) sdid = DEFAULT_STATS_DATA_ID;

  char url[512];
  snprintf(url, sizeof url,
    API_BASE "?appId=%s&statsDataId=%s&metaGetFlg=Y&limit=100000", id, sdid);
  cJSON *json = feed_get_json(ctx->http, url, 20000);
  if (!json) { fprintf(stderr, "[estat-crime] api unreachable\n"); return 0; }

  cJSON *stat = cJSON_GetObjectItem(
                  cJSON_GetObjectItem(json, "GET_STATS_DATA"),
                  "STATISTICAL_DATA");
  cJSON *clsInf = stat ? cJSON_GetObjectItem(stat, "CLASS_INF") : NULL;
  cJSON *clsObj = clsInf ? cJSON_GetObjectItem(clsInf, "CLASS_OBJ") : NULL;
  cJSON *dataInf = stat ? cJSON_GetObjectItem(stat, "DATA_INF") : NULL;
  cJSON *valArr = dataInf ? cJSON_GetObjectItem(dataInf, "VALUE") : NULL;
  if (!cJSON_IsArray(valArr)) { cJSON_Delete(json); return 0; }

  /* classMaps[@id] = {code:name} */
  cJSON *maps = cJSON_CreateObject();
  if (cJSON_IsArray(clsObj)) {
    cJSON *o;
    cJSON_ArrayForEach(o, clsObj) {
      const char *cid = at(o, "@id");
      if (cid) cJSON_AddItemToObject(maps, cid, class_map(o));
    }
  } else if (cJSON_IsObject(clsObj)) {
    const char *cid = at(clsObj, "@id");
    if (cid) cJSON_AddItemToObject(maps, cid, class_map(clsObj));
  }

  /* pivot: key "<code>|<ym>" → {pref, ym, indicators{}} preserved in
   * first-seen order via a parallel cJSON array of buckets. */
  cJSON *buckets = cJSON_CreateArray();        /* [{key,prefIdx,ym,ind{}}] */

  cJSON *v;
  cJSON_ArrayForEach(v, valArr) {
    const char *areaCode = at(v, "@area");
    if (!areaCode) areaCode = at(v, "@cat01");
    const char *timeCode = at(v, "@time");
    const char *icode = at(v, "@cat01");
    if (!icode) icode = at(v, "@tab");

    const cJSON *dollar = cJSON_GetObjectItem(v, "$");
    char raw[64] = "";
    if (dollar && cJSON_IsString(dollar)) snprintf(raw, sizeof raw, "%s", dollar->valuestring);
    else if (dollar && cJSON_IsNumber(dollar)) snprintf(raw, sizeof raw, "%.0f", cJSON_GetNumberValue(dollar));
    char nocomma[64]; size_t j = 0;
    for (size_t i = 0; raw[i] && j < sizeof nocomma - 1; i++)
      if (raw[i] != ',') nocomma[j++] = raw[i];
    nocomma[j] = 0;
    char *end; long value = strtol(nocomma, &end, 10);
    if (end == nocomma) continue;              /* !Number.isFinite */

    const char *areaName = map_get(maps, "area", areaCode);
    if (!areaName) areaName = map_get(maps, "cat01", areaCode);
    const char *timeName = map_get(maps, "time", timeCode);
    const char *indName = map_get(maps, "cat01", icode);
    if (!indName) indName = map_get(maps, "tab", icode);

    const pref_t *pref = resolve_pref(areaName ? areaName : "");
    if (!pref) continue;
    char ym[16];
    if (!time_ym(timeName, ym, sizeof ym)) continue;

    char key[32];
    snprintf(key, sizeof key, "%s|%s", pref->code, ym);
    cJSON *bk = NULL, *b;
    cJSON_ArrayForEach(b, buckets) {
      const cJSON *bkey = cJSON_GetObjectItem(b, "key");
      if (bkey && !strcmp(bkey->valuestring, key)) { bk = b; break; }
    }
    if (!bk) {
      bk = cJSON_CreateObject();
      cJSON_AddStringToObject(bk, "key", key);
      cJSON_AddNumberToObject(bk, "prefIdx", (double)(pref - PREFS));
      cJSON_AddStringToObject(bk, "ym", ym);
      cJSON_AddItemToObject(bk, "ind", cJSON_CreateObject());
      cJSON_AddItemToArray(buckets, bk);
    }
    if (indName) {
      cJSON *ind = cJSON_GetObjectItem(bk, "ind");
      cJSON *ex = cJSON_GetObjectItem(ind, indName);
      if (ex) cJSON_SetNumberValue(ex, (double)value);
      else cJSON_AddNumberToObject(ind, indName, (double)value);
    }
  }

  /* build features (EXACT JS property key order) */
  cJSON *features = cJSON_CreateArray();
  cJSON *b;
  cJSON_ArrayForEach(b, buckets) {
    int pi = (int)cJSON_GetNumberValue(cJSON_GetObjectItem(b, "prefIdx"));
    const pref_t *p = &PREFS[pi];
    const char *ym = cJSON_GetObjectItem(b, "ym")->valuestring;
    cJSON *ind = cJSON_GetObjectItem(b, "ind");

    const cJSON *rec = cJSON_GetObjectItem(ind, "認知件数");
    if (!rec) rec = cJSON_GetObjectItem(ind, "総数");
    const cJSON *cle = cJSON_GetObjectItem(ind, "検挙件数");
    const cJSON *arr = cJSON_GetObjectItem(ind, "検挙人員");

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *gm = cJSON_CreateObject();
    cJSON_AddStringToObject(gm, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(p->lon));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(p->lat));
    cJSON_AddItemToObject(gm, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", gm);

    cJSON *pr = cJSON_CreateObject();
    char fid[48]; snprintf(fid, sizeof fid, "ESTAT_%s_%s", p->code, ym);
    cJSON_AddStringToObject(pr, "id", fid);
    cJSON_AddStringToObject(pr, "prefecture_code", p->code);
    cJSON_AddStringToObject(pr, "prefecture_ja", p->ja);
    cJSON_AddStringToObject(pr, "prefecture_en", p->en);
    cJSON_AddStringToObject(pr, "year_month", ym);
    cJSON_AddItemToObject(pr, "recognised",
      rec ? cJSON_CreateNumber(cJSON_GetNumberValue(rec)) : cJSON_CreateNull());
    cJSON_AddItemToObject(pr, "cleared",
      cle ? cJSON_CreateNumber(cJSON_GetNumberValue(cle)) : cJSON_CreateNull());
    cJSON_AddItemToObject(pr, "arrests",
      arr ? cJSON_CreateNumber(cJSON_GetNumberValue(arr)) : cJSON_CreateNull());
    cJSON_AddItemToObject(pr, "indicators", cJSON_Duplicate(ind, 1));
    cJSON_AddStringToObject(pr, "source", SOURCE_ID);
    cJSON_AddItemToObject(f, "properties", pr);
    cJSON_AddItemToArray(features, f);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  cJSON_Delete(buckets);
  cJSON_Delete(maps);
  cJSON_Delete(json);
  fprintf(stderr, "[estat-crime] emitted %d features (+1 directory)\n", n);
  return 0;
}

static const source_def estat_crime_def = {
  .id = "estat-crime", .collector = "crime",
  .name = "e-Stat Crime Statistics", .name_ja = "e-Stat 犯罪統計",
   .update_interval_sec = 604800, .run = run,
};
REGISTER_SOURCE(estat_crime_def)
