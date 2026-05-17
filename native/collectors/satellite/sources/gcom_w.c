/* collectors/satellite/sources/gcom_w.c
 * Port of server/src/collectors/gcomW.js.
 * Keyless NASA CMR granule search (keyword=AMSR2, JP bbox) → one intel item
 * per granule. Optional JAXA_GPORTAL_TOKEN (note only — CMR path is keyless).
 * Non-spatial swath catalog (has_geo=0). Honest empty on fetch failure. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *sstr(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *gportal = getenv("JAXA_GPORTAL_TOKEN");
  int has_gp = gportal && *gportal;

  const char *url =
    "https://cmr.earthdata.nasa.gov/search/granules.json"
    "?keyword=AMSR2&bounding_box=122.0,24.0,146.0,46.0"
    "&sort_key=-start_date&page_size=50";
  cJSON *data = feed_get_json(ctx->http, url, 20000);

  cJSON *feed = data ? cJSON_GetObjectItem(data, "feed") : NULL;
  cJSON *rows = feed ? cJSON_GetObjectItem(feed, "entry") : NULL;
  if (!rows || !cJSON_IsArray(rows) || cJSON_GetArraySize(rows) == 0) {
    if (data) cJSON_Delete(data);
    fprintf(stderr, "[gcom-w] unavailable (NASA CMR no granules)\n");
    return -1;
  }

  int n = 0, i = 0;
  cJSON *g;
  cJSON_ArrayForEach(g, rows) {
    if (i >= 50) break;
    char sidbuf[32];
    const char *sid = sstr(g, "producer_granule_id");
    if (!sid) sid = sstr(g, "title");
    if (!sid) sid = sstr(g, "id");
    if (!sid) { snprintf(sidbuf, sizeof sidbuf, "granule-%d", i); sid = sidbuf; }

    const char *acquired = sstr(g, "time_start");
    if (!acquired) acquired = sstr(g, "updated");
    const char *time_end = sstr(g, "time_end");
    const char *dataset = sstr(g, "dataset_id");

    const char *link0 = NULL;
    cJSON *links = cJSON_GetObjectItem(g, "links");
    if (links && cJSON_IsArray(links)) {
      cJSON *l0 = cJSON_GetArrayItem(links, 0);
      link0 = sstr(l0, "href");
    }

    char title[160], summary[256], bodytxt[512];
    snprintf(title, sizeof title, "GCOM-W AMSR2 granule %s", sid);
    snprintf(summary, sizeof summary,
      "JAXA GCOM-W AMSR2 microwave granule acquired %s over Japan.",
      acquired ? acquired : "unknown");
    snprintf(bodytxt, sizeof bodytxt,
      "GCOM-W \"SHIZUKU\" AMSR2 microwave radiometer granule %s (NASA CMR "
      "mirror) intersecting the Japan AOI. Acquired %s.%s",
      sid, acquired ? acquired : "unknown",
      has_gp ? " JAXA G-Portal token present (native products via "
               "G-Portal)." : "");

    cJSON *p = cJSON_CreateObject();
    cJSON *jb = cJSON_CreateArray();
    cJSON_AddItemToArray(jb, cJSON_CreateNumber(122.0));
    cJSON_AddItemToArray(jb, cJSON_CreateNumber(24.0));
    cJSON_AddItemToArray(jb, cJSON_CreateNumber(146.0));
    cJSON_AddItemToArray(jb, cJSON_CreateNumber(46.0));
    cJSON_AddItemToObject(p, "bbox", jb);
    if (acquired) cJSON_AddStringToObject(p, "acquired", acquired);
    else cJSON_AddNullToObject(p, "acquired");
    if (time_end) cJSON_AddStringToObject(p, "time_end", time_end);
    else cJSON_AddNullToObject(p, "time_end");
    cJSON_AddStringToObject(p, "sensor", "AMSR2");
    cJSON_AddStringToObject(p, "platform", "GCOM-W");
    cJSON_AddStringToObject(p, "scene_id", sid);
    if (dataset) cJSON_AddStringToObject(p, "dataset", dataset);
    else cJSON_AddNullToObject(p, "dataset");
    char *pj = cJSON_PrintUnformatted(p);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("satellite"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("gcom-w"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("amsr2"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("microwave"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("jaxa"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("raster"));
    char *tj = cJSON_PrintUnformatted(tags);

    intel_item it = {0};
    it.remote_key = sid;             /* uid gcom-w|<sid> */
    it.title = title;
    it.summary = summary;
    it.body = bodytxt;
    it.link = link0 ? link0 : "https://gportal.jaxa.jp/";
    it.published_at = acquired;
    it.record_type = "gcom-w";
    it.has_geo = 0;
    it.properties_json = pj;
    it.tags_json = tj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags);
    i++;
  }
  cJSON_Delete(data);
  fprintf(stderr, "[gcom-w] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def gcom_w_def = {
  .id = "gcom-w", .collector = "satellite",
  .name = "GCOM-W Water Cycle", .name_ja = "GCOM-W 水循環",
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(gcom_w_def)
