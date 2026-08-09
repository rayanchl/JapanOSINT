/* collectors/social/sources/instagram_geo.c
 * Port of server/src/collectors/instagramGeo.js (instagram-geo).
 * Keyed on INSTAGRAM_SESSION_COOKIE (Cookie + X-IG-App-ID headers). Walks a
 * fixed set of JP location IDs via web_info, emits each recent media as a
 * spatial intel item (has_geo=1) at the location lat/lng. honest empty when
 * gated / on failure. uid = instagram-geo|<shortcode> (Node featureUid:
 * NATIVE_ID_KEYS has no shortcode, so feature has no .id -> here we mirror
 * the post identity by emitting per-media with remote_key = shortcode). */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SOURCE_ID "instagram-geo"

static const char *LOCATION_IDS[] = {
  "212999109", "213385402", "214074699",
  "105466816170557", "249193905",
};
#define NL ((int)(sizeof(LOCATION_IDS)/sizeof(LOCATION_IDS[0])))

/* loc?.lat ?? loc?.location?.lat (number or null). */
static int loc_num(const cJSON *loc, const char *k, double *out) {
  if (!loc) return 0;
  const cJSON *v = cJSON_GetObjectItem(loc, k);
  if (v && cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  const cJSON *ll = cJSON_GetObjectItem(loc, "location");
  if (ll) { v = cJSON_GetObjectItem(ll, k);
    if (v && cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; } }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *cookie = getenv("INSTAGRAM_SESSION_COOKIE");
  if (!cookie || !*cookie) {
    fprintf(stderr, "[instagram-geo] gated (no INSTAGRAM_SESSION_COOKIE)\n");
    return 0;
  }
  char ch[2048];
  snprintf(ch, sizeof ch, "Cookie: %s", cookie);
  const char *hdrs[] = {
    ch,
    "X-IG-App-ID: 936619743392459",
    "Accept: application/json",
    "Referer: https://www.instagram.com/",
    NULL,
  };

  int n = 0;
  for (int li = 0; li < NL; li++) {
    char url[256];
    snprintf(url, sizeof url,
      "https://www.instagram.com/api/v1/locations/web_info/"
      "?location_id=%s&show_nearby=false", LOCATION_IDS[li]);
    cJSON *data = feed_get_json_h(ctx->http, url, hdrs, 15000);
    if (!data) continue;

    cJSON *nld = cJSON_GetObjectItem(data, "native_location_data");
    cJSON *loc = nld ? cJSON_GetObjectItem(nld, "location_info") : NULL;
    if (!loc) loc = cJSON_GetObjectItem(data, "location");
    if (!loc) loc = nld;

    double lat, lng;
    if (!loc_num(loc, "lat", &lat) || !loc_num(loc, "lng", &lng)) {
      cJSON_Delete(data); continue;
    }
    const char *locName = jo_sv(loc, "name");

    cJSON *recent = nld ? cJSON_GetObjectItem(nld, "recent") : NULL;
    cJSON *secs = recent ? cJSON_GetObjectItem(recent, "sections") : NULL;
    if (!secs) {
      cJSON *r2 = cJSON_GetObjectItem(data, "recent");
      secs = r2 ? cJSON_GetObjectItem(r2, "sections") : NULL;
    }

    if (cJSON_IsArray(secs)) {
      cJSON *sec;
      cJSON_ArrayForEach(sec, secs) {
        cJSON *lc = cJSON_GetObjectItem(sec, "layout_content");
        cJSON *medias = lc ? cJSON_GetObjectItem(lc, "medias") : NULL;
        if (!cJSON_IsArray(medias)) continue;
        cJSON *mw;
        cJSON_ArrayForEach(mw, medias) {
          cJSON *media = cJSON_GetObjectItem(mw, "media");
          if (!media) media = mw;
          const char *code = jo_sv(media, "code");
          if (!code) code = jo_sv(media, "id");
          if (!code) continue;

          char uidb[128];
          snprintf(uidb, sizeof uidb, "%s|%s", SOURCE_ID, code);
          char link[128];
          snprintf(link, sizeof link, "https://www.instagram.com/p/%s/", code);

          cJSON *cap = cJSON_GetObjectItem(media, "caption");
          const char *capText = cap ? jo_sv(cap, "text") : NULL;
          cJSON *usr = cJSON_GetObjectItem(media, "user");
          const char *owner = usr ? jo_sv(usr, "username") : NULL;

          char takenIso[32]; const char *taken = NULL;
          cJSON *ta = cJSON_GetObjectItem(media, "taken_at");
          if (ta && cJSON_IsNumber(ta)) {
            time_t t = (time_t)ta->valuedouble;
            struct tm g; gmtime_r(&t, &g);
            strftime(takenIso, sizeof takenIso, "%Y-%m-%dT%H:%M:%S.000Z", &g);
            taken = takenIso;
          }

          cJSON *props = cJSON_CreateObject();
          cJSON_AddStringToObject(props, "shortcode", code);
          cJSON_AddStringToObject(props, "location_id", LOCATION_IDS[li]);
          if (locName) cJSON_AddStringToObject(props, "location_name", locName);
          else cJSON_AddNullToObject(props, "location_name");
          if (capText) cJSON_AddStringToObject(props, "caption", capText);
          else cJSON_AddNullToObject(props, "caption");
          if (owner) cJSON_AddStringToObject(props, "owner", owner);
          else cJSON_AddNullToObject(props, "owner");
          if (taken) cJSON_AddStringToObject(props, "taken_at", taken);
          else cJSON_AddNullToObject(props, "taken_at");
          cJSON_AddStringToObject(props, "link", link);
          cJSON_AddStringToObject(props, "source", "instagram_web_api");
          char *pj = cJSON_PrintUnformatted(props);

          intel_item it = {0};
          it.uid = uidb;
          it.title = capText ? capText : code;
          it.summary = locName;
          it.link = link;
          it.author = owner;
          it.lang = "ja";
          it.published_at = taken;
          it.record_type = SOURCE_ID;
          it.has_geo = 1;
          it.lat = lat; it.lon = lng;
          it.properties_json = pj;
          if (sink->emit(sink, &it) >= 0) n++;

          free(pj);
          cJSON_Delete(props);
        }
      }
    }
    cJSON_Delete(data);
  }
  fprintf(stderr, "[instagram-geo] emitted %d\n", n);
  return 0;              /* audit-09: an empty result set is not a run error */
}

static const source_def instagram_geo_def = {
  .id = "instagram-geo", .collector = "social",
  .name = "Instagram Geotagged", .name_ja = "Instagram ジオタグ",
  .update_interval_sec = 600, .run = run,
};
REGISTER_SOURCE(instagram_geo_def)
