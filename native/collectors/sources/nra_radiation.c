/* collectors/environment/sources/nra_radiation.c
 * Port of server/src/collectors/nraRadiation.js — PRIMARY path only
 * (tryNraJson over NRA_FEEDS). The OSM-station and curated-seed fallbacks
 * (tryOsmStations / generateSeedData) are intentionally NOT ported (RULE 8:
 * JSON-API primary, drop the Overpass/seed fallback). JSON_API family.
 * Registry id "nra-radiation", category "environment". */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define TIMEOUT_MS 10000

static const char *NRA_FEEDS[] = {
  "https://radioactivity.nra.go.jp/cont/json/dose_today.json",
  "https://radioactivity.nra.go.jp/cont/json/MP/MP_today.json",
  "https://www.kankyo-hoshano.go.jp/api/now_value.json",
  "https://emdb.jaea.go.jp/emdb/api/v1/monitoring_post/latest.json",
};
#define NFEEDS ((int)(sizeof(NRA_FEEDS) / sizeof(NRA_FEEDS[0])))

/* JS: Number(x) — coerce string/number; NaN if not finite. */
static int as_num(const cJSON *v, double *out) {
  if (!v) return 0;
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return isfinite(*out); }
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *end;
    double d = strtod(v->valuestring, &end);
    if (end != v->valuestring && isfinite(d)) { *out = d; return 1; }
  }
  return 0;
}

/* first present of a key list, else NULL (=== JS `a ?? b ?? c`) */
static const cJSON *first(const cJSON *o, const char *const *keys) {
  for (int i = 0; keys[i]; i++) {
    const cJSON *v = cJSON_GetObjectItem(o, keys[i]);
    if (v && !cJSON_IsNull(v)) return v;
  }
  return NULL;
}

static cJSON *dup_or_null(const cJSON *v) {
  return v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull();
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  for (int fi = 0; fi < NFEEDS; fi++) {
    cJSON *data = feed_get_json(ctx->http, NRA_FEEDS[fi], TIMEOUT_MS);
    if (!data) continue;

    /* arr = Array.isArray(data) ? data
     *       : (data.stations || data.data || data.results || data.items || []) */
    const cJSON *arr = NULL;
    if (cJSON_IsArray(data)) {
      arr = data;
    } else {
      static const char *ak[] = { "stations", "data", "results", "items", NULL };
      const cJSON *a = first(data, ak);
      if (a && cJSON_IsArray(a)) arr = a;
    }
    if (!arr || !cJSON_IsArray(arr) || cJSON_GetArraySize(arr) == 0) {
      cJSON_Delete(data);
      continue;
    }

    cJSON *features = cJSON_CreateArray();
    const cJSON *r;
    cJSON_ArrayForEach(r, arr) {
      static const char *latk[] = { "lat", "LAT", "緯度", NULL };
      static const char *lonk[] = { "lon", "lng", "LON", "経度", NULL };
      double lat = 0, lon = 0;
      int glat = as_num(first(r, latk), &lat);
      int glon = as_num(first(r, lonk), &lon);
      int geocoded = glat && glon;

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      if (geocoded) {
        cJSON *g = cJSON_CreateObject();
        cJSON_AddStringToObject(g, "type", "Point");
        cJSON *co = cJSON_CreateArray();
        cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
        cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
        cJSON_AddItemToObject(g, "coordinates", co);
        cJSON_AddItemToObject(f, "geometry", g);
      } else {
        cJSON_AddItemToObject(f, "geometry", cJSON_CreateNull());
      }

      cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
      static const char *idk[]  = { "id", "station_id", "code", NULL };
      static const char *nmk[]  = { "name", "station_name", "name_jp", NULL };
      static const char *prk[]  = { "pref", "prefecture", NULL };
      static const char *dsk[]  = { "value", "dose_rate", "gamma", NULL };
      static const char *mtk[]  = { "datetime", "measured_at", "time", NULL };
      cJSON_AddItemToObject(p, "station_id",   dup_or_null(first(r, idk)));
      cJSON_AddItemToObject(p, "station_name", dup_or_null(first(r, nmk)));
      cJSON_AddItemToObject(p, "prefecture",   dup_or_null(first(r, prk)));
      cJSON_AddItemToObject(p, "dose_rate_nGyh", dup_or_null(first(r, dsk)));
      cJSON_AddItemToObject(p, "measured_at",  dup_or_null(first(r, mtk)));
      cJSON_AddStringToObject(p, "source", "nra_live");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
    }
    cJSON_Delete(data);

    if (cJSON_GetArraySize(features) > 0) {
      int n = geojson_emit_features(sink, ctx->source_id, features);
      cJSON_Delete(features);
      fprintf(stderr, "[nra-radiation] emitted %d (feed %d)\n", n, fi);
      return n >= 0 ? 0 : -1;
    }
    cJSON_Delete(features);
  }
  fprintf(stderr, "[nra-radiation] no live feed\n");
  return -1;
}

static const source_def nra_radiation_def = {
  .id = "nra-radiation", .collector = "environment",
  .name = "NRA Radiation Monitoring",
  .name_ja = "原子力規制委員会 放射線モニタリング",
   .update_interval_sec = 600, .run = run };
REGISTER_SOURCE(nra_radiation_def)
