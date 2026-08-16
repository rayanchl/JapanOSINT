/* collectors/statistics/sources/resas_population.c — port of
 * server/src/collectors/resasPopulation.js. Live: RESAS opendata
 * /api/v1/population/composition/perYear?prefCode=N&cityCode=- iterated
 * over prefCodes 1..47, gated on RESAS_API_KEY sent as the X-API-KEY
 * header (feed_get_json_h). Honest empty without the key; per-pref
 * failures skipped (Node: result null / data not array → continue).
 * One intel_item per series. uid key mirrors JS
 * intelUid(SOURCE_ID, `${pref}-${label}`). */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RESAS_URL "https://opendata.resas-portal.go.jp/api/v1/population/composition/perYear"

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("RESAS_API_KEY");
  if (!key || !*key) { fprintf(stderr, "[resas-population] gated (RESAS_API_KEY)\n"); return 0; }

  char authhdr[256];
  snprintf(authhdr, sizeof authhdr, "X-API-KEY: %s", key);
  const char *hdrs[] = { authhdr, NULL };

  int n = 0, fetched = 0;
  for (int pref = 1; pref <= 47; pref++) {
    char url[256];
    snprintf(url, sizeof url, "%s?prefCode=%d&cityCode=-", RESAS_URL, pref);
    cJSON *root = feed_get_json_h(ctx->http, url, hdrs, 12000);
    if (!root) continue;
    fetched++;
    cJSON *result = cJSON_GetObjectItem(root, "result");
    cJSON *data = result ? cJSON_GetObjectItem(result, "data") : NULL;
    if (!data || !cJSON_IsArray(data)) { cJSON_Delete(root); continue; }
    cJSON *boundary = cJSON_GetObjectItem(result, "boundaryYear");

    cJSON *series;
    cJSON_ArrayForEach(series, data) {
      const char *label = jo_vstr(cJSON_GetObjectItem(series, "label"));
      if (!label) label = "population";
      cJSON *sdata = cJSON_GetObjectItem(series, "data");
      cJSON *latest = NULL;
      if (sdata && cJSON_IsArray(sdata) && cJSON_GetArraySize(sdata) > 0)
        latest = cJSON_GetArrayItem(sdata, cJSON_GetArraySize(sdata) - 1);

      char rk[160], title[160], summary[192], body[256];
      snprintf(rk, sizeof rk, "%d-%s", pref, label);
      snprintf(title, sizeof title, "RESAS population - pref %d (%s)",
               pref, label);
      if (latest) {
        cJSON *yr = cJSON_GetObjectItem(latest, "year");
        cJSON *vl = cJSON_GetObjectItem(latest, "value");
        char yrbuf[32] = "?", vlbuf[32] = "?";
        if (yr && cJSON_IsNumber(yr)) snprintf(yrbuf, sizeof yrbuf, "%g", yr->valuedouble);
        else if (jo_vstr(yr)) snprintf(yrbuf, sizeof yrbuf, "%s", yr->valuestring);
        if (vl && cJSON_IsNumber(vl)) snprintf(vlbuf, sizeof vlbuf, "%g", vl->valuedouble);
        else if (jo_vstr(vl)) snprintf(vlbuf, sizeof vlbuf, "%s", vl->valuestring);
        snprintf(summary, sizeof summary, "%s %s: %s", label, yrbuf, vlbuf);
      } else {
        snprintf(summary, sizeof summary, "%s", label);
      }
      char bybuf[32] = "?";
      if (boundary && cJSON_IsNumber(boundary))
        snprintf(bybuf, sizeof bybuf, "%g", boundary->valuedouble);
      else if (jo_vstr(boundary))
        snprintf(bybuf, sizeof bybuf, "%s", boundary->valuestring);
      snprintf(body, sizeof body,
               "RESAS population composition perYear, prefCode=%d, series=%s, boundaryYear=%s",
               pref, label, bybuf);

      cJSON *p = cJSON_CreateObject();
      cJSON_AddNumberToObject(p, "prefCode", pref);
      cJSON_AddStringToObject(p, "series", label);
      if (boundary && !cJSON_IsNull(boundary))
        cJSON_AddItemToObject(p, "boundaryYear", cJSON_Duplicate(boundary, 1));
      else cJSON_AddNullToObject(p, "boundaryYear");
      if (sdata && cJSON_IsArray(sdata))
        cJSON_AddItemToObject(p, "data", cJSON_Duplicate(sdata, 1));
      else cJSON_AddItemToObject(p, "data", cJSON_CreateArray());
      char *pj = cJSON_PrintUnformatted(p);

      cJSON *tags = cJSON_CreateArray();
      cJSON_AddItemToArray(tags, cJSON_CreateString("statistics"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("population"));
      cJSON_AddItemToArray(tags, cJSON_CreateString("resas"));
      char *tj = cJSON_PrintUnformatted(tags);

      intel_item it = {0};
      it.remote_key      = rk;
      it.title           = title;
      it.summary         = summary;
      it.body            = body;
      it.link            = "https://opendata.resas-portal.go.jp/";
      it.lang            = "ja";
      it.record_type     = "resas-population";
      it.properties_json = pj;
      it.tags_json       = tj;
      if (sink->emit(sink, &it) >= 0) n++;

      free(pj); free(tj);
      cJSON_Delete(p); cJSON_Delete(tags);
    }
    cJSON_Delete(root);
  }
  fprintf(stderr, "[resas-population] emitted %d (%d/47 prefectures fetched)\n",
          n, fetched);
  /* STATUS code, not a row count: a prefecture that returns no series is an
   * honest empty. Only a total fetch failure is a real error. */
  return fetched > 0 ? 0 : -1;
}

static const source_def resas_population_def = {
  .id = "resas-population", .collector = "statistics",
  .name = "RESAS Population by City", .name_ja = "RESAS 市町村別人口",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(resas_population_def)
