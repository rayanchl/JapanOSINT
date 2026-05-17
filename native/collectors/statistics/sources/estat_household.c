/* collectors/statistics/sources/estat_household.c — port of
 * server/src/collectors/estatHousehold.js. Live path: e-Stat REST 3.0
 * getStatsData, gated on ESTAT_APP_ID (appId in query string). Without the
 * app id, or on any upstream failure / empty VALUE array, emits nothing
 * (honest empty — Node returns an empty intel envelope). Non-spatial:
 * one intel_item per statistical VALUE. uid key mirrors the JS
 * `${area}-${cat}-${time}` template (always a non-empty string, so the
 * JS `|| i` index fallback is dead code and is not ported). */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_BASE "https://api.e-stat.go.jp/rest/3.0/app/json/getStatsData"
#define DEFAULT_STATS_DATA_ID "0003448237"

static const char *s_or_null(cJSON *v) {
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  (void)ctx;
  const char *appId = getenv("ESTAT_APP_ID");
  if (!appId || !*appId) { fprintf(stderr, "[estat-household] gated (ESTAT_APP_ID)\n"); return 0; }
  const char *sdi = getenv("ESTAT_HOUSEHOLD_STATS_DATA_ID");
  if (!sdi || !*sdi) sdi = DEFAULT_STATS_DATA_ID;

  char url[512];
  snprintf(url, sizeof url, "%s?appId=%s&statsDataId=%s&limit=2000",
           API_BASE, appId, sdi);
  cJSON *root = feed_get_json(ctx->http, url, 20000);
  if (!root) { fprintf(stderr, "[estat-household] fetch failed\n"); return -1; }

  cJSON *values = cJSON_GetObjectItem(root,
      "GET_STATS_DATA");
  values = values ? cJSON_GetObjectItem(values, "STATISTICAL_DATA") : NULL;
  values = values ? cJSON_GetObjectItem(values, "DATA_INF") : NULL;
  values = values ? cJSON_GetObjectItem(values, "VALUE") : NULL;
  if (!values || !cJSON_IsArray(values) || cJSON_GetArraySize(values) == 0) {
    cJSON_Delete(root);
    fprintf(stderr, "[estat-household] unavailable (no VALUE)\n");
    return -1;
  }

  int n = 0, i = 0;
  cJSON *v;
  cJSON_ArrayForEach(v, values) {
    if (i++ >= 2000) break;
    const char *area = s_or_null(cJSON_GetObjectItem(v, "@area"));
    const char *cat  = s_or_null(cJSON_GetObjectItem(v, "@cat01"));
    const char *time = s_or_null(cJSON_GetObjectItem(v, "@time"));
    const char *val  = s_or_null(cJSON_GetObjectItem(v, "$"));

    char rk[160], title[160], summary[160], body[320];
    snprintf(rk, sizeof rk, "%s-%s-%s",
             area ? area : "null", cat ? cat : "null", time ? time : "null");
    snprintf(title, sizeof title, "Household stat - area %s (%s)",
             area ? area : "?", time ? time : "?");
    snprintf(summary, sizeof summary, "value=%s cat=%s",
             val ? val : "null", cat ? cat : "-");
    snprintf(body, sizeof body,
             "e-Stat household composition table %s: area=%s, category=%s, time=%s, value=%s",
             sdi, area ? area : "null", cat ? cat : "null",
             time ? time : "null", val ? val : "null");

    cJSON *p = cJSON_CreateObject();
    if (area) cJSON_AddStringToObject(p, "area", area); else cJSON_AddNullToObject(p, "area");
    if (cat)  cJSON_AddStringToObject(p, "cat01", cat); else cJSON_AddNullToObject(p, "cat01");
    if (time) cJSON_AddStringToObject(p, "time", time); else cJSON_AddNullToObject(p, "time");
    if (val)  cJSON_AddStringToObject(p, "value", val); else cJSON_AddNullToObject(p, "value");
    cJSON_AddStringToObject(p, "statsDataId", sdi);
    char *pj = cJSON_PrintUnformatted(p);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("statistics"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("population"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("household"));
    char *tj = cJSON_PrintUnformatted(tags);

    intel_item it = {0};
    it.remote_key      = rk;
    it.title           = title;
    it.summary         = summary;
    it.body            = body;
    it.link            = "https://www.e-stat.go.jp/";
    it.lang            = "ja";
    it.record_type     = "estat-household";
    it.properties_json = pj;
    it.tags_json       = tj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[estat-household] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def estat_household_def = {
  .id = "estat-household", .collector = "statistics",
  .name = "e-Stat Household Data", .name_ja = "e-Stat 世帯データ",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(estat_household_def)
