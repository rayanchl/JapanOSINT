/* collectors/environment/sources/jma_himawari.c
 * INTEL source — port of server/src/collectors/jmaHimawari.js.
 * targetTimes_fd.json (array of {basetime,validtime} JST stamps); emit ONE
 * intel item describing the latest full-disk frame + tile template + viewer.
 * uid = jma-himawari|<validtime|latest>. Honest empty on failure. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TARGET_TIMES_FD "https://www.jma.go.jp/bosai/himawari/data/satimg/targetTimes_fd.json"
#define VIEWER "https://www.jma.go.jp/bosai/map.html#5/34.5/137/&elem=satellite&contents=himawari"

static void iso_now(char *out, size_t n) {
  time_t t = time(NULL); struct tm g; gmtime_r(&t, &g);
  strftime(out, n, "%Y-%m-%dT%H:%M:%S.000Z", &g);
}

/* "20260516060000" (JST) -> ISO 8601 UTC string (subtract 9h). */
static int jst_stamp_to_iso(const char *s, char *out, size_t n) {
  if (!s || strlen(s) != 14) return 0;
  struct tm tmv = {0};
  char buf[5];
  memcpy(buf, s, 4); buf[4] = 0; tmv.tm_year = atoi(buf) - 1900;
  memcpy(buf, s + 4, 2); buf[2] = 0; tmv.tm_mon = atoi(buf) - 1;
  memcpy(buf, s + 6, 2); buf[2] = 0; tmv.tm_mday = atoi(buf);
  memcpy(buf, s + 8, 2); buf[2] = 0; tmv.tm_hour = atoi(buf);
  memcpy(buf, s + 10, 2); buf[2] = 0; tmv.tm_min = atoi(buf);
  memcpy(buf, s + 12, 2); buf[2] = 0; tmv.tm_sec = atoi(buf);
  time_t base = timegm(&tmv);              /* interpret fields as UTC */
  if (base == (time_t)-1) return 0;
  base -= 9 * 3600;                        /* JST -> UTC */
  struct tm g; gmtime_r(&base, &g);
  strftime(out, n, "%Y-%m-%dT%H:%M:%S.000Z", &g);
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *times = feed_get_json(ctx->http, TARGET_TIMES_FD, 8000);
  if (!times || !cJSON_IsArray(times) || cJSON_GetArraySize(times) == 0) {
    if (times) cJSON_Delete(times);
    fprintf(stderr, "[jma-himawari] upstream unavailable\n");
    return -1;
  }
  cJSON *latest = cJSON_GetArrayItem(times, cJSON_GetArraySize(times) - 1);
  cJSON *bt = latest ? cJSON_GetObjectItem(latest, "basetime") : NULL;
  cJSON *vt = latest ? cJSON_GetObjectItem(latest, "validtime") : NULL;
  const char *basetime = (bt && cJSON_IsString(bt)) ? bt->valuestring : NULL;
  const char *validtime = (vt && cJSON_IsString(vt)) ? vt->valuestring : NULL;

  char observed[40]; int has_obs = jst_stamp_to_iso(validtime, observed, sizeof observed);
  char now[40]; iso_now(now, sizeof now);

  char tile[512] = {0};
  if (basetime && validtime)
    snprintf(tile, sizeof tile,
      "https://www.jma.go.jp/bosai/himawari/data/satimg/%s/fd/%s/B13/TBB/{z}/{x}/{y}.jpg",
      basetime, validtime);

  char title[160], body[512];
  snprintf(title, sizeof title, "Himawari-9 full-disk imagery%s%s",
           has_obs ? " \xE2\x80\x94 " : "", has_obs ? observed : "");
  snprintf(body, sizeof body,
    "Latest Himawari-9 full-disk frame basetime=%s validtime=%s. Imagery is "
    "raster tiles served by JMA bosai; open the interactive viewer for the "
    "live mosaic.", basetime ? basetime : "null", validtime ? validtime : "null");

  cJSON *p = cJSON_CreateObject();
  if (basetime) cJSON_AddStringToObject(p, "basetime", basetime);
  else cJSON_AddNullToObject(p, "basetime");
  if (validtime) cJSON_AddStringToObject(p, "validtime", validtime);
  else cJSON_AddNullToObject(p, "validtime");
  if (has_obs) cJSON_AddStringToObject(p, "observed_at", observed);
  else cJSON_AddNullToObject(p, "observed_at");
  if (tile[0]) cJSON_AddStringToObject(p, "tile_template", tile);
  else cJSON_AddNullToObject(p, "tile_template");
  cJSON_AddNumberToObject(p, "frames_available", cJSON_GetArraySize(times));
  char *pj = cJSON_PrintUnformatted(p);

  cJSON *tags = cJSON_CreateArray();
  cJSON_AddItemToArray(tags, cJSON_CreateString("satellite"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("himawari"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("jma"));
  cJSON_AddItemToArray(tags, cJSON_CreateString("imagery"));
  char *tj = cJSON_PrintUnformatted(tags);

  intel_item it = {0};
  it.remote_key = (validtime && *validtime) ? validtime : "latest";
  it.title = title;
  it.summary = "Latest available JMA Himawari-9 geostationary satellite full-disk frame";
  it.body = body;
  it.link = VIEWER;
  it.author = "\xE6\xB0\x97\xE8\xB1\xA1\xE5\xBA\x81 Japan Meteorological Agency";
  it.lang = "ja";
  it.published_at = has_obs ? observed : now;
  it.record_type = "article";
  it.properties_json = pj;
  it.tags_json = tj;
  int rc = sink->emit(sink, &it);

  free(pj); free(tj);
  cJSON_Delete(p); cJSON_Delete(tags); cJSON_Delete(times);
  fprintf(stderr, "[jma-himawari] emitted %d\n", rc >= 0 ? 1 : 0);
  return rc >= 0 ? 0 : -1;
}

static const source_def jma_himawari_def = {
  .id = "jma-himawari", .collector = "environment",
  .name = "JMA Himawari Satellite", .name_ja = "ひまわり衛星",
  .update_interval_sec = 600, .run = run,
};
REGISTER_SOURCE(jma_himawari_def)
