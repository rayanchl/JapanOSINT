/* collectors/social/sources/instagram_locations.c
 * Port of server/src/collectors/instagramLocations.js (instagram-locations).
 * Keyed on INSTAGRAM_SESSION_COOKIE. Resolves web_info metadata for a fixed
 * set of JP location pages -> non-spatial intel (a location directory).
 * honest empty when gated / on failure. uid = instagram-locations|<locId>. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOURCE_ID "instagram-locations"

static const char *LOCATION_IDS[] = {
  "212999109", "213385402", "214074699",
  "105466816170557", "249193905",
  "213820592", "214058107", "213038402",
};
#define NL ((int)(sizeof(LOCATION_IDS)/sizeof(LOCATION_IDS[0])))

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *cookie = getenv("INSTAGRAM_SESSION_COOKIE");
  if (!cookie || !*cookie) {
    fprintf(stderr, "[instagram-locations] gated (no INSTAGRAM_SESSION_COOKIE)\n");
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

  int n = 0, fetched = 0;
  for (int li = 0; li < NL; li++) {
    char url[256];
    snprintf(url, sizeof url,
      "https://www.instagram.com/api/v1/locations/web_info/"
      "?location_id=%s&show_nearby=false", LOCATION_IDS[li]);
    cJSON *data = feed_get_json_h(ctx->http, url, hdrs, 15000);
    if (!data) continue;
    fetched++;

    cJSON *nld = cJSON_GetObjectItem(data, "native_location_data");
    cJSON *loc = nld ? cJSON_GetObjectItem(nld, "location_info") : NULL;
    if (!loc) loc = cJSON_GetObjectItem(data, "location");
    if (!loc) loc = nld;
    if (!loc) { cJSON_Delete(data); continue; }

    const char *name = jo_sv(loc, "name");
    char nameBuf[64];
    if (!name) {
      snprintf(nameBuf, sizeof nameBuf, "Location %s", LOCATION_IDS[li]);
      name = nameBuf;
    }
    const char *addr = jo_sv(loc, "address");
    const char *city = jo_sv(loc, "city");
    const char *country = jo_sv(loc, "country");

    /* summary: loc.address || loc.city || null */
    const char *summary = addr ? addr : city;

    /* body: [address,city,country].filter(Boolean).join(', ') || null */
    char body[512]; body[0] = '\0';
    int first = 1;
    const char *parts[3] = { addr, city, country };
    for (int i = 0; i < 3; i++) {
      if (parts[i] && parts[i][0]) {
        if (!first) strncat(body, ", ", sizeof body - strlen(body) - 1);
        strncat(body, parts[i], sizeof body - strlen(body) - 1);
        first = 0;
      }
    }

    char uidb[128];
    snprintf(uidb, sizeof uidb, "%s|%s", SOURCE_ID, LOCATION_IDS[li]);
    char link[128];
    snprintf(link, sizeof link,
      "https://www.instagram.com/explore/locations/%s/", LOCATION_IDS[li]);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("instagram"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("location"));
    char *tj = cJSON_PrintUnformatted(tags);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "location_id", LOCATION_IDS[li]);
    cJSON *latv = cJSON_GetObjectItem(loc, "lat");
    if (latv && cJSON_IsNumber(latv))
      cJSON_AddNumberToObject(props, "lat", latv->valuedouble);
    else cJSON_AddNullToObject(props, "lat");
    cJSON *lngv = cJSON_GetObjectItem(loc, "lng");
    if (lngv && cJSON_IsNumber(lngv))
      cJSON_AddNumberToObject(props, "lng", lngv->valuedouble);
    else cJSON_AddNullToObject(props, "lng");
    cJSON *mc = cJSON_GetObjectItem(loc, "media_count");
    if (!mc || !cJSON_IsNumber(mc)) mc = cJSON_GetObjectItem(data, "media_count");
    if (mc && cJSON_IsNumber(mc))
      cJSON_AddNumberToObject(props, "media_count", mc->valuedouble);
    else cJSON_AddNullToObject(props, "media_count");
    if (addr) cJSON_AddStringToObject(props, "address", addr);
    else cJSON_AddNullToObject(props, "address");
    cJSON_AddStringToObject(props, "source", "instagram_web_api");
    char *pj = cJSON_PrintUnformatted(props);

    intel_item it = {0};
    it.uid = uidb;
    it.title = name;
    it.summary = summary;
    it.body = body[0] ? body : NULL;
    it.link = link;
    it.lang = "ja";
    it.record_type = SOURCE_ID;
    it.tags_json = tj;
    it.properties_json = pj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(tj); free(pj);
    cJSON_Delete(tags); cJSON_Delete(props);
    cJSON_Delete(data);
  }
  fprintf(stderr, "[instagram-locations] emitted %d (%d/%d locations fetched)\n",
          n, fetched, NL);
  /* STATUS code, not a row count: a location page with nothing new is an honest
   * empty. Only a total fetch failure is a real error. */
  return fetched > 0 ? 0 : -1;
}

static const source_def instagram_locations_def = {
  .id = "instagram-locations", .collector = "social",
  .name = "Instagram public location pages",
  .name_ja = "Instagram 公開ロケーション",
  .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(instagram_locations_def)
