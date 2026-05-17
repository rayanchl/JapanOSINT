/* collectors/agriculture/sources/bear_encounters.c
 * Port of server/src/collectors/bearEncounters.js — prefectural bear
 * sighting (クマ出没情報) open data. No national feed, so the ONLY source
 * is the optional env var BEAR_ENCOUNTERS_GEOJSON_URL (Node: envFor →
 * native: getenv); GeoJSON FeatureCollection w/ defensive prop schema +
 * Japan bbox filter. No source / fetch fail / no rows → honest empty
 * (rule 8 — no fabricated sighting points). Property key order
 * (sighting_id, date, place, species, note, source) mirrors JS.
 * (No native wildlife/ dir — filed under agriculture per task rule.) */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* prop(p, keys): first key whose String()-trimmed value is non-empty. */
static const char *prop(cJSON *p, const char *const *keys, int nk) {
  for (int i = 0; i < nk; i++) {
    cJSON *v = cJSON_GetObjectItem(p, keys[i]);
    if (!v) continue;
    if (cJSON_IsString(v)) {
      const char *s = v->valuestring;
      while (*s == ' ') s++;
      if (*s) return v->valuestring;
    } else if (cJSON_IsNumber(v)) {
      return NULL; /* numeric: handled separately where needed */
    }
  }
  return NULL;
}
static int prop_num(cJSON *p, const char *const *keys, int nk, double *out) {
  for (int i = 0; i < nk; i++) {
    cJSON *v = cJSON_GetObjectItem(p, keys[i]);
    if (!v) continue;
    if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
    if (cJSON_IsString(v) && v->valuestring[0]) {
      char *e; double n = strtod(v->valuestring, &e);
      if (e != v->valuestring) { *out = n; return 1; }
    }
  }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = getenv("BEAR_ENCOUNTERS_GEOJSON_URL");
  if (!url || !*url) {
    fprintf(stderr, "[bear-encounters] no source (BEAR_ENCOUNTERS_GEOJSON_URL unset)\n");
    return -1;
  }
  cJSON *data = feed_get_json(ctx->http, url, 20000);
  cJSON *src = data ? cJSON_GetObjectItem(data, "features") : NULL;
  if (!src || !cJSON_IsArray(src)) {
    if (data) cJSON_Delete(data);
    fprintf(stderr, "[bear-encounters] unavailable\n");
    return -1;
  }

  static const char *LAT_K[] = { "lat","latitude","緯度" };
  static const char *LON_K[] = { "lon","lng","longitude","経度" };
  static const char *ID_K[]  = { "id","ID","番号" };
  static const char *DT_K[]  = { "date","日付","発生日時","目撃日時" };
  static const char *PL_K[]  = { "place","場所","市町村","address","住所" };
  static const char *SP_K[]  = { "species","種別","クマの種類" };
  static const char *NT_K[]  = { "note","備考","状況" };

  cJSON *features = cJSON_CreateArray();
  cJSON *f;
  cJSON_ArrayForEach(f, src) {
    double lon, lat;
    int have = 0;
    cJSON *geom = cJSON_GetObjectItem(f, "geometry");
    cJSON *gt = geom ? cJSON_GetObjectItem(geom, "type") : NULL;
    cJSON *gc = geom ? cJSON_GetObjectItem(geom, "coordinates") : NULL;
    if (gt && cJSON_IsString(gt) && strcmp(gt->valuestring, "Point") == 0 &&
        gc && cJSON_IsArray(gc) && cJSON_GetArraySize(gc) >= 2) {
      cJSON *c0 = cJSON_GetArrayItem(gc, 0);
      cJSON *c1 = cJSON_GetArrayItem(gc, 1);
      if (c0 && cJSON_IsNumber(c0) && c1 && cJSON_IsNumber(c1)) {
        lon = c0->valuedouble; lat = c1->valuedouble; have = 1;
      }
    }
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    if (!p) p = cJSON_CreateObject(), cJSON_AddItemToObject(f, "properties", p);
    if (!have) {
      if (!prop_num(p, LAT_K, 3, &lat)) continue;
      if (!prop_num(p, LON_K, 4, &lon)) continue;
      have = 1;
    }
    if (lat < 20 || lat > 46 || lon < 122 || lon > 154) continue;

    const char *sid = prop(p, ID_K, 3);
    char sidbuf[64];
    const char *dt = prop(p, DT_K, 4);
    if (!sid) {
      char hk[24]; char lonb[32], latb[32];
      snprintf(lonb, sizeof lonb, "%g", lon);
      snprintf(latb, sizeof latb, "%g", lat);
      const char *parts[] = { lonb, latb, dt ? dt : NULL };
      feed_hash_key(hk, parts, 3);
      snprintf(sidbuf, sizeof sidbuf, "BEAR_%s", hk);
      sid = sidbuf;
    }
    const char *plc = prop(p, PL_K, 5);
    const char *spc = prop(p, SP_K, 3);
    const char *nt  = prop(p, NT_K, 3);

    cJSON *nf = cJSON_CreateObject();
    cJSON_AddStringToObject(nf, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(nf, "geometry", g);

    cJSON *np = cJSON_CreateObject();             /* EXACT JS key order */
    cJSON_AddStringToObject(np, "sighting_id", sid);
    if (dt)  cJSON_AddStringToObject(np, "date", dt);  else cJSON_AddNullToObject(np, "date");
    if (plc) cJSON_AddStringToObject(np, "place", plc); else cJSON_AddNullToObject(np, "place");
    if (spc) cJSON_AddStringToObject(np, "species", spc); else cJSON_AddNullToObject(np, "species");
    if (nt)  cJSON_AddStringToObject(np, "note", nt);  else cJSON_AddNullToObject(np, "note");
    cJSON_AddStringToObject(np, "source", "bear_encounters");
    cJSON_AddItemToObject(nf, "properties", np);
    cJSON_AddItemToArray(features, nf);
  }
  cJSON_Delete(data);

  if (cJSON_GetArraySize(features) == 0) {
    cJSON_Delete(features);
    fprintf(stderr, "[bear-encounters] unavailable (no geo rows)\n");
    return -1;
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[bear-encounters] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def bear_encounters_def = {
  .id = "bear-encounters", .collector = "agriculture",
  .name = "Bear Encounters", .name_ja = "クマ出没情報",
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(bear_encounters_def)
