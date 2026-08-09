/* collectors/economy/sources/reinfolib.c
 * Port of server/src/collectors/reinfolib.js — MLIT 不動産情報ライブラリ
 * external API XIT001 (real-estate transaction prices). Keyed via
 * REINFOLIB_API_KEY sent as the Ocp-Apim-Subscription-Key header. Pulls the
 * latest fully-published quarter (now - 6 months) for all 47 prefectures.
 * Gated (no key) → emit nothing. Honest empty on upstream failure — never a
 * fabricated transaction. Non-spatial: emit intel_item (has_geo=0).
 * uid = reinfolib|<area>-<year>Q<quarter>-<i> (mirrors intelUid). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define API_URL "https://www.reinfolib.mlit.go.jp/ex-api/external/XIT001"

/* JS: d=new Date(); d.setMonth(d.getMonth()-6);
 *     year=d.getFullYear(); quarter=floor(d.getMonth()/3)+1 */
static void latest_yq(int *year, int *quarter) {
  time_t t = time(NULL);
  struct tm tm; localtime_r(&t, &tm);
  int m = tm.tm_mon - 6;            /* 0-based month, shifted back 6 */
  int y = tm.tm_year + 1900;
  while (m < 0) { m += 12; y -= 1; }
  *year = y;
  *quarter = m / 3 + 1;
}

/* r.X ?? null : returns string for any JSON scalar, else NULL. cJSON owns. */
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
  const char *key = getenv("REINFOLIB_API_KEY");
  if (!key || !*key) { fprintf(stderr, "[reinfolib] gated\n"); return 0; }

  int year, quarter;
  latest_yq(&year, &quarter);

  char hdr[256];
  snprintf(hdr, sizeof hdr, "Ocp-Apim-Subscription-Key: %s", key);
  const char *headers[] = { hdr, "Accept: application/json", NULL };

  char now[32];
  { time_t t = time(NULL); struct tm tm; gmtime_r(&t, &tm);
    strftime(now, sizeof now, "%Y-%m-%dT%H:%M:%S.000Z", &tm); }

  int n = 0, fetched = 0;
  for (int pref = 1; pref <= 47; pref++) {
    char area[3]; snprintf(area, sizeof area, "%02d", pref);
    char url[256];
    snprintf(url, sizeof url, "%s?year=%d&quarter=%d&area=%s",
             API_URL, year, quarter, area);
    cJSON *data = feed_get_json_h(ctx->http, url, headers, 15000);
    if (data) fetched++;
    cJSON *rows = data ? cJSON_GetObjectItem(data, "data") : NULL;
    if (!rows || !cJSON_IsArray(rows)) { if (data) cJSON_Delete(data); continue; }

    int i = 0;
    cJSON *r;
    cJSON_ArrayForEach(r, rows) {
      char uidkey[48];
      snprintf(uidkey, sizeof uidkey, "%s-%dQ%d-%d", area, year, quarter, i);

      const char *muni = str_or(r, "Municipality", NULL);
      const char *prefn = str_or(r, "Prefecture", NULL);
      const char *typ = str_or(r, "Type", "transaction");
      const char *tp = str_or(r, "TradePrice", "?");
      const char *dn = str_or(r, "DistrictName", "");

      char title[256];
      snprintf(title, sizeof title, "%s — %s",
               muni ? muni : (prefn ? prefn : area), typ);
      char summary[256];
      snprintf(summary, sizeof summary, "%s JPY · %s", tp, dn);

      char *rawj = cJSON_PrintUnformatted(r);
      char *body = malloc((rawj ? strlen(rawj) : 2) + 64);
      sprintf(body, "MLIT reinfolib XIT001 %dQ%d area=%s: %s",
              year, quarter, area, rawj ? rawj : "{}");

      cJSON *p = cJSON_CreateObject();   /* EXACT JS key order */
      cJSON_AddNumberToObject(p, "prefCode", pref);
      cJSON_AddNumberToObject(p, "year", year);
      cJSON_AddNumberToObject(p, "quarter", quarter);
      cJSON_AddItemToObject(p, "prefecture", prop_or_null(r, "Prefecture"));
      cJSON_AddItemToObject(p, "municipality", prop_or_null(r, "Municipality"));
      cJSON_AddItemToObject(p, "district", prop_or_null(r, "DistrictName"));
      cJSON_AddItemToObject(p, "type", prop_or_null(r, "Type"));
      cJSON_AddItemToObject(p, "trade_price", prop_or_null(r, "TradePrice"));
      cJSON_AddItemToObject(p, "area_sqm", prop_or_null(r, "Area"));
      cJSON_AddItemToObject(p, "raw", cJSON_Duplicate(r, 1));
      char *pj = cJSON_PrintUnformatted(p);

      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("economy"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("real-estate"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("landprice"));
      char *tj = cJSON_PrintUnformatted(tags);

      intel_item it = {0};
      it.remote_key   = uidkey;       /* → uid reinfolib|<area>-<y>Q<q>-<i> */
      it.title        = title;
      it.summary      = summary;
      it.body         = body;
      it.link         = "https://www.reinfolib.mlit.go.jp/";
      it.lang         = "ja";
      it.published_at = now;
      it.record_type  = "reinfolib";
      it.tags_json    = tj;
      it.properties_json = pj;
      if (sink->emit(sink, &it) >= 0) n++;

      free(rawj); free(body); free(pj); free(tj);
      cJSON_Delete(p); cJSON_Delete(tags);
      i++;
    }
    cJSON_Delete(data);
  }

  fprintf(stderr, "[reinfolib] emitted %d (year=%d q=%d, %d/47 areas fetched)\n",
          n, year, quarter, fetched);
  /* STATUS code, not a row count: an area with no transactions in the quarter is
   * an honest empty. Only a total fetch failure is a real error. */
  return fetched > 0 ? 0 : -1;
}

static const source_def reinfolib_def = {
  .id = "reinfolib", .collector = "economy",
  .name = "Real Estate Transaction Prices", .name_ja = "不動産取引価格情報",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(reinfolib_def)
