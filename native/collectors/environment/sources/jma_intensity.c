/* collectors/environment/sources/jma_intensity.c
 * Port of server/src/collectors/jmaIntensity.js. Bespoke transform: JMA quake
 * list.json → intensity Features, then the shared geojson toolkit maps to
 * intel. uid = jma-intensity|JMA_INT_<intelHashKey(anm,at,mag,cod)[:20]>
 * (event_id ∈ NATIVE_ID_KEYS). The JS coord quirk
 * (coordinates[0]=cod.split('+')[1], [1]=cod.split('+')[0]) is reproduced
 * verbatim for byte-parity. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include "../../../core/intel.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LIST_URL "https://www.jma.go.jp/bosai/quake/data/list.json"

/* "+38.7+141.9-50000/" : split on '+' → ["","38.7","141.9-50000/"].
 * JS: coords[0]=parseFloat(parts[1])||139.0, coords[1]=parseFloat(parts[0])||36.0 */
static double split_plus(const char *cod, int idx, double dflt) {
  if (!cod) return dflt;
  const char *p = cod; int cur = 0;
  const char *seg = p;
  for (;; p++) {
    if (*p == '+' || *p == '\0') {
      if (cur == idx) {
        if (seg == p) return dflt;          /* empty segment → NaN→dflt */
        char buf[64]; size_t l = (size_t)(p - seg);
        if (l >= sizeof buf) l = sizeof buf - 1;
        memcpy(buf, seg, l); buf[l] = 0;
        char *end; double v = strtod(buf, &end);
        return end == buf ? dflt : v;
      }
      cur++; seg = p + 1;
      if (*p == '\0') break;
    }
  }
  return dflt;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *arr = feed_get_json(ctx->http, LIST_URL, 20000);
  if (!arr || !cJSON_IsArray(arr)) { if (arr) cJSON_Delete(arr); return -1; }

  cJSON *features = cJSON_CreateArray();
  int i = 0;
  cJSON *q;
  cJSON_ArrayForEach(q, arr) {
    if (i++ >= 100) break;                  /* data.slice(0,100) */
    const cJSON *anm = cJSON_GetObjectItem(q, "anm");
    const cJSON *mag = cJSON_GetObjectItem(q, "mag");
    const cJSON *maxi= cJSON_GetObjectItem(q, "maxi");
    const cJSON *at  = cJSON_GetObjectItem(q, "at");
    const cJSON *cod = cJSON_GetObjectItem(q, "cod");
    const char *cods = (cod && cJSON_IsString(cod)) ? cod->valuestring : NULL;

    double lon = split_plus(cods, 1, 139.0);
    double lat = split_plus(cods, 0, 36.0);

    const char *parts[4] = {
      anm && cJSON_IsString(anm) ? anm->valuestring : NULL,
      at  && cJSON_IsString(at)  ? at->valuestring  : NULL,
      mag && cJSON_IsString(mag) ? mag->valuestring : NULL,
      cods };
    char hk[21]; feed_hash_key(hk, parts, 4);
    char eid[40]; snprintf(eid, sizeof eid, "JMA_INT_%s", hk);

    cJSON *feat = cJSON_CreateObject();
    cJSON_AddStringToObject(feat, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(feat, "geometry", g);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "event_id", eid);
    cJSON_AddStringToObject(p, "name",
        anm && cJSON_IsString(anm) ? anm->valuestring : "Unknown");
    cJSON_AddStringToObject(p, "intensity",
        maxi && cJSON_IsString(maxi) ? maxi->valuestring : "");
    cJSON_AddStringToObject(p, "time",
        at && cJSON_IsString(at) ? at->valuestring : "");
    cJSON_AddStringToObject(p, "country", "JP");
    cJSON_AddStringToObject(p, "source", "jma_bosai");
    cJSON_AddItemToObject(feat, "properties", p);
    cJSON_AddItemToArray(features, feat);
  }
  cJSON_Delete(arr);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jma-intensity] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def jma_intensity_def = {
  .id = "jma-intensity", .collector = "environment",
  .name = "JMA Seismic Intensity", .name_ja = "気象庁 震度情報",
   .update_interval_sec = 60, .run = run,
};
REGISTER_SOURCE(jma_intensity_def)
