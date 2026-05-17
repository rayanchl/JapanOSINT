/* collectors/satellite/sources/alos_palsar.c
 * Port of server/src/collectors/alosPalsar.js.
 * Keyless ASF DAAC SearchAPI (platform=ALOS, JP bbox, jsonlite) → one intel
 * item per SAR scene. Optional JAXA_GPORTAL_TOKEN (note only — ASF path is
 * always attempted, keyless). Non-spatial scene catalog (has_geo=0).
 * Honest empty on fetch failure. */
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
    "https://api.daac.asf.alaska.edu/services/search/param"
    "?platform=ALOS&bbox=122.0,24.0,146.0,46.0"
    "&maxResults=50&output=jsonlite";
  cJSON *data = feed_get_json(ctx->http, url, 20000);

  cJSON *rows = NULL;
  if (data) {
    cJSON *r = cJSON_GetObjectItem(data, "results");
    if (r && cJSON_IsArray(r)) rows = r;
    else if (cJSON_IsArray(data)) rows = data;
  }
  if (!rows || cJSON_GetArraySize(rows) == 0) {
    if (data) cJSON_Delete(data);
    fprintf(stderr, "[alos-palsar] unavailable (ASF DAAC no results)\n");
    return -1;
  }

  int n = 0, i = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    if (i >= 50) break;
    char sidbuf[32];
    const char *sid = sstr(r, "granuleName");
    if (!sid) sid = sstr(r, "sceneName");
    if (!sid) sid = sstr(r, "productID");
    if (!sid) { snprintf(sidbuf, sizeof sidbuf, "scene-%d", i); sid = sidbuf; }

    const char *acquired = sstr(r, "startTime");
    if (!acquired) acquired = sstr(r, "sceneDate");
    if (!acquired) acquired = sstr(r, "processingDate");
    const char *dl = sstr(r, "url");

    char title[128], summary[256], bodytxt[512];
    snprintf(title, sizeof title, "ALOS PALSAR scene %s", sid);
    snprintf(summary, sizeof summary,
      "JAXA ALOS PALSAR SAR scene acquired %s over Japan.",
      acquired ? acquired : "unknown");
    snprintf(bodytxt, sizeof bodytxt,
      "ALOS PALSAR L-band SAR scene %s (ASF DAAC archive) intersecting the "
      "Japan AOI. Acquired %s.%s",
      sid, acquired ? acquired : "unknown",
      has_gp ? " JAXA G-Portal token present (higher-tier products available "
               "via G-Portal)." : "");

    cJSON *p = cJSON_CreateObject();
    cJSON *jb = cJSON_CreateArray();
    cJSON_AddItemToArray(jb, cJSON_CreateNumber(122.0));
    cJSON_AddItemToArray(jb, cJSON_CreateNumber(24.0));
    cJSON_AddItemToArray(jb, cJSON_CreateNumber(146.0));
    cJSON_AddItemToArray(jb, cJSON_CreateNumber(46.0));
    cJSON_AddItemToObject(p, "bbox", jb);
    if (acquired) cJSON_AddStringToObject(p, "acquired", acquired);
    else cJSON_AddNullToObject(p, "acquired");
    cJSON_AddStringToObject(p, "sensor", "PALSAR");
    cJSON_AddStringToObject(p, "platform", "ALOS");
    cJSON_AddStringToObject(p, "scene_id", sid);
    const char *bm = sstr(r, "beamMode");
    if (bm) cJSON_AddStringToObject(p, "beam_mode", bm);
    else cJSON_AddNullToObject(p, "beam_mode");
    const char *pol = sstr(r, "polarization");
    if (pol) cJSON_AddStringToObject(p, "polarization", pol);
    else cJSON_AddNullToObject(p, "polarization");
    if (dl) cJSON_AddStringToObject(p, "download_url", dl);
    else cJSON_AddNullToObject(p, "download_url");
    char *pj = cJSON_PrintUnformatted(p);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("satellite"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("alos"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("palsar"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("sar"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("jaxa"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("raster"));
    char *tj = cJSON_PrintUnformatted(tags);

    intel_item it = {0};
    it.remote_key = sid;             /* uid alos-palsar|<sid> */
    it.title = title;
    it.summary = summary;
    it.body = bodytxt;
    it.link = dl ? dl : "https://search.asf.alaska.edu/";
    it.published_at = acquired;
    it.record_type = "alos-palsar";
    it.has_geo = 0;
    it.properties_json = pj;
    it.tags_json = tj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags);
    i++;
  }
  cJSON_Delete(data);
  fprintf(stderr, "[alos-palsar] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def alos_palsar_def = {
  .id = "alos-palsar", .collector = "satellite",
  .name = "ALOS/PALSAR SAR", .name_ja = "ALOS/PALSAR 合成開口レーダー",
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(alos_palsar_def)
