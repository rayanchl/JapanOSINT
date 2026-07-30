/* collectors/infrastructure/sources/cam_windy_api.c
 *
 * Registered source_def — port of ONE cameraDiscovery.js channel:
 *   fromWindy()  (sources[] id 'windy_api', label 'Windy API').
 *
 * JS behaviour reproduced faithfully:
 *   key = envFor('WINDY_API_KEY'); if (!key) { log; return []; }
 *     → here: getenv("WINDY_API_KEY"); unset/empty → 0 rows, return 0 (ran).
 *   pageSize = 50 (Windy v3 caps `limit` at 50).
 *   for offset = 0; offset < 2000; offset += 50:
 *     url = https://api.windy.com/webcams/api/v3/webcams
 *           ?countries=JP&limit=50&offset=<offset>&include=location,player,images
 *     data = fetchJson(url, {headers:{'x-windy-api-key':key}}, 15 s)
 *       — on fetch throw: break (JS `catch { break; }`).
 *     items = Array.isArray(data.webcams) ? data.webcams : []
 *     if (items.length === 0) break;
 *     for it: lat = it.location.latitude, lon = it.location.longitude;
 *       skip if !finite. embed = it.player.live.embed || it.player.day.embed
 *         || null. thumb = it.images.current.thumbnail ||
 *         it.images.current.preview || null.
 *       makeFeature(camera_type='aggregator_windy',
 *         discovery_channel='windy_api',
 *         extra order: url=embed, thumbnail_url=thumb, windy_id=it.webcamId,
 *                      city=it.location.city||null).
 *     if (items.length < pageSize) break;
 *
 *   This is the FAITHFUL bounded paged loop from the JS (offset 0..1950 in 40
 *   pages of 50), reproduced exactly — NOT a fixed single page.  It still
 *   terminates early on empty/short page or fetch failure exactly as the JS
 *   `break`s, so against a real Windy account it walks only as many pages as
 *   the JP set needs (≤40 hard cap, matching `offset < 2000`).
 *
 * Every feature → camera_upsert(channel="windy_api").  makeFeature parity
 * byte-identical to camera_discovery.c.
 *
 * No WINDY_API_KEY (the default deployment) → 0 cameras, run() returns 0 —
 * faithful to envFor() returning '' and the JS early `return []`.  A fetch
 * failure on the very FIRST page (offset 0) → return -1 (fetch fail); a later
 * page failure is a JS `break` (already have rows) → return 0.
 */
#include "../../source.h"
#include "../../core/camera_store.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct { const char *k; const char *sv; int is_num; double nv;
                 int is_null; } kv;
static double round4(double v) { return floor(v * 1e4 + 0.5) / 1e4; }
static void uid_tail(const char *url, const char *name, char *out,
                     size_t outsz) {
  const char *src = (url && *url) ? url : (name ? name : "");
  size_t i = 0;
  for (; src[i] && i < 60 && i + 1 < outsz; i++) {
    unsigned char c = (unsigned char)src[i];
    out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
  }
  out[i] = 0;
}
static void js_num(double v, char *out, size_t outsz) {
  cJSON *n = cJSON_CreateNumber(v);
  char *s = cJSON_PrintUnformatted(n);
  snprintf(out, outsz, "%s", s ? s : "0");
  free(s);
  cJSON_Delete(n);
}
static cJSON *make_feature(double lat, double lon, const char *name,
                           const char *camera_type,
                           const char *discovery_channel,
                           const kv *extra, int nextra) {
  const char *url = NULL;
  for (int i = 0; i < nextra; i++)
    if (strcmp(extra[i].k, "url") == 0 && !extra[i].is_null && extra[i].sv) {
      url = extra[i].sv; break;
    }
  char lats[32], lons[32], tail[80], uid[160];
  snprintf(lats, sizeof lats, "%.4f", round4(lat));
  snprintf(lons, sizeof lons, "%.4f", round4(lon));
  uid_tail(url, name, tail, sizeof tail);
  snprintf(uid, sizeof uid, "%s:%s:%s", lats, lons, tail);
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "camera_uid", uid);
  cJSON_AddStringToObject(p, "name", (name && *name) ? name : "Unknown camera");
  cJSON_AddStringToObject(p, "camera_type",
                          (camera_type && *camera_type) ? camera_type
                                                        : "unknown");
  cJSON_AddStringToObject(p, "discovery_channel", discovery_channel);
  cJSON_AddStringToObject(p, "country", "JP");
  for (int i = 0; i < nextra; i++) {
    const kv *e = &extra[i];
    if (e->is_null) cJSON_AddNullToObject(p, e->k);
    else if (e->is_num) cJSON_AddNumberToObject(p, e->k, e->nv);
    else cJSON_AddItemToObject(p, e->k,
           e->sv ? cJSON_CreateString(e->sv) : cJSON_CreateNull());
  }
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

/* nested getter: obj[a][b] string, or NULL. */
static const char *getstr2(cJSON *o, const char *a, const char *b) {
  cJSON *x = o ? cJSON_GetObjectItem(o, a) : NULL;
  cJSON *y = x ? cJSON_GetObjectItem(x, b) : NULL;
  return (y && cJSON_IsString(y) && y->valuestring[0]) ? y->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("WINDY_API_KEY");
  if (!key || !*key) {
    fprintf(stderr,
      "[cam-windy_api] WINDY_API_KEY not set, skipping (0 cameras)\n");
    return 0;                          /* envFor()→'' → return [] */
  }

  char hdr[256];
  snprintf(hdr, sizeof hdr, "x-windy-api-key: %s", key);
  const char *headers[] = { hdr, NULL };

  const int pageSize = 50;
  int count = 0;
  int first_page = 1;

  for (int offset = 0; offset < 2000; offset += pageSize) {
    char url[256];
    snprintf(url, sizeof url,
      "https://api.windy.com/webcams/api/v3/webcams"
      "?countries=JP&limit=%d&offset=%d&include=location,player,images",
      pageSize, offset);
    cJSON *data = feed_get_json_h(ctx->http, url, headers, 15000);
    if (!data) {
      /* JS: catch { break; }  — fetch failed/non-2xx. */
      if (first_page) return -1;       /* first-page fetch fail */
      break;
    }
    first_page = 0;
    cJSON *webcams = cJSON_GetObjectItem(data, "webcams");
    if (!cJSON_IsArray(webcams)) {     /* items = [] → length 0 → break */
      cJSON_Delete(data);
      break;
    }
    int items = cJSON_GetArraySize(webcams);
    if (items == 0) { cJSON_Delete(data); break; }

    cJSON *it;
    cJSON_ArrayForEach(it, webcams) {
      cJSON *loc = cJSON_GetObjectItem(it, "location");
      cJSON *latv = loc ? cJSON_GetObjectItem(loc, "latitude") : NULL;
      cJSON *lonv = loc ? cJSON_GetObjectItem(loc, "longitude") : NULL;
      if (!latv || !cJSON_IsNumber(latv) || !lonv || !cJSON_IsNumber(lonv))
        continue;
      double lat = latv->valuedouble, lon = lonv->valuedouble;
      if (!isfinite(lat) || !isfinite(lon)) continue;

      /* embed = player.live.embed || player.day.embed || null */
      const char *embed = getstr2(cJSON_GetObjectItem(it, "player"),
                                  "live", "embed");
      if (!embed) embed = getstr2(cJSON_GetObjectItem(it, "player"),
                                  "day", "embed");
      /* thumb = images.current.thumbnail || images.current.preview || null */
      const char *thumb = getstr2(cJSON_GetObjectItem(it, "images"),
                                  "current", "thumbnail");
      if (!thumb) thumb = getstr2(cJSON_GetObjectItem(it, "images"),
                                  "current", "preview");

      cJSON *titlev = cJSON_GetObjectItem(it, "title");
      cJSON *widv   = cJSON_GetObjectItem(it, "webcamId");
      char namebuf[160];
      const char *name;
      if (titlev && cJSON_IsString(titlev) && titlev->valuestring[0]) {
        name = titlev->valuestring;
      } else {
        char widstr[40];
        if (widv && cJSON_IsNumber(widv)) js_num(widv->valuedouble,
                                                 widstr, sizeof widstr);
        else if (widv && cJSON_IsString(widv))
          snprintf(widstr, sizeof widstr, "%s", widv->valuestring);
        else widstr[0] = 0;
        snprintf(namebuf, sizeof namebuf, "Windy webcam %s", widstr);
        name = namebuf;
      }
      const char *city = NULL;
      if (loc) {
        cJSON *cv = cJSON_GetObjectItem(loc, "city");
        if (cv && cJSON_IsString(cv) && cv->valuestring[0])
          city = cv->valuestring;
      }

      /* extra order: url, thumbnail_url, windy_id, city */
      kv ex[4]; int n = 0;
      ex[n].k="url"; ex[n].is_num=0; ex[n].is_null=(embed==NULL);
        ex[n].sv=embed; ex[n].nv=0; n++;
      ex[n].k="thumbnail_url"; ex[n].is_num=0; ex[n].is_null=(thumb==NULL);
        ex[n].sv=thumb; ex[n].nv=0; n++;
      /* windy_id: pass through as number if numeric (JS it.webcamId is a
       * number in the v3 API); string fallback if the API ever returns a
       * string id. */
      if (widv && cJSON_IsNumber(widv)) {
        ex[n].k="windy_id"; ex[n].is_num=1; ex[n].is_null=0;
          ex[n].sv=NULL; ex[n].nv=widv->valuedouble; n++;
      } else {
        ex[n].k="windy_id"; ex[n].is_num=0;
          ex[n].is_null=!(widv && cJSON_IsString(widv));
          ex[n].sv=(widv && cJSON_IsString(widv)) ? widv->valuestring : NULL;
          ex[n].nv=0; n++;
      }
      ex[n].k="city"; ex[n].is_num=0; ex[n].is_null=(city==NULL);
        ex[n].sv=city; ex[n].nv=0; n++;

      cJSON *f = make_feature(lat, lon, name, "aggregator_windy",
                              "windy_api", ex, n);
      if (camera_upsert(ctx->db, sink, f, "windy_api") >= 0) count++;
      cJSON_Delete(f);
    }
    cJSON_Delete(data);
    if (items < pageSize) break;       /* JS: last partial page */
  }
  fprintf(stderr, "[cam-windy_api] keyed; upserted %d cameras\n", count);
  return 0;
}

static const source_def cam_windy_api_def = {
  .id = "cam-windy_api", .collector = "camera-discovery",
  .name = "Camera discovery — Windy API",
  .name_ja = "カメラ探索 — Windy API",
   .layer = "cameras",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(cam_windy_api_def)
