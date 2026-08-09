/* collectors/crime/sources/pref_police_crime.c — port of
 * server/src/collectors/prefPoliceCrime.js.
 * 47 prefectures (JP_PREFECTURES embedded). Per prefecture: HEAD/GET
 * reachability probe + env-var CSV adapter (codes 13/14/27/23 only) parsed
 * with headers=true → {year_month,count} rows → one map Feature each, and
 * ALWAYS one directory intel item (uid pref-police-crime|<code>).
 * SEED/_meta dropped (rule 7); 0 features when no adapter env vars set. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/csv.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../lib/probe.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TIMEOUT_MS 15000

typedef struct { const char *code, *en, *ja; double lat, lon; const char *portal; } pref_t;

static const pref_t PREFS[] = {
  {"01","Hokkaido","北海道",43.0628,141.3478,"https://www.police.pref.hokkaido.lg.jp/"},
  {"02","Aomori","青森県",40.8244,140.7400,"https://www.police.pref.aomori.jp/"},
  {"03","Iwate","岩手県",39.7036,141.1525,"https://www.pref.iwate.jp/kenkei/"},
  {"04","Miyagi","宮城県",38.2683,140.8719,"https://www.police.pref.miyagi.jp/"},
  {"05","Akita","秋田県",39.7186,140.1024,"https://www.police.pref.akita.lg.jp/"},
  {"06","Yamagata","山形県",38.2403,140.3633,"https://www.pref.yamagata.jp/police/"},
  {"07","Fukushima","福島県",37.7503,140.4675,"https://www.police.pref.fukushima.jp/"},
  {"08","Ibaraki","茨城県",36.3658,140.4711,"https://www.pref.ibaraki.jp/keisatsu/"},
  {"09","Tochigi","栃木県",36.5658,139.8836,"https://www.pref.tochigi.lg.jp/keisatu/"},
  {"10","Gunma","群馬県",36.3911,139.0608,"https://www.police.pref.gunma.jp/"},
  {"11","Saitama","埼玉県",35.8617,139.6455,"https://www.police.pref.saitama.lg.jp/"},
  {"12","Chiba","千葉県",35.6083,140.1233,"https://www.police.pref.chiba.jp/"},
  {"13","Tokyo","東京都",35.6895,139.6917,"https://www.keishicho.metro.tokyo.lg.jp/"},
  {"14","Kanagawa","神奈川県",35.4437,139.6380,"https://www.police.pref.kanagawa.jp/"},
  {"15","Niigata","新潟県",37.9161,139.0364,"https://www.pref.niigata.lg.jp/site/kenkei/"},
  {"16","Toyama","富山県",36.6953,137.2113,"https://www.pref.toyama.jp/3500/kurashi/anzen/anzen/seian/seian/index.html"},
  {"17","Ishikawa","石川県",36.5946,136.6256,"https://www.pref.ishikawa.lg.jp/police/"},
  {"18","Fukui","福井県",36.0652,136.2216,"https://www.pref.fukui.lg.jp/doc/kenkei/"},
  {"19","Yamanashi","山梨県",35.6642,138.5683,"https://www.pref.yamanashi.jp/police/"},
  {"20","Nagano","長野県",36.6489,138.1944,"https://www.pref.nagano.lg.jp/police/"},
  {"21","Gifu","岐阜県",35.4233,136.7606,"https://www.pref.gifu.lg.jp/keisatsu/"},
  {"22","Shizuoka","静岡県",34.9756,138.3828,"https://www.pref.shizuoka.jp/police/"},
  {"23","Aichi","愛知県",35.1814,136.9069,"https://www.pref.aichi.jp/police/"},
  {"24","Mie","三重県",34.7184,136.5067,"https://www.police.pref.mie.jp/"},
  {"25","Shiga","滋賀県",35.0044,135.8686,"https://www.pref.shiga.lg.jp/police/"},
  {"26","Kyoto","京都府",35.0116,135.7681,"https://www.pref.kyoto.jp/fukei/"},
  {"27","Osaka","大阪府",34.6864,135.5197,"https://www.police.pref.osaka.lg.jp/"},
  {"28","Hyogo","兵庫県",34.6913,135.1830,"https://www.police.pref.hyogo.lg.jp/"},
  {"29","Nara","奈良県",34.6850,135.8048,"https://www.police.pref.nara.jp/"},
  {"30","Wakayama","和歌山県",34.2261,135.1675,"https://www.police.pref.wakayama.lg.jp/"},
  {"31","Tottori","鳥取県",35.5039,134.2378,"https://www.pref.tottori.lg.jp/keisatsu/"},
  {"32","Shimane","島根県",35.4722,133.0506,"https://www.pref.shimane.lg.jp/police/"},
  {"33","Okayama","岡山県",34.6628,133.9197,"https://www.pref.okayama.jp/site/kenkei/"},
  {"34","Hiroshima","広島県",34.3853,132.4553,"https://www.pref.hiroshima.lg.jp/site/police/"},
  {"35","Yamaguchi","山口県",34.1856,131.4714,"https://www.police.pref.yamaguchi.jp/"},
  {"36","Tokushima","徳島県",34.0658,134.5594,"https://www.pref.tokushima.lg.jp/site/police/"},
  {"37","Kagawa","香川県",34.3401,134.0434,"https://www.pref.kagawa.lg.jp/police/"},
  {"38","Ehime","愛媛県",33.8392,132.7656,"https://www.police.pref.ehime.jp/"},
  {"39","Kochi","高知県",33.5594,133.5311,"https://www.police.pref.kochi.lg.jp/"},
  {"40","Fukuoka","福岡県",33.5904,130.4017,"https://www.police.pref.fukuoka.jp/"},
  {"41","Saga","佐賀県",33.2494,130.2989,"https://www.police.pref.saga.jp/"},
  {"42","Nagasaki","長崎県",32.7503,129.8775,"https://www.police.pref.nagasaki.jp/"},
  {"43","Kumamoto","熊本県",32.8019,130.7256,"https://www.police.pref.kumamoto.jp/"},
  {"44","Oita","大分県",33.2381,131.6126,"https://www.police.pref.oita.jp/"},
  {"45","Miyazaki","宮崎県",31.9111,131.4239,"https://www.pref.miyazaki.lg.jp/police/"},
  {"46","Kagoshima","鹿児島県",31.5963,130.5571,"https://www.pref.kagoshima.jp/police/"},
  {"47","Okinawa","沖縄県",26.2125,127.6809,"https://www.police.pref.okinawa.jp/"},
};
#define NPREF ((int)(sizeof(PREFS)/sizeof(PREFS[0])))

/* env var name for the four prefectures with a CSV adapter, else NULL. */
static const char *adapter_env(const char *code) {
  if (!strcmp(code,"13")) return "TOKYO_MPD_CRIME_CSV_URL";
  if (!strcmp(code,"14")) return "KANAGAWA_POLICE_CSV_URL";
  if (!strcmp(code,"27")) return "OSAKA_POLICE_CSV_URL";
  if (!strcmp(code,"23")) return "AICHI_POLICE_CSV_URL";
  return NULL;
}

/* {year_month,count} adapter. code 13 also accepts r.month/r.total; all four
 * accept year_month|年月 + count|件数 (13 also total). parseInt(count,10);
 * needs ym truthy AND finite count. ym sliced to first 7 chars. */
typedef struct { char ym[16]; long count; } crow_t;

static crow_t *parse_adapter(const char *code, const char *text, size_t *out_n) {
  *out_n = 0;
  cJSON *rows = csv_parse(text, 1);             /* headers=true → objects */
  int nr = cJSON_GetArraySize(rows);
  crow_t *out = malloc((nr ? nr : 1) * sizeof(crow_t));
  size_t n = 0;
  int isTokyo = !strcmp(code, "13");
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const char *ym = jo_str(r, "year_month");
    if (!ym || !*ym) ym = jo_str(r, "年月");
    if ((!ym || !*ym) && isTokyo) ym = jo_str(r, "month");
    const char *cs = jo_str(r, "count");
    if (!cs || !*cs) cs = jo_str(r, "件数");
    if ((!cs || !*cs) && isTokyo) cs = jo_str(r, "total");
    if (!ym || !*ym) continue;
    if (!cs || !*cs) continue;
    char *e; long cnt = strtol(cs, &e, 10);
    if (e == cs) continue;                      /* !Number.isFinite */
    crow_t *o = &out[n++];
    snprintf(o->ym, sizeof o->ym, "%.7s", ym);  /* String(ym).slice(0,7) */
    o->count = cnt;
  }
  cJSON_Delete(rows);
  *out_n = n;
  return out;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char now[32]; probe_iso_now(now, sizeof now);
  cJSON *features = cJSON_CreateArray();
  int items = 0;

  for (int pi = 0; pi < NPREF; pi++) {
    const pref_t *p = &PREFS[pi];

    /* probeReachability: HEAD ok → true; else small GET, true if body>200;
       JS returns null only when url falsy (never here). */
    int reachable;
    if (probe_head(ctx->http, p->portal)) {
      reachable = 1;
    } else {
      char *t = feed_get_text(ctx->http, p->portal, 6000);
      reachable = (t && strlen(t) > 200) ? 1 : 0;
      free(t);
    }

    /* adapter rows */
    size_t rn = 0; crow_t *rws = NULL;
    const char *env = adapter_env(p->code);
    if (env) {
      const char *url = getenv(env);
      if (url && *url) {
        char *txt = feed_get_text(ctx->http, url, TIMEOUT_MS);
        if (txt) { rws = parse_adapter(p->code, txt, &rn); free(txt); }
      }
    }

    for (size_t k = 0; k < rn; k++) {
      cJSON *f = gj_point_feature(p->lon, p->lat);
      cJSON *pr = cJSON_CreateObject();         /* EXACT JS key order */
      char idb[48];
      snprintf(idb, sizeof idb, "PREFCRIME_%s_%s", p->code, rws[k].ym);
      cJSON_AddStringToObject(pr, "id", idb);
      cJSON_AddStringToObject(pr, "prefecture_code", p->code);
      cJSON_AddStringToObject(pr, "prefecture_ja", p->ja);
      cJSON_AddStringToObject(pr, "prefecture_en", p->en);
      cJSON_AddStringToObject(pr, "year_month", rws[k].ym);
      cJSON_AddNumberToObject(pr, "count", (double)rws[k].count);
      cJSON_AddStringToObject(pr, "portal_url", p->portal);
      cJSON_AddStringToObject(pr, "source", "pref-police-crime");
      cJSON_AddItemToObject(f, "properties", pr);
      cJSON_AddItemToArray(features, f);
    }

    /* directory intel item — ALWAYS (uid pref-police-crime|<code>) */
    char title[96], summary[96];
    snprintf(title, sizeof title, "%s 警察 犯罪統計ポータル", p->ja);
    snprintf(summary, sizeof summary,
      "%s prefectural police public crime statistics portal", p->en);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("crime"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("statistics"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("pref-police"));
    cJSON_AddItemToArray(tags, cJSON_CreateString(reachable ? "reachable" : "unreachable"));
    cJSON_AddItemToArray(tags, cJSON_CreateString(rn > 0 ? "has-monthly-data" : "directory-only"));
    char *tj = cJSON_PrintUnformatted(tags);

    cJSON *pr = cJSON_CreateObject();           /* EXACT JS key order */
    cJSON_AddStringToObject(pr, "prefecture_code", p->code);
    cJSON_AddStringToObject(pr, "prefecture_ja", p->ja);
    cJSON_AddStringToObject(pr, "prefecture_en", p->en);
    cJSON_AddStringToObject(pr, "portal_url", p->portal);
    cJSON_AddBoolToObject(pr, "reachable", reachable);
    cJSON_AddItemToObject(pr, "latest_year_month",
      rn > 0 ? cJSON_CreateString(rws[rn-1].ym) : cJSON_CreateNull());
    cJSON_AddNumberToObject(pr, "rows_available", (double)rn);
    char *pj = cJSON_PrintUnformatted(pr);

    intel_item it = {0};
    it.remote_key      = p->code;               /* → uid pref-police-crime|<code> */
    it.title           = title;
    it.summary         = summary;
    it.link            = p->portal;
    it.lang            = "ja";
    it.published_at    = now;
    it.tags_json       = tj;
    it.properties_json = pj;
    if (sink->emit(sink, &it) >= 0) items++;

    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(pr);
    free(rws);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[pref-police-crime] emitted %d features + %d directory\n", n, items);
  return 0;
}

static const source_def pref_police_crime_def = {
  .id = "pref-police-crime", .collector = "crime",
  .name = "Prefectural Police Crime", .name_ja = "都道府県警察 犯罪統計",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(pref_police_crime_def)
