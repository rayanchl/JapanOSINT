/* collectors/statistics/sources/resas_municipality.c — port of
 * server/src/collectors/resasMunicipality.js. Live: RESAS opendata
 * /api/v1/cities?prefCode=N iterated over prefCodes 1..47, gated on
 * RESAS_API_KEY sent as the X-API-KEY header (feed_get_json_h). Honest
 * empty without the key; per-pref fetch failures are skipped (Node:
 * result null → continue). One intel_item per city. uid key mirrors
 * JS intelUid(SOURCE_ID, `${pref}-${cityCode}`). */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE "https://opendata.resas-portal.go.jp/api/v1"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("RESAS_API_KEY");
  if (!key || !*key) { fprintf(stderr, "[resas-municipality] gated (RESAS_API_KEY)\n"); return 0; }

  char authhdr[256];
  snprintf(authhdr, sizeof authhdr, "X-API-KEY: %s", key);
  const char *hdrs[] = { authhdr, NULL };

  int n = 0, fetched = 0;
  for (int pref = 1; pref <= 47; pref++) {
    char url[256];
    snprintf(url, sizeof url, "%s/cities?prefCode=%d", BASE, pref);
    cJSON *root = feed_get_json_h(ctx->http, url, hdrs, 12000);
    if (!root) continue;
    fetched++;
    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!result || !cJSON_IsArray(result)) { cJSON_Delete(root); continue; }

    cJSON *c;
    cJSON_ArrayForEach(c, result) {
      const char *cityCode = jo_vstr(cJSON_GetObjectItem(c, "cityCode"));
      const char *cityName = jo_vstr(cJSON_GetObjectItem(c, "cityName"));
      cJSON *bigFlag = cJSON_GetObjectItem(c, "bigCityFlag");
      const char *bigStr = jo_vstr(bigFlag);

      char rk[64], title[160], summary[160], body[256];
      snprintf(rk, sizeof rk, "%d-%s", pref, cityCode ? cityCode : "null");
      snprintf(title, sizeof title, "%s (pref %d)",
               cityName ? cityName : "municipality", pref);
      snprintf(summary, sizeof summary, "cityCode=%s bigCityFlag=%s",
               cityCode ? cityCode : "null", bigStr ? bigStr : "-");
      snprintf(body, sizeof body,
               "RESAS municipality: prefCode=%d, cityCode=%s, name=%s",
               pref, cityCode ? cityCode : "null",
               cityName ? cityName : "null");

      cJSON *p = cJSON_CreateObject();
      cJSON_AddNumberToObject(p, "prefCode", pref);
      if (cityCode) cJSON_AddStringToObject(p, "cityCode", cityCode);
      else cJSON_AddNullToObject(p, "cityCode");
      if (cityName) cJSON_AddStringToObject(p, "cityName", cityName);
      else cJSON_AddNullToObject(p, "cityName");
      if (bigFlag && !cJSON_IsNull(bigFlag))
        cJSON_AddItemToObject(p, "bigCityFlag", cJSON_Duplicate(bigFlag, 1));
      else cJSON_AddNullToObject(p, "bigCityFlag");
      char *pj = cJSON_PrintUnformatted(p);

      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("statistics"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("population"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("municipality"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("resas"));
      char *tj = cJSON_PrintUnformatted(tags);

      intel_item it = {0};
      it.remote_key      = rk;
      it.title           = title;
      it.summary         = summary;
      it.body            = body;
      it.link            = "https://opendata.resas-portal.go.jp/";
      it.lang            = "ja";
      it.record_type     = "resas-municipality";
      it.properties_json = pj;
      it.tags_json       = tj;
      if (sink->emit(sink, &it) >= 0) n++;

      free(pj); free(tj);
      cJSON_Delete(p); cJSON_Delete(tags);
    }
    cJSON_Delete(root);
  }
  fprintf(stderr, "[resas-municipality] emitted %d (%d/47 prefectures fetched)\n",
          n, fetched);
  /* STATUS code, not a row count: a prefecture with no new cities is an honest
   * empty. Only a total fetch failure is a real error. */
  return fetched > 0 ? 0 : -1;
}

static const source_def resas_municipality_def = {
  .id = "resas-municipality", .collector = "statistics",
  .name = "RESAS Municipal Comparison", .name_ja = "RESAS 自治体比較",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(resas_municipality_def)
