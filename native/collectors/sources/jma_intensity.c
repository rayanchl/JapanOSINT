/* collectors/environment/sources/jma_intensity.c
 * Port of server/src/collectors/jmaIntensity.js. Bespoke transform: JMA quake
 * list.json → intensity Features, then the shared geojson toolkit maps to
 * intel. uid = jma-intensity|JMA_INT_<intelHashKey(anm,at,mag,cod)[:20]>
 * (event_id ∈ NATIVE_ID_KEYS). The JS coord quirk
 * (coordinates[0]=cod.split('+')[1], [1]=cod.split('+')[0]) is reproduced
 * verbatim for byte-parity. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../core/intel.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define LIST_URL "https://www.jma.go.jp/bosai/quake/data/list.json"

/* JMA `cod` is an ISO-6709 point: "+32.7+130.7-10000/" = lat +32.7, lon
 * +130.7, depth -10000 m. Southern/western epicentres carry a leading '-'.
 *
 * audit-09 — THIS WAS PUTTING EVERY EPICENTRE IN THE WRONG PLACE. The old
 * code split on '+' and took parts[1] as the LONGITUDE and parts[0] as the
 * LATITUDE. Splitting "+32.7+130.7-10000/" on '+' yields ["", "32.7",
 * "130.7-10000/"], so parts[1] is the LATITUDE (published as longitude) and
 * parts[0] is the empty string before the leading '+', which fell through to
 * the 36.0 default. Every one of the ~93 rows was emitted at lat 36.0 with a
 * longitude of ~30–45 — a point in the eastern Mediterranean, not Japan. It
 * also never split on '-', so a negative ordinate could not parse at all.
 * Parse the signed ordinates in order instead. */
static int parse_cod(const char *cod, double *lat, double *lon, double *dep_m) {
  if (!cod) return 0;
  double v[3]; int nv = 0;
  for (const char *p = cod; *p && nv < 3; ) {
    if (*p == '+' || *p == '-') {
      char *end; double d = strtod(p, &end);
      if (end != p) { v[nv++] = d; p = end; continue; }
    }
    p++;
  }
  if (nv < 2) return 0;
  *lat = v[0]; *lon = v[1];
  if (dep_m) *dep_m = (nv >= 3) ? v[2] : 0.0;
  return 1;
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
    const cJSON *eid_j = cJSON_GetObjectItem(q, "eid");
    const cJSON *rdt   = cJSON_GetObjectItem(q, "rdt");
    const cJSON *ttl   = cJSON_GetObjectItem(q, "ttl");
    const cJSON *enanm = cJSON_GetObjectItem(q, "en_anm");
    const cJSON *entt  = cJSON_GetObjectItem(q, "en_ttl");
    const cJSON *acd   = cJSON_GetObjectItem(q, "acd");
    const cJSON *jsonf = cJSON_GetObjectItem(q, "json");

    double lat = 0, lon = 0, dep_m = 0;
    int has_geo = parse_cod(cods, &lat, &lon, &dep_m);
    if (!has_geo) continue;      /* no epicentre → no pin, and no fake one */

    const char *parts[4] = {
      anm && cJSON_IsString(anm) ? anm->valuestring : NULL,
      at  && cJSON_IsString(at)  ? at->valuestring  : NULL,
      mag && cJSON_IsString(mag) ? mag->valuestring : NULL,
      cods };
    char hk[21]; feed_hash_key(hk, parts, 4);
    char eid[40]; snprintf(eid, sizeof eid, "JMA_INT_%s", hk);

    cJSON *feat = gj_point_feature(lon, lat);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "event_id", eid);
    /* audit-09: the upstream row carries a magnitude, a JMA event id, a report
     * time, the English place name and the detail-file name; none of them were
     * being carried into the intel row. A seismic-intensity pin without the
     * magnitude is exactly the "labels, not data" shape. */
    const char *nm = anm && cJSON_IsString(anm) ? anm->valuestring : "Unknown";
    const char *mg = mag && cJSON_IsString(mag) ? mag->valuestring : NULL;
    const char *mi = maxi && cJSON_IsString(maxi) ? maxi->valuestring : NULL;
    char title[256];
    snprintf(title, sizeof title, "%s%s%s%s%s%s", nm,
             mg ? " M" : "", mg ? mg : "",
             mi ? " 震度" : "", mi ? mi : "",
             "");
    cJSON_AddStringToObject(p, "title", title);
    cJSON_AddStringToObject(p, "name", nm);
    cJSON_AddStringToObject(p, "record_type", "earthquake");
    if (mg) cJSON_AddNumberToObject(p, "magnitude", strtod(mg, NULL));
    cJSON_AddStringToObject(p, "intensity", mi ? mi : "");
    cJSON_AddNumberToObject(p, "depth_km", -dep_m / 1000.0);
    cJSON_AddStringToObject(p, "time",
        at && cJSON_IsString(at) ? at->valuestring : "");
    if (rdt && cJSON_IsString(rdt))
      cJSON_AddStringToObject(p, "reported_at", rdt->valuestring);
    if (eid_j && cJSON_IsString(eid_j))
      cJSON_AddStringToObject(p, "jma_event_id", eid_j->valuestring);
    if (ttl && cJSON_IsString(ttl))
      cJSON_AddStringToObject(p, "bulletin", ttl->valuestring);
    if (entt && cJSON_IsString(entt))
      cJSON_AddStringToObject(p, "bulletin_en", entt->valuestring);
    if (enanm && cJSON_IsString(enanm))
      cJSON_AddStringToObject(p, "name_en", enanm->valuestring);
    if (acd && cJSON_IsString(acd))
      cJSON_AddStringToObject(p, "area_code", acd->valuestring);
    if (jsonf && cJSON_IsString(jsonf)) {
      char link[256];
      snprintf(link, sizeof link,
               "https://www.jma.go.jp/bosai/quake/data/%s", jsonf->valuestring);
      cJSON_AddStringToObject(p, "link", link);   /* verifiable provenance */
    }
    cJSON_AddStringToObject(p, "country", "JP");
    cJSON_AddStringToObject(p, "source", "jma_bosai");
    cJSON *tg = cJSON_CreateArray();
    cJSON_AddItemToArray(tg, cJSON_CreateString("earthquake"));
    cJSON_AddItemToArray(tg, cJSON_CreateString("jma"));
    cJSON_AddItemToObject(p, "tags", tg);
    cJSON_AddItemToObject(feat, "properties", p);
    cJSON_AddItemToArray(features, feat);
  }
  cJSON_Delete(arr);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jma-intensity] emitted %d\n", n);
  return 0;   /* audit-09: no felt quake in the window is an honest empty */
}

static const source_def jma_intensity_def = {
  .id = "jma-intensity", .collector = "environment",
  .name = "JMA Seismic Intensity", .name_ja = "気象庁 震度情報",
   .update_interval_sec = 60, .run = run,
};
REGISTER_SOURCE(jma_intensity_def)
