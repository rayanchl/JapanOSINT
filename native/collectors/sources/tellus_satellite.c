/* collectors/satellite/sources/tellus_satellite.c
 * Port of server/src/collectors/tellusSatellite.js.
 * Tellus Traveler data-search API (POST, Bearer TELLUS_TOKEN) → one intel
 * item per scene. Non-spatial scene catalog (has_geo=0). Gated on
 * TELLUS_TOKEN (no key-free path). Honest empty on fetch failure. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *sstr(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *token = getenv("TELLUS_TOKEN");
  if (!token || !*token) {
    fprintf(stderr, "[tellus-satellite] gated (no TELLUS_TOKEN)\n");
    return 0;
  }

  /* JP_BBOX = [122,24,146,46] -> intersects polygon */
  const char *body =
    "{\"intersects\":{\"type\":\"Polygon\",\"coordinates\":"
    "[[[122,24],[146,24],[146,46],[122,46],[122,24]]]},"
    "\"limit\":50,\"sortby\":[{\"field\":\"properties.start_datetime\","
    "\"direction\":\"desc\"}]}";

  char auth[600];
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", token);
  const char *hdrs[] = { "Content-Type: application/json", auth, NULL };

  cJSON *data = feed_post_json(ctx->http,
    "https://www.tellusxdp.com/api/traveler/v1/data-search/",
    body, hdrs, 20000);

  cJSON *feats = NULL;
  if (data) {
    cJSON *f;
    if ((f = cJSON_GetObjectItem(data, "features")) && cJSON_IsArray(f))
      feats = f;
    else if ((f = cJSON_GetObjectItem(data, "results")) && cJSON_IsArray(f))
      feats = f;
    else if ((f = cJSON_GetObjectItem(data, "items")) && cJSON_IsArray(f))
      feats = f;
  }
  if (!feats || cJSON_GetArraySize(feats) == 0) {
    if (data) cJSON_Delete(data);
    fprintf(stderr, "[tellus-satellite] unavailable (no scenes)\n");
    return -1;
  }

  int n = 0, i = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    if (i >= 50) break;
    cJSON *props = cJSON_GetObjectItem(f, "properties");
    if (!props || !cJSON_IsObject(props)) props = f;

    char sidbuf[32];
    const char *sid = sstr(f, "id");
    if (!sid) sid = sstr(props, "scene_id");
    if (!sid) sid = sstr(props, "id");
    if (!sid) { snprintf(sidbuf, sizeof sidbuf, "scene-%d", i); sid = sidbuf; }

    const char *acquired = sstr(props, "start_datetime");
    if (!acquired) acquired = sstr(props, "datetime");
    if (!acquired) acquired = sstr(props, "acquired");
    const char *sensor = sstr(props, "sensor");
    const char *platform = sstr(props, "platform");

    char title[128], summary[256], bodytxt[512];
    snprintf(title, sizeof title, "Tellus scene %s", sid);
    snprintf(summary, sizeof summary,
      "Tellus satellite scene acquired %s over Japan.",
      acquired ? acquired : "unknown");
    snprintf(bodytxt, sizeof bodytxt,
      "Tellus platform satellite scene %s intersecting the Japan AOI. "
      "Acquired %s. Sensor %s.",
      sid, acquired ? acquired : "unknown",
      sensor ? sensor : (platform ? platform : "unknown"));

    cJSON *p = cJSON_CreateObject();
    cJSON *bb = cJSON_GetObjectItem(f, "bbox");
    if (bb) cJSON_AddItemToObject(p, "bbox", cJSON_Duplicate(bb, 1));
    else {
      cJSON *jb = cJSON_CreateArray();
      cJSON_AddItemToArray(jb, cJSON_CreateNumber(122.0));
      cJSON_AddItemToArray(jb, cJSON_CreateNumber(24.0));
      cJSON_AddItemToArray(jb, cJSON_CreateNumber(146.0));
      cJSON_AddItemToArray(jb, cJSON_CreateNumber(46.0));
      cJSON_AddItemToObject(p, "bbox", jb);
    }
    cJSON *gm = cJSON_GetObjectItem(f, "geometry");
    cJSON_AddItemToObject(p, "geometry",
      gm ? cJSON_Duplicate(gm, 1) : cJSON_CreateNull());
    if (acquired) cJSON_AddStringToObject(p, "acquired", acquired);
    else cJSON_AddNullToObject(p, "acquired");
    if (sensor) cJSON_AddStringToObject(p, "sensor", sensor);
    else cJSON_AddNullToObject(p, "sensor");
    if (platform) cJSON_AddStringToObject(p, "platform", platform);
    else cJSON_AddNullToObject(p, "platform");
    cJSON_AddStringToObject(p, "scene_id", sid);
    char *pj = cJSON_PrintUnformatted(p);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("satellite"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("tellus"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("japan"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("raster"));
    char *tj = cJSON_PrintUnformatted(tags);

    intel_item it = {0};
    it.remote_key = sid;             /* uid tellus-satellite|<sid> */
    it.title = title;
    it.summary = summary;
    it.body = bodytxt;
    it.link = "https://www.tellusxdp.com/";
    it.published_at = acquired;
    it.record_type = "tellus-satellite";
    it.has_geo = 0;
    it.properties_json = pj;
    it.tags_json = tj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags);
    i++;
  }
  cJSON_Delete(data);
  fprintf(stderr, "[tellus-satellite] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def tellus_satellite_def = {
  .id = "tellus-satellite", .collector = "satellite",
  .name = "Tellus Satellite Imagery", .name_ja = "Tellus 衛星画像",
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(tellus_satellite_def)
