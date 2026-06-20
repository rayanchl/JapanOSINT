/* collectors/economy/sources/tochi_info.c
 * Port of server/src/collectors/tochiInfo.js — MLIT 土地総合情報システム
 * key-free webland JSON API (TradeListSearch). Pulls the latest completed
 * quarter (now - 6 months) for all 47 prefectures. data.status==='OK' &&
 * Array.isArray(data.data) gate. Honest empty on failure — never fabricated.
 * Non-spatial: emit intel_item (has_geo=0).
 * uid = tochi-info|<area>-<year>Q<quarter>-<i> (mirrors intelUid). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define API_URL "https://www.land.mlit.go.jp/webland/api/TradeListSearch"

static void latest_yq(int *year, int *quarter) {
  time_t t = time(NULL);
  struct tm tm; localtime_r(&t, &tm);
  int m = tm.tm_mon - 6;
  int y = tm.tm_year + 1900;
  while (m < 0) { m += 12; y -= 1; }
  *year = y;
  *quarter = m / 3 + 1;
}

static cJSON *prop_or_null(cJSON *r, const char *k) {
  cJSON *v = cJSON_GetObjectItem(r, k);
  if (v && !cJSON_IsNull(v)) return cJSON_Duplicate(v, 1);
  return cJSON_CreateNull();
}

static const char *str_or(cJSON *r, const char *k, const char *def) {
  cJSON *v = cJSON_GetObjectItem(r, k);
  if (v && cJSON_IsString(v) && v->valuestring[0]) return v->valuestring;
  return def;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int year, quarter;
  latest_yq(&year, &quarter);

  char now[32];
  { time_t t = time(NULL); struct tm tm; gmtime_r(&t, &tm);
    strftime(now, sizeof now, "%Y-%m-%dT%H:%M:%S.000Z", &tm); }

  int n = 0;
  for (int pref = 1; pref <= 47; pref++) {
    char area[3]; snprintf(area, sizeof area, "%02d", pref);
    char url[256];
    /* from=<year><quarter>&to=<year><quarter>&area=<area> (JS template) */
    snprintf(url, sizeof url, "%s?from=%d%d&to=%d%d&area=%s",
             API_URL, year, quarter, year, quarter, area);
    cJSON *data = feed_get_json(ctx->http, url, 15000);
    if (!data) continue;
    cJSON *status = cJSON_GetObjectItem(data, "status");
    cJSON *rows   = cJSON_GetObjectItem(data, "data");
    int ok = status && cJSON_IsString(status) &&
             strcmp(status->valuestring, "OK") == 0 &&
             rows && cJSON_IsArray(rows);
    if (!ok) { cJSON_Delete(data); continue; }

    int i = 0;
    cJSON *r;
    cJSON_ArrayForEach(r, rows) {
      char uidkey[48];
      snprintf(uidkey, sizeof uidkey, "%s-%dQ%d-%d", area, year, quarter, i);

      const char *muni = str_or(r, "Municipality", NULL);
      const char *prefn = str_or(r, "Prefecture", NULL);
      const char *typ = str_or(r, "Type", "land transaction");
      const char *tp = str_or(r, "TradePrice", "?");
      const char *dn = str_or(r, "DistrictName", "");
      const char *ls = str_or(r, "LandShape", "");

      char title[256];
      snprintf(title, sizeof title, "%s — %s",
               muni ? muni : (prefn ? prefn : area), typ);
      char summary[320];
      snprintf(summary, sizeof summary, "%s JPY · %s · %s", tp, dn, ls);

      char *rawj = cJSON_PrintUnformatted(r);
      char *body = malloc((rawj ? strlen(rawj) : 2) + 64);
      sprintf(body, "MLIT webland TradeListSearch %dQ%d area=%s: %s",
              year, quarter, area, rawj ? rawj : "{}");

      cJSON *p = cJSON_CreateObject();   /* EXACT JS key order */
      cJSON_AddNumberToObject(p, "prefCode", pref);
      cJSON_AddNumberToObject(p, "year", year);
      cJSON_AddNumberToObject(p, "quarter", quarter);
      cJSON_AddItemToObject(p, "prefecture", prop_or_null(r, "Prefecture"));
      cJSON_AddItemToObject(p, "municipality", prop_or_null(r, "Municipality"));
      cJSON_AddItemToObject(p, "district", prop_or_null(r, "DistrictName"));
      cJSON_AddItemToObject(p, "type", prop_or_null(r, "Type"));
      cJSON_AddItemToObject(p, "land_use", prop_or_null(r, "Use"));
      cJSON_AddItemToObject(p, "land_shape", prop_or_null(r, "LandShape"));
      cJSON_AddItemToObject(p, "trade_price", prop_or_null(r, "TradePrice"));
      cJSON_AddItemToObject(p, "area_sqm", prop_or_null(r, "Area"));
      cJSON_AddItemToObject(p, "raw", cJSON_Duplicate(r, 1));
      char *pj = cJSON_PrintUnformatted(p);

      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("economy"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("land-use"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("landprice"));
      char *tj = cJSON_PrintUnformatted(tags);

      intel_item it = {0};
      it.remote_key   = uidkey;      /* → uid tochi-info|<area>-<y>Q<q>-<i> */
      it.title        = title;
      it.summary      = summary;
      it.body         = body;
      it.link         = "https://www.land.mlit.go.jp/webland/";
      it.lang         = "ja";
      it.published_at = now;
      it.record_type  = "tochi-info";
      it.tags_json    = tj;
      it.properties_json = pj;
      if (sink->emit(sink, &it) >= 0) n++;

      free(rawj); free(body); free(pj); free(tj);
      cJSON_Delete(p); cJSON_Delete(tags);
      i++;
    }
    cJSON_Delete(data);
  }

  fprintf(stderr, "[tochi-info] emitted %d (year=%d q=%d)\n", n, year, quarter);
  return n > 0 ? 0 : -1;
}

static const source_def tochi_info_def = {
  .id = "tochi-info", .collector = "economy",
  .name = "Tochi.info Land Use", .name_ja = "土地情報 土地利用",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(tochi_info_def)
