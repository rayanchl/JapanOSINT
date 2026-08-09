/* collectors/environment/sources/wolfx_eqlist.c
 * Port of server/src/collectors/wolfxEqlist.js. api.wolfx.jp/jma_eqlist.json
 * is an OBJECT keyed by report; each object value → a Feature. uid via
 * event_id (∈ NATIVE_ID_KEYS) = EventID ?? key. Non-object values (md5,
 * Title…) skipped, exactly like `typeof v==='object' && v`. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../core/intel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define API_URL "https://api.wolfx.jp/jma_eqlist.json"

static void add_or_null(cJSON *p, const char *k, cJSON *v) {
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *data = feed_get_json(ctx->http, API_URL, 8000);
  if (!data || !cJSON_IsObject(data)) { if (data) cJSON_Delete(data); return -1; }

  cJSON *features = cJSON_CreateArray();
  cJSON *eq;
  cJSON_ArrayForEach(eq, data) {                  /* Object.entries order */
    if (!cJSON_IsObject(eq)) continue;            /* typeof v==='object' && v */
    const char *key = eq->string ? eq->string : "";

    cJSON *lat = cJSON_GetObjectItem(eq, "Latitude");
    cJSON *lon = cJSON_GetObjectItem(eq, "Longitude");
    double dlat = lat ? (cJSON_IsNumber(lat) ? lat->valuedouble
                        : (cJSON_IsString(lat) ? atof(lat->valuestring) : 0)) : 0;
    double dlon = lon ? (cJSON_IsNumber(lon) ? lon->valuedouble
                        : (cJSON_IsString(lon) ? atof(lon->valuestring) : 0)) : 0;
    int geo = (lat && lon && dlat == dlat && dlon == dlon &&
               (cJSON_IsNumber(lat) || (cJSON_IsString(lat) && lat->valuestring[0])) &&
               (cJSON_IsNumber(lon) || (cJSON_IsString(lon) && lon->valuestring[0])));

    cJSON *feat = cJSON_CreateObject();
    cJSON_AddStringToObject(feat, "type", "Feature");
    if (geo) {
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(dlon));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(dlat));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(feat, "geometry", g);
    }
    /* else: no "geometry" key at all. NOT cJSON_CreateNull() — lib/geojson.c:235
     * serialises whatever is under "geometry", so an explicit null is persisted
     * as the literal 4-byte string "null" and reads as "has geometry". */
    cJSON *p = cJSON_CreateObject();             /* exact JS key order */
    cJSON *eid = cJSON_GetObjectItem(eq, "EventID");
    if (eid) cJSON_AddItemToObject(p, "event_id", cJSON_Duplicate(eid, 1));
    else cJSON_AddStringToObject(p, "event_id", key);
    cJSON *mg = cJSON_GetObjectItem(eq, "Magunitude");
    if (!mg) mg = cJSON_GetObjectItem(eq, "Magnitude");
    add_or_null(p, "magnitude", mg);
    add_or_null(p, "depth_km",      cJSON_GetObjectItem(eq, "Depth"));
    /* AUDIT NOTE (slice a3, 2026-07-31): these two were read as "MaxIntensity"
     * and "Hypocenter" and came back null on EVERY row. cJSON_GetObjectItem is
     * case-insensitive, which is why Latitude/Magnitude/Depth happened to work
     * against the feed's lower-case latitude/magnitude/depth — but the feed
     * names the other two "shindo" and "location", which no casing of the old
     * keys can match. Verified against the live payload today:
     *   {"Title":"震源・震度情報","EventID":"20260801014850","time":"2026/08/01 01:48",
     *    "time_full":…,"location":"熊本県熊本地方","magnitude":"2.3","shindo":"1",
     *    "depth":"10km","latitude":"32.7","longitude":"130.7","info":…}
     * So every row shipped with no place name and no JMA intensity — the two
     * fields a reader actually wants — and the title degraded to a bare "M2.3". */
    cJSON *inten = cJSON_GetObjectItem(eq, "shindo");
    if (!inten) inten = cJSON_GetObjectItem(eq, "MaxIntensity");
    cJSON *place = cJSON_GetObjectItem(eq, "location");
    if (!place) place = cJSON_GetObjectItem(eq, "Hypocenter");
    add_or_null(p, "max_intensity", inten);
    add_or_null(p, "place",         place);
    cJSON *tm = cJSON_GetObjectItem(eq, "time");
    if (!tm) tm = cJSON_GetObjectItem(eq, "Time");
    add_or_null(p, "time", tm);
    /* time_full and info (the tsunami advisory line) were dropped entirely. */
    add_or_null(p, "time_full",      cJSON_GetObjectItem(eq, "time_full"));
    add_or_null(p, "tsunami_info",   cJSON_GetObjectItem(eq, "info"));
    add_or_null(p, "report_type",    cJSON_GetObjectItem(eq, "Title"));
    /* Every row was landing in the UI with an empty title (the geojson sink
     * picks title/name/name_ja/label and this feed carries none). Build one
     * from the real fields: "M6.1 — 石川県能登地方 (震度5弱)". */
    {
      char magbuf[32] = {0}, intbuf[32] = {0}, title[256];
      if (mg) {
        if (cJSON_IsNumber(mg)) snprintf(magbuf, sizeof magbuf, "M%.1f", mg->valuedouble);
        else if (cJSON_IsString(mg) && mg->valuestring[0])
          snprintf(magbuf, sizeof magbuf, "M%s", mg->valuestring);
      }
      if (inten) {
        if (cJSON_IsNumber(inten)) snprintf(intbuf, sizeof intbuf, " (\xE9\x9C\x87\xE5\xBA\xA6%g)", inten->valuedouble);
        else if (cJSON_IsString(inten) && inten->valuestring[0])
          snprintf(intbuf, sizeof intbuf, " (\xE9\x9C\x87\xE5\xBA\xA6%s)", inten->valuestring);
      }
      const char *ps = (place && cJSON_IsString(place) && place->valuestring[0])
                         ? place->valuestring : NULL;
      if (magbuf[0] && ps)      snprintf(title, sizeof title, "%s \xE2\x80\x94 %s%s", magbuf, ps, intbuf);
      else if (ps)              snprintf(title, sizeof title, "%s%s", ps, intbuf);
      else if (magbuf[0])       snprintf(title, sizeof title, "%s%s", magbuf, intbuf);
      else                      snprintf(title, sizeof title, "JMA earthquake %s", key);
      cJSON_AddStringToObject(p, "title", title);
    }
    /* published_at was the feed's raw "2026/08/01 01:48", which is not ISO-8601
     * and text-sorts ahead of every ISO row in the timeline. JMA publishes these
     * in JST; normalise, preferring time_full because it carries seconds. */
    {
      cJSON *src = cJSON_GetObjectItem(eq, "time_full");
      if (!(src && cJSON_IsString(src) && src->valuestring[0])) src = tm;
      if (src && cJSON_IsString(src)) {
        int Y, Mo, D, H, Mi, S = 0;
        int got = sscanf(src->valuestring, "%d/%d/%d %d:%d:%d",
                         &Y, &Mo, &D, &H, &Mi, &S);
        if (got >= 5) {
          char iso[40];
          snprintf(iso, sizeof iso, "%04d-%02d-%02dT%02d:%02d:%02d+09:00",
                   Y, Mo, D, H, Mi, got >= 6 ? S : 0);
          cJSON_AddStringToObject(p, "published_at", iso);
        }
      }
    }
    cJSON_AddStringToObject(p, "record_type", "earthquake");
    cJSON_AddStringToObject(p, "link", "https://api.wolfx.jp/jma_eqlist.json");
    cJSON_AddStringToObject(p, "source", "wolfx_eqlist");
    cJSON_AddItemToObject(feat, "properties", p);
    cJSON_AddItemToArray(features, feat);
  }
  cJSON_Delete(data);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[wolfx-eqlist] emitted %d\n", n);
  return 0;                  /* a quiet seismic hour is honest, not an error */
}

static const source_def wolfx_eqlist_def = {
  .id = "wolfx-eqlist", .collector = "environment",
  .name = "Wolfx JMA Earthquake List", .name_ja = "Wolfx 地震一覧",
   .update_interval_sec = 60, .run = run,
};
REGISTER_SOURCE(wolfx_eqlist_def)
