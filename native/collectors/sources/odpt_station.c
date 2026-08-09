/* collectors/transport/sources/odpt_station.c
 * Port of server/src/collectors/odptRealtime.js::collectOdptStation.
 * Primary: ODPT v4 odpt:Station when an ODPT token is set. Real key-free
 * fallback (always available, ported faithfully): every OSM rail station
 * across Japan via a single area.jp Overpass query. Token idiom mirrors
 * odpt_transport.c. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *BASES[] = {
  "https://api.odpt.org/api/v4/",
  "https://api-challenge.odpt.org/api/v4/",
};

static const char *odpt_tok(void) {
  const char *t = getenv("ODPT_TOKEN");
  if (t && *t) return t;
  t = getenv("ODPT_CONSUMER_KEY");
  if (t && *t) return t;
  t = getenv("ODPT_CHALLENGE_TOKEN");
  if (t && *t) return t;
  return NULL;
}

static cJSON *odpt_get(http_client *http, const char *rdf, const char *tok) {
  for (size_t i = 0; i < sizeof BASES / sizeof BASES[0]; i++) {
    char url[512];
    snprintf(url, sizeof url, "%s%s?acl:consumerKey=%s", BASES[i], rdf, tok);
    cJSON *d = feed_get_json(http, url, 20000);
    if (d && cJSON_IsArray(d) && cJSON_GetArraySize(d) > 0) return d;
    if (d) cJSON_Delete(d);
  }
  return NULL;
}

/* odpt:stationTitle?.en || dc:title || null  (and .ja variant) */
static void title_pick(cJSON *p, const char *outk, cJSON *r,
                       const char *titleLang) {
  cJSON *t = cJSON_GetObjectItem(r, "odpt:stationTitle");
  cJSON *dct = cJSON_GetObjectItem(r, "dc:title");
  cJSON *tl = t ? cJSON_GetObjectItem(t, titleLang) : NULL;
  if (tl && cJSON_IsString(tl) && tl->valuestring[0])
    cJSON_AddStringToObject(p, outk, tl->valuestring);
  else if (dct && cJSON_IsString(dct) && dct->valuestring[0])
    cJSON_AddStringToObject(p, outk, dct->valuestring);
  else
    cJSON_AddItemToObject(p, outk, cJSON_CreateNull());
}

/* OSM fallback map — EXACT JS property key order. */
static cJSON *osm_map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char sid[64];
  snprintf(sid, sizeof sid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "station_uid", sid);
  const char *nen = ov_tag(el, "name:en");
  const char *nm = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "station_name",
    nen ? nen : (nm ? nm : "Station"));
  if (nm) cJSON_AddStringToObject(p, "station_name_ja", nm);
  else cJSON_AddItemToObject(p, "station_name_ja", cJSON_CreateNull());
  const char *op = ov_tag(el, "operator");
  if (op) cJSON_AddStringToObject(p, "operator", op);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  const char *rw = ov_tag(el, "railway");
  if (rw) cJSON_AddStringToObject(p, "railway", rw);
  else cJSON_AddItemToObject(p, "railway", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *tok = odpt_tok();
  cJSON *features = cJSON_CreateArray();

  if (tok) {
    cJSON *rows = odpt_get(ctx->http, "odpt:Station", tok);
    if (rows) {
      cJSON *r;
      cJSON_ArrayForEach(r, rows) {
        cJSON *la = cJSON_GetObjectItem(r, "geo:lat");
        cJSON *lo = cJSON_GetObjectItem(r, "geo:long");
        if (!la || cJSON_IsNull(la) || !lo || cJSON_IsNull(lo)) continue;
        double lat = cJSON_IsNumber(la) ? la->valuedouble
                   : (cJSON_IsString(la) ? strtod(la->valuestring, NULL) : 0);
        double lon = cJSON_IsNumber(lo) ? lo->valuedouble
                   : (cJSON_IsString(lo) ? strtod(lo->valuestring, NULL) : 0);

        cJSON *f = gj_point_feature(lon, lat);

        cJSON *p = cJSON_CreateObject();         /* EXACT JS key order */
        cJSON *same = cJSON_GetObjectItem(r, "owl:sameAs");
        cJSON *atid = cJSON_GetObjectItem(r, "@id");
        if (same && cJSON_IsString(same) && same->valuestring[0])
          cJSON_AddStringToObject(p, "station_uid", same->valuestring);
        else if (atid && cJSON_IsString(atid) && atid->valuestring[0])
          cJSON_AddStringToObject(p, "station_uid", atid->valuestring);
        else
          cJSON_AddItemToObject(p, "station_uid", cJSON_CreateNull());
        title_pick(p, "station_name", r, "en");
        title_pick(p, "station_name_ja", r, "ja");
        jo_put_str_or_null(p, "railway", r, "odpt:railway");
        jo_put_str_or_null(p, "operator", r, "odpt:operator");
        cJSON_AddStringToObject(p, "source", "odpt_api");
        cJSON_AddItemToObject(f, "properties", p);
        cJSON_AddItemToArray(features, f);
      }
      cJSON_Delete(rows);
    }
  }

  if (cJSON_GetArraySize(features) > 0) {
    int n = geojson_emit_features(sink, ctx->source_id, features);
    cJSON_Delete(features);
    fprintf(stderr, "[odpt-station] emitted %d (odpt_api)\n", n);
    return n >= 0 ? 0 : -1;
  }
  cJSON_Delete(features);

  /* Real key-free fallback: every OSM rail station across Japan. */
  int n = overpass_collect(ctx, sink,
    "node[\"railway\"=\"station\"](area.jp);"
    "node[\"railway\"=\"halt\"](area.jp);",
    180, 200000, osm_map, NULL);
  fprintf(stderr, "[odpt-station] emitted %d (osm_overpass)\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def odpt_station_def = {
  .id = "odpt-station", .collector = "transport",
  .name = "ODPT Station Data", .name_ja = "ODPT 駅データ",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(odpt_station_def)
