/* collectors/infrastructure/sources/cam_youtube_live.c
 *
 * Registered source_def — port of ONE cameraDiscovery.js channel:
 *   fromYouTubeLive()  (sources[] id 'youtube_live', label 'YouTube Live API').
 *
 * JS behaviour reproduced faithfully (the API-fetch + feature build):
 *   apiKey = envFor('YOUTUBE_API_KEY'); if (!apiKey) return [].
 *     → here: getenv("YOUTUBE_API_KEY"); unset/empty → 0 rows, return 0 (ran).
 *   1) search.list:
 *      https://www.googleapis.com/youtube/v3/search?part=snippet&type=video
 *        &eventType=live&regionCode=JP&maxResults=50&key=<urlenc key>
 *      ids = search.items[*].id.videoId filtered truthy; if 0 → return [].
 *   2) videos.list:
 *      https://www.googleapis.com/youtube/v3/videos
 *        ?part=snippet,liveStreamingDetails,recordingDetails
 *        &id=<ids join ','>&key=<urlenc key>
 *      items = videos.items || [].
 *   For each v: loc = v.recordingDetails.location; lat/lon only if
 *     Number.isFinite — else skip (the JS deliberately drops broadcasts with
 *     no owner-set coordinates rather than jitter blind search hits).
 *     makeFeature(camera_type='youtube_live', discovery_channel='youtube_live',
 *       extra order:
 *         url=`https://www.youtube.com/watch?v=<v.id>`,
 *         thumbnail_url = sn.thumbnails.high.url || sn.thumbnails.default.url
 *                         || null,
 *         operator = sn.channelTitle || null,
 *         youtube_id = v.id,
 *         youtube_channel = sn.channelId || null,
 *         concurrent_viewers = ls.concurrentViewers != null
 *                              ? Number(ls.concurrentViewers) : null,
 *         actual_start_time = ls.actualStartTime || null,
 *         location_description = v.recordingDetails.locationDescription
 *                                || null).
 *
 * The JS channel fromYouTubeLive() does NOT invoke upgradeYouTubeStreamUrls
 * (that concurrency worker is only used by the SCRAPER channels — it builds
 * features straight from the Data API), so there is nothing concurrent to
 * port here: this is a faithful two-call API fetch + feature build.
 *
 * Every feature → camera_upsert(channel="youtube_live").  makeFeature parity
 * byte-identical to camera_discovery.c.
 *
 * No YOUTUBE_API_KEY (the default deployment) → 0 cameras, run() returns 0 —
 * faithful to the JS `if (!apiKey) return []`.  A hard fetch failure of the
 * search call → return -1 (fetch fail); empty result sets → 0 rows, return 0.
 */
#include "../../../source.h"
#include "../../../core/camera_store.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
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

/* encodeURIComponent for an API key (RFC3986 unreserved kept verbatim;
 * everything else %-encoded — covers the JS encodeURIComponent(apiKey)). */
static void url_enc(const char *s, char *out, size_t n) {
  static const char *hex = "0123456789ABCDEF";
  size_t o = 0;
  for (size_t i = 0; s[i] && o + 4 < n; i++) {
    unsigned char c = (unsigned char)s[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') ||
        c == '-' || c == '_' || c == '.' || c == '!' || c == '~' ||
        c == '*' || c == '\'' || c == '(' || c == ')') {
      out[o++] = (char)c;
    } else {
      out[o++] = '%';
      out[o++] = hex[c >> 4];
      out[o++] = hex[c & 0xF];
    }
  }
  out[o] = 0;
}

/* obj.thumbnails.<which>.url string or NULL. */
static const char *thumb_url(cJSON *sn, const char *which) {
  cJSON *t = sn ? cJSON_GetObjectItem(sn, "thumbnails") : NULL;
  cJSON *w = t ? cJSON_GetObjectItem(t, which) : NULL;
  cJSON *u = w ? cJSON_GetObjectItem(w, "url") : NULL;
  return (u && cJSON_IsString(u) && u->valuestring[0]) ? u->valuestring : NULL;
}
static const char *str_or_null(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *apiKey = getenv("YOUTUBE_API_KEY");
  if (!apiKey || !*apiKey) {
    fprintf(stderr,
      "[cam-youtube_live] YOUTUBE_API_KEY not set, skipping (0 cameras)\n");
    return 0;                          /* if (!apiKey) return [] */
  }
  char ek[256];
  url_enc(apiKey, ek, sizeof ek);

  /* 1) search.list */
  char surl[512];
  snprintf(surl, sizeof surl,
    "https://www.googleapis.com/youtube/v3/search"
    "?part=snippet&type=video&eventType=live&regionCode=JP&maxResults=50"
    "&key=%s", ek);
  cJSON *search = feed_get_json_h(ctx->http, surl, NULL, 20000);
  if (!search) return -1;              /* fetch fail */

  cJSON *sitems = cJSON_GetObjectItem(search, "items");
  /* ids = items[*].id.videoId filtered truthy */
  char *idlist = NULL;
  size_t idlen = 0;
  int nids = 0;
  if (cJSON_IsArray(sitems)) {
    cJSON *it;
    cJSON_ArrayForEach(it, sitems) {
      cJSON *idobj = cJSON_GetObjectItem(it, "id");
      cJSON *vid = idobj ? cJSON_GetObjectItem(idobj, "videoId") : NULL;
      if (vid && cJSON_IsString(vid) && vid->valuestring[0]) {
        size_t vl = strlen(vid->valuestring);
        char *nl = realloc(idlist, idlen + vl + 2);
        if (!nl) { free(idlist); cJSON_Delete(search); return -1; }
        idlist = nl;
        if (idlen) idlist[idlen++] = ',';      /* ids.join(',') */
        memcpy(idlist + idlen, vid->valuestring, vl);
        idlen += vl;
        idlist[idlen] = 0;
        nids++;
      }
    }
  }
  cJSON_Delete(search);
  if (nids == 0) { free(idlist); return 0; }   /* ids.length===0 → [] */

  /* 2) videos.list */
  size_t vurl_sz = idlen + 512;
  char *vurl = malloc(vurl_sz);
  if (!vurl) { free(idlist); return -1; }
  snprintf(vurl, vurl_sz,
    "https://www.googleapis.com/youtube/v3/videos"
    "?part=snippet,liveStreamingDetails,recordingDetails"
    "&id=%s&key=%s", idlist, ek);
  free(idlist);
  cJSON *videos = feed_get_json_h(ctx->http, vurl, NULL, 20000);
  free(vurl);
  if (!videos) return 0;               /* JS: videos?.items || [] → []  */

  cJSON *vitems = cJSON_GetObjectItem(videos, "items");
  int count = 0;
  if (cJSON_IsArray(vitems)) {
    cJSON *v;
    cJSON_ArrayForEach(v, vitems) {
      cJSON *rd  = cJSON_GetObjectItem(v, "recordingDetails");
      cJSON *loc = rd ? cJSON_GetObjectItem(rd, "location") : NULL;
      cJSON *latv = loc ? cJSON_GetObjectItem(loc, "latitude") : NULL;
      cJSON *lonv = loc ? cJSON_GetObjectItem(loc, "longitude") : NULL;
      if (!latv || !cJSON_IsNumber(latv) || !lonv || !cJSON_IsNumber(lonv))
        continue;
      double lat = latv->valuedouble, lon = lonv->valuedouble;
      if (!isfinite(lat) || !isfinite(lon)) continue;

      cJSON *sn = cJSON_GetObjectItem(v, "snippet");
      cJSON *ls = cJSON_GetObjectItem(v, "liveStreamingDetails");
      cJSON *idv = cJSON_GetObjectItem(v, "id");
      const char *vidid = (idv && cJSON_IsString(idv)) ? idv->valuestring : "";

      char name[256];
      const char *title = str_or_null(sn, "title");
      if (title) snprintf(name, sizeof name, "%s", title);
      else snprintf(name, sizeof name, "YouTube live %s", vidid);

      char watch[80];
      snprintf(watch, sizeof watch,
               "https://www.youtube.com/watch?v=%s", vidid);

      const char *thumb = thumb_url(sn, "high");
      if (!thumb) thumb = thumb_url(sn, "default");
      const char *chTitle = str_or_null(sn, "channelTitle");
      const char *chId = str_or_null(sn, "channelId");
      const char *startT = str_or_null(ls, "actualStartTime");
      const char *locDesc = rd ? str_or_null(rd, "locationDescription") : NULL;

      /* concurrent_viewers: ls.concurrentViewers != null ? Number(it) : null
       * (the API returns it as a STRING; Number() → numeric). */
      int cv_present = 0; double cv_num = 0;
      if (ls) {
        cJSON *cvv = cJSON_GetObjectItem(ls, "concurrentViewers");
        if (cvv && !cJSON_IsNull(cvv)) {
          if (cJSON_IsString(cvv)) { cv_num = atof(cvv->valuestring);
                                     cv_present = 1; }
          else if (cJSON_IsNumber(cvv)) { cv_num = cvv->valuedouble;
                                          cv_present = 1; }
        }
      }

      /* extra order: url, thumbnail_url, operator, youtube_id,
       *              youtube_channel, concurrent_viewers, actual_start_time,
       *              location_description */
      kv ex[8]; int n = 0;
      ex[n].k="url"; ex[n].is_num=0; ex[n].is_null=0; ex[n].sv=watch;
        ex[n].nv=0; n++;
      ex[n].k="thumbnail_url"; ex[n].is_num=0; ex[n].is_null=(thumb==NULL);
        ex[n].sv=thumb; ex[n].nv=0; n++;
      ex[n].k="operator"; ex[n].is_num=0; ex[n].is_null=(chTitle==NULL);
        ex[n].sv=chTitle; ex[n].nv=0; n++;
      ex[n].k="youtube_id"; ex[n].is_num=0; ex[n].is_null=0; ex[n].sv=vidid;
        ex[n].nv=0; n++;
      ex[n].k="youtube_channel"; ex[n].is_num=0; ex[n].is_null=(chId==NULL);
        ex[n].sv=chId; ex[n].nv=0; n++;
      if (cv_present) {
        ex[n].k="concurrent_viewers"; ex[n].is_num=1; ex[n].is_null=0;
          ex[n].sv=NULL; ex[n].nv=cv_num; n++;
      } else {
        ex[n].k="concurrent_viewers"; ex[n].is_num=0; ex[n].is_null=1;
          ex[n].sv=NULL; ex[n].nv=0; n++;
      }
      ex[n].k="actual_start_time"; ex[n].is_num=0; ex[n].is_null=(startT==NULL);
        ex[n].sv=startT; ex[n].nv=0; n++;
      ex[n].k="location_description"; ex[n].is_num=0;
        ex[n].is_null=(locDesc==NULL); ex[n].sv=locDesc; ex[n].nv=0; n++;

      cJSON *f = make_feature(lat, lon, name, "youtube_live",
                              "youtube_live", ex, n);
      if (camera_upsert(ctx->db, sink, f, "youtube_live") >= 0) count++;
      cJSON_Delete(f);
    }
  }
  cJSON_Delete(videos);
  fprintf(stderr, "[cam-youtube_live] keyed; upserted %d cameras\n", count);
  return 0;
}

static const source_def cam_youtube_live_def = {
  .id = "cam-youtube_live", .collector = "infrastructure",
  .name = "Camera discovery — YouTube Live API",
  .name_ja = "カメラ探索 — YouTube Live API",
   .update_interval_sec = 21600, .run = run };
REGISTER_SOURCE(cam_youtube_live_def)
