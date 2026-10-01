/* collectors/environment/sources/soramame.c
 * Port of server/src/collectors/soramame.js.
 * Live cascade (rule 7 — generateSeedData not ported):
 *   1. tryPrefScan: Pref01..Pref47.json (keyless)
 *   2. tryAtmosJson: ATMOS_URLS list
 *   3. tryOsmStations: OSM Overpass monitoring_station
 * First non-empty wins. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include "lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Number(r?.a ?? r?.b ?? r?.c) → finite double, or NAN. */
static int num_of(cJSON *r, const char *a, const char *b, const char *c,
                  double *out) {
  const char *ks[3] = { a, b, c };
  for (int i = 0; i < 3; i++) {
    cJSON *v = cJSON_GetObjectItem(r, ks[i]);
    if (!v) continue;
    if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
    if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
      char *end; double d = strtod(v->valuestring, &end);
      if (end != v->valuestring) { *out = d; return 1; }
    }
  }
  return 0;
}

/* r?.x ?? r?.y → cJSON value duplicate, or JSON null. */
static cJSON *pick2(cJSON *r, const char *x, const char *y) {
  cJSON *v = cJSON_GetObjectItem(r, x);
  if (v && !cJSON_IsNull(v)) return cJSON_Duplicate(v, 1);
  v = cJSON_GetObjectItem(r, y);
  if (v && !cJSON_IsNull(v)) return cJSON_Duplicate(v, 1);
  return cJSON_CreateNull();
}
static cJSON *pick1(cJSON *r, const char *x) {
  cJSON *v = cJSON_GetObjectItem(r, x);
  return (v && !cJSON_IsNull(v)) ? cJSON_Duplicate(v, 1) : cJSON_CreateNull();
}

/* String(r?.測定局コード ?? r?.code ?? '') */
static void str_id(cJSON *r, const char *a, const char *b, char *out,
                   size_t n) {
  cJSON *v = cJSON_GetObjectItem(r, a);
  if (!(v && !cJSON_IsNull(v))) v = cJSON_GetObjectItem(r, b);
  if (v && cJSON_IsString(v) && v->valuestring) {
    snprintf(out, n, "%s", v->valuestring);
  } else if (v && cJSON_IsNumber(v)) {
    snprintf(out, n, "%lld", (long long)v->valuedouble);
  } else {
    out[0] = '\0';
  }
}

/* Build the pref-scan Feature (JS prop order). */
static cJSON *pref_feature(cJSON *r) {
  double lat, lon;
  int geo = num_of(r, "緯度", "lat", "LAT", &lat) &&
            num_of(r, "経度", "lon", "LON", &lon);
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  if (geo) {
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *c = cJSON_CreateArray();
    cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(g, "coordinates", c);
    cJSON_AddItemToObject(f, "geometry", g);
  } else {
    cJSON_AddItemToObject(f, "geometry", cJSON_CreateNull());
  }
  cJSON *p = cJSON_CreateObject();
  char sid[64];
  str_id(r, "測定局コード", "code", sid, sizeof sid);
  cJSON_AddStringToObject(p, "station_id", sid);
  cJSON_AddItemToObject(p, "station_name", pick2(r, "測定局名称", "name"));
  cJSON_AddItemToObject(p, "prefecture", pick1(r, "都道府県名"));
  cJSON_AddItemToObject(p, "pm25_ugm3", pick1(r, "PM25"));
  cJSON_AddItemToObject(p, "pm10_ugm3", pick1(r, "SPM"));
  cJSON_AddItemToObject(p, "so2_ppb", pick1(r, "SO2"));
  cJSON_AddItemToObject(p, "no2_ppb", pick1(r, "NO2"));
  cJSON_AddItemToObject(p, "ox_ppb", pick1(r, "OX"));
  cJSON_AddItemToObject(p, "co_ppm", pick1(r, "CO"));
  cJSON_AddItemToObject(p, "measured_at", pick1(r, "測定時刻"));
  cJSON_AddStringToObject(p, "source", "soramame_live");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static cJSON *atmos_feature(cJSON *r) {
  cJSON *geom = cJSON_GetObjectItem(r, "geometry");
  cJSON *gcoords = geom ? cJSON_GetObjectItem(geom, "coordinates") : NULL;
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  if (gcoords) {
    cJSON_AddItemToObject(f, "geometry", cJSON_Duplicate(geom, 1));
    cJSON *p = cJSON_CreateObject();
    cJSON *props = cJSON_GetObjectItem(r, "properties");
    if (cJSON_IsObject(props)) {
      cJSON *kv;
      cJSON_ArrayForEach(kv, props) {
        if (kv->string && !strcmp(kv->string, "source")) continue;
        cJSON_AddItemToObject(p, kv->string, cJSON_Duplicate(kv, 1));
      }
    }
    cJSON_AddStringToObject(p, "source", "soramame_live");
    cJSON_AddItemToObject(f, "properties", p);
    return f;
  }
  double lat, lon;
  int geo = num_of(r, "lat", "LAT", "緯度", &lat) &&
            num_of(r, "lon", "LON", "経度", &lon);
  if (geo) {
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *c = cJSON_CreateArray();
    cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(g, "coordinates", c);
    cJSON_AddItemToObject(f, "geometry", g);
  } else {
    cJSON_AddItemToObject(f, "geometry", cJSON_CreateNull());
  }
  cJSON *p = cJSON_CreateObject();
  cJSON_AddItemToObject(p, "station_id", pick2(r, "station_id", "code"));
  cJSON_AddItemToObject(p, "station_name", pick2(r, "name", "station_name"));
  cJSON_AddItemToObject(p, "prefecture", pick1(r, "pref"));
  cJSON_AddItemToObject(p, "pm25_ugm3", pick1(r, "pm25"));
  cJSON_AddStringToObject(p, "source", "soramame_live");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static cJSON *osm_map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)i; (void)ud;
  cJSON *f = gj_point_feature(lon, lat);
  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char sid[48];
  snprintf(sid, sizeof sid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "station_id", sid);
  const char *nm = ov_tag(el, "name:en");
  if (!nm) nm = ov_tag(el, "name");
  cJSON_AddStringToObject(p, "station_name",
                          nm ? nm : "Air monitoring station");
  const char *op = ov_tag(el, "operator");
  if (op) cJSON_AddStringToObject(p, "operator", op);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  const char *ref = ov_tag(el, "ref");
  if (ref) cJSON_AddStringToObject(p, "ref", ref);
  else cJSON_AddItemToObject(p, "ref", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static const char *ATMOS_URLS[] = {
  "https://soramame.env.go.jp/data/map/kyokuLatest/Pref01.json",
  "https://soramame.env.go.jp/data/jsons/japan/atmos.json",
  "https://tenbou.nies.go.jp/download/Air_pollution/json/StationLatest.json",
  NULL
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* 1. tryPrefScan: Pref01..Pref47.json */
  cJSON *features = cJSON_CreateArray();
  for (int pi = 1; pi <= 47; pi++) {
    char url[160];
    snprintf(url, sizeof url,
      "https://soramame.env.go.jp/data/map/kyokuLatest/Pref%02d.json", pi);
    cJSON *data = feed_get_json(ctx->http, url, 10000);
    if (data && cJSON_IsArray(data)) {
      cJSON *r;
      cJSON_ArrayForEach(r, data)
        cJSON_AddItemToArray(features, pref_feature(r));
    }
    if (data) cJSON_Delete(data);
  }
  if (cJSON_GetArraySize(features) > 0) {
    int n = geojson_emit_features(sink, ctx->source_id, features);
    cJSON_Delete(features);
    fprintf(stderr, "[soramame] pref-scan emitted %d\n", n);
    return n >= 0 ? 0 : -1;
  }
  cJSON_Delete(features);

  /* 2. tryAtmosJson */
  for (int u = 0; ATMOS_URLS[u]; u++) {
    cJSON *data = feed_get_json(ctx->http, ATMOS_URLS[u], 10000);
    if (!data) continue;
    cJSON *arr = NULL;
    if (cJSON_IsArray(data)) arr = data;
    else {
      const char *keys[] = { "features", "stations", "list", "data" };
      for (int k = 0; k < 4 && !arr; k++) {
        cJSON *a = cJSON_GetObjectItem(data, keys[k]);
        if (cJSON_IsArray(a)) arr = a;
      }
    }
    if (!cJSON_IsArray(arr) || cJSON_GetArraySize(arr) == 0) {
      cJSON_Delete(data);
      continue;
    }
    cJSON *feats = cJSON_CreateArray();
    cJSON *r;
    cJSON_ArrayForEach(r, arr)
      cJSON_AddItemToArray(feats, atmos_feature(r));
    cJSON_Delete(data);
    if (cJSON_GetArraySize(feats) > 0) {
      int n = geojson_emit_features(sink, ctx->source_id, feats);
      cJSON_Delete(feats);
      fprintf(stderr, "[soramame] atmos emitted %d\n", n);
      return n >= 0 ? 0 : -1;
    }
    cJSON_Delete(feats);
  }

  /* 3. tryOsmStations */
  int n = overpass_collect(ctx, sink,
    "node[\"man_made\"=\"monitoring_station\"][\"monitoring:air_quality\"=\"yes\"](area.jp);"
    "node[\"man_made\"=\"monitoring_station\"][\"monitoring:weather\"=\"yes\"](area.jp);",
    120, 60000, osm_map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def soramame_def = {
  .id = "soramame", .collector = "environment",
  .name = "SORAMAME Air Quality", .name_ja = "そらまめ君 大気汚染情報",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(soramame_def)
