/* collectors/transport/sources/vessel_finder.c
 * Port of server/src/collectors/vesselFinder.js — PRIMARY tryApi() path.
 * VesselFinder Master REST API, gated on VESSELFINDER_API_KEY (0 rows when
 * unset). The OSM ferry-terminal fallback and the curated SEED_HUBS are
 * intentionally not ported (correctness-neutral). */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* JS: [lon,lat] = [parseFloat(v.LONGITUDE), parseFloat(v.LATITUDE)] */
static double pf(cJSON *r, const char *k) {
  cJSON *v = cJSON_GetObjectItem(r, k);
  if (!v) return 0.0;
  if (cJSON_IsNumber(v)) return v->valuedouble;
  if (cJSON_IsString(v)) return strtod(v->valuestring, NULL);
  return 0.0;
}

/* JS: `v.X` passthrough into properties — null when absent. */
static void passthru(cJSON *p, const char *outk, cJSON *r, const char *ink) {
  cJSON *v = cJSON_GetObjectItem(r, ink);
  cJSON_AddItemToObject(p, outk, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("VESSELFINDER_API_KEY");
  if (!key || !*key) { fprintf(stderr, "[vessel-finder] no API key\n"); return -1; }

  char url[256];
  snprintf(url, sizeof url,
    "https://api.vesselfinder.com/vesselslist?userkey=%s&bbox=122,24,154,46&format=json",
    key);
  cJSON *arr = feed_get_json(ctx->http, url, 10000);
  if (!arr || !cJSON_IsArray(arr)) { if (arr) cJSON_Delete(arr); return -1; }

  cJSON *features = cJSON_CreateArray();
  int i = 0;
  cJSON *v;
  cJSON_ArrayForEach(v, arr) {
    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(pf(v, "LONGITUDE")));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(pf(v, "LATITUDE")));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *p = cJSON_CreateObject();                 /* EXACT JS key order */
    cJSON *mmsi = cJSON_GetObjectItem(v, "MMSI");
    char id[64];
    if (mmsi && cJSON_IsNumber(mmsi))
      snprintf(id, sizeof id, "VF_%lld", (long long)mmsi->valuedouble);
    else if (mmsi && cJSON_IsString(mmsi) && *mmsi->valuestring)
      snprintf(id, sizeof id, "VF_%s", mmsi->valuestring);
    else
      snprintf(id, sizeof id, "VF_%d", i);
    cJSON_AddStringToObject(p, "id", id);

    passthru(p, "mmsi", v, "MMSI");
    passthru(p, "imo", v, "IMO");
    passthru(p, "vessel_name", v, "NAME");

    /* (v.TYPE || 'other').toString().toLowerCase() */
    cJSON *tp = cJSON_GetObjectItem(v, "TYPE");
    char tbuf[64]; const char *tsrc = "other";
    if (tp && cJSON_IsString(tp) && *tp->valuestring) tsrc = tp->valuestring;
    else if (tp && cJSON_IsNumber(tp)) {
      snprintf(tbuf, sizeof tbuf, "%g", tp->valuedouble); tsrc = tbuf;
    }
    char tl[64]; size_t j = 0;
    for (; tsrc[j] && j < sizeof tl - 1; j++) {
      char c = tsrc[j];
      tl[j] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
    }
    tl[j] = '\0';
    cJSON_AddStringToObject(p, "vessel_type", tl);

    passthru(p, "flag", v, "FLAG");
    passthru(p, "speed_knots", v, "SPEED");
    passthru(p, "heading", v, "COURSE");
    passthru(p, "destination", v, "DESTINATION");
    passthru(p, "length_m", v, "LENGTH");
    passthru(p, "last_position_update", v, "TIMESTAMP");
    cJSON_AddStringToObject(p, "country", "JP");
    cJSON_AddStringToObject(p, "source", "vesselfinder_api");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
    i++;
  }
  cJSON_Delete(arr);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[vessel-finder] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def vessel_finder_def = {
  .id = "vessel-finder", .collector = "transport",
  .name = "VesselFinder AIS", .name_ja = "ベッセルファインダー AIS",
   .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(vessel_finder_def)
