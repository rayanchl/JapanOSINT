/* collectors/satellite/sources/himawari_realtime.c
 * Port of server/src/collectors/himawariRealtime.js.
 * Keyless NICT Himawari latest-frame pointer (latest.json -> {date}) → ONE
 * intel item describing the latest full-disk frame + preview/tile URLs.
 * Non-spatial single raster frame (has_geo=0). Honest empty if pointer
 * unreachable. */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LATEST "https://himawari.asia/img/D531106/latest.json"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *latest = feed_get_json(ctx->http, LATEST, 10000);
  cJSON *dv = latest ? cJSON_GetObjectItem(latest, "date") : NULL;
  if (!dv || !cJSON_IsString(dv) || !dv->valuestring[0]) {
    if (latest) cJSON_Delete(latest);
    fprintf(stderr, "[himawari-realtime] unavailable (no latest pointer)\n");
    return -1;
  }
  const char *date = dv->valuestring;          /* "YYYY-MM-DD HH:MM:SS" */
  if (strlen(date) < 16) {
    cJSON_Delete(latest);
    fprintf(stderr, "[himawari-realtime] unavailable (bad date)\n");
    return -1;
  }

  /* iso = new Date(date.replace(' ','T')+'Z') -> ISO, else now */
  char iso[32];
  {
    struct tm tmv; memset(&tmv, 0, sizeof tmv);
    if (sscanf(date, "%4d-%2d-%2d %2d:%2d:%2d",
               &tmv.tm_year, &tmv.tm_mon, &tmv.tm_mday,
               &tmv.tm_hour, &tmv.tm_min, &tmv.tm_sec) == 6) {
      snprintf(iso, sizeof iso, "%04d-%02d-%02dT%02d:%02d:%02dZ",
        tmv.tm_year, tmv.tm_mon, tmv.tm_mday,
        tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
    } else {
      time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
      strftime(iso, sizeof iso, "%Y-%m-%dT%H:%M:%SZ", &g);
    }
  }

  char y[5], mo[3], da[3], hh[3], mm[3];
  memcpy(y, date, 4); y[4] = '\0';
  memcpy(mo, date + 5, 2); mo[2] = '\0';
  memcpy(da, date + 8, 2); da[2] = '\0';
  memcpy(hh, date + 11, 2); hh[2] = '\0';
  memcpy(mm, date + 14, 2); mm[2] = '\0';

  char preview[256], tile[256];
  snprintf(preview, sizeof preview,
    "https://himawari.asia/img/D531106/8d/%s/%s/%s/%s%s00_3_3.png",
    y, mo, da, hh, mm);
  snprintf(tile, sizeof tile,
    "https://himawari.asia/img/D531106/8d/%s/%s/%s/%s%s00_{x}_{y}.png",
    y, mo, da, hh, mm);

  char title[96], summary[256], bodytxt[512];
  snprintf(title, sizeof title, "Himawari-9 full-disk frame %s UTC", date);
  snprintf(summary, sizeof summary,
    "Latest NICT Himawari-9 AHI full-disk true-color frame covering Japan, "
    "acquired %s UTC.", date);
  snprintf(bodytxt, sizeof bodytxt,
    "Himawari-9 (AHI sensor) geostationary full-disk frame published by "
    "NICT real-time web. Latest frame timestamp %s UTC. Covers Japan and "
    "the western Pacific at 10-minute cadence.", date);

  cJSON *p = cJSON_CreateObject();
  cJSON *bb = cJSON_CreateArray();
  cJSON_AddItemToArray(bb, cJSON_CreateNumber(85.0));
  cJSON_AddItemToArray(bb, cJSON_CreateNumber(-60.0));
  cJSON_AddItemToArray(bb, cJSON_CreateNumber(205.0));
  cJSON_AddItemToArray(bb, cJSON_CreateNumber(60.0));
  cJSON_AddItemToObject(p, "bbox", bb);
  cJSON_AddStringToObject(p, "acquired", iso);
  cJSON_AddStringToObject(p, "sensor", "AHI");
  cJSON_AddStringToObject(p, "platform", "Himawari-9");
  cJSON_AddStringToObject(p, "scene_id", date);
  cJSON_AddStringToObject(p, "preview_url", preview);
  cJSON_AddStringToObject(p, "tile_url", tile);
  cJSON_AddNumberToObject(p, "cadence_minutes", 10);
  char *pj = cJSON_PrintUnformatted(p);

  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("satellite"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("himawari"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("nict"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("geostationary"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("real-time"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("raster"));
  char *tj = cJSON_PrintUnformatted(tags);

  intel_item it = {0};
  it.remote_key = date;              /* uid himawari-realtime|<date> */
  it.title = title;
  it.summary = summary;
  it.body = bodytxt;
  it.link = "https://himawari.asia/";
  it.published_at = iso;
  it.record_type = "himawari-realtime";
  it.has_geo = 0;
  it.properties_json = pj;
  it.tags_json = tj;
  int rc = sink->emit(sink, &it);

  free(pj); free(tj);
  cJSON_Delete(p); cJSON_Delete(tags);
  cJSON_Delete(latest);
  fprintf(stderr, "[himawari-realtime] emitted %d\n", rc >= 0 ? 1 : 0);
  return rc >= 0 ? 0 : -1;
}

static const source_def himawari_realtime_def = {
  .id = "himawari-realtime", .collector = "satellite",
  .name = "Himawari-9 Realtime", .name_ja = "ひまわり9号 リアルタイム",
  .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(himawari_realtime_def)
