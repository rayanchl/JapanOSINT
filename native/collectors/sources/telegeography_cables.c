/* collectors/infrastructure/sources/telegeography_cables.c
 * Port of server/src/collectors/teleGeographyCables.js. Two GeoJSON
 * endpoints (landing points + cables), JP-bbox filter, intel rows.
 * uid = telegeography-cables|landing|<id||name>  /  |cable|<id||name>
 * (== intelUid(SOURCE_ID, "landing|"+...)). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../core/intel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LANDING_URL "https://www.submarinecablemap.com/api/v3/landing-point/landing-point-geo.json"
#define CABLE_URL   "https://www.submarinecablemap.com/api/v3/cable/cable-geo.json"

static int in_jp(double lon, double lat) {
  return lat >= 24 && lat <= 46 && lon >= 122 && lon <= 154;
}
static const char *pstr(cJSON *p, const char *k) {
  cJSON *v = p ? cJSON_GetObjectItem(p, k) : NULL;
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static int emit_one(intel_sink *s, const char *prefix, cJSON *props,
                    double lat, double lon, int have_ll, cJSON *geom) {
  const char *id = pstr(props, "id");
  const char *nm = pstr(props, "name");
  if (!id && !nm) return 0;
  char rk[400]; snprintf(rk, sizeof rk, "%s|%s", prefix, id ? id : nm);
  char title[300], summ[360];
  if (nm) snprintf(title, sizeof title, "%s", nm);
  else snprintf(title, sizeof title, "%s %s",
                strcmp(prefix,"landing")==0?"Landing point":"Cable", id?id:"");
  if (strcmp(prefix, "landing") == 0)
    snprintf(summ, sizeof summ, "Submarine cable landing — %s", nm ? nm : "");
  else {
    const char *rfs = pstr(props, "rfs");
    snprintf(summ, sizeof summ, "Submarine cable touching JP — %s (RFS %s)",
             nm ? nm : "", rfs ? rfs : "?");
  }
  /* properties = { kind, lat, lon, ...orig } */
  cJSON *pj = props ? cJSON_Duplicate(props, 1) : cJSON_CreateObject();
  cJSON_DeleteItemFromObject(pj, "kind");
  cJSON_AddStringToObject(pj, "kind",
      strcmp(prefix,"landing")==0 ? "landing" : "cable");
  if (have_ll) {
    cJSON_DeleteItemFromObject(pj, "lat"); cJSON_DeleteItemFromObject(pj, "lon");
    cJSON_AddNumberToObject(pj, "lat", lat);
    cJSON_AddNumberToObject(pj, "lon", lon);
  }
  char *pjs = cJSON_PrintUnformatted(pj);

  /* A cable is a ROUTE. The upstream feature carries the full
   * MultiLineString, but only its first vertex was kept (as lat/lon), so the
   * map could never draw the cable — 142 rows, 0 with geometry. Carry the
   * real geometry through. */
  char *gjs = NULL;
  if (geom && cJSON_GetObjectItem(geom, "coordinates"))
    gjs = cJSON_PrintUnformatted(geom);

  /* The API exposes no "url" key, so every row persisted with link=NULL and
   * no way back to the source. submarinecablemap.com routes both record kinds
   * by the very id we already have (verified 200 for both patterns). */
  char lk[512];
  const char *link = pstr(props, "url");
  if (!link && id) {
    snprintf(lk, sizeof lk, "https://www.submarinecablemap.com/%s/%s",
             strcmp(prefix, "landing") == 0 ? "landing-point" : "submarine-cable",
             id);
    link = lk;
  }

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.summary = summ;
  it.link = link;
  it.lang = "en";
  it.record_type = "telegeography-cables";
  it.has_geo = have_ll; it.lat = lat; it.lon = lon;
  it.geometry_geojson = gjs;
  it.properties_json = pjs;
  it.tags_json = strcmp(prefix,"landing")==0
    ? "[\"submarine-cable\",\"landing\",\"telegeography\"]"
    : "[\"submarine-cable\",\"cable\",\"telegeography\"]";
  int rc = s->emit(s, &it);
  free(pjs); free(gjs); cJSON_Delete(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = 0, fetched = 0;
  cJSON *L = feed_get_json(ctx->http, LANDING_URL, 12000);
  if (L) {
    fetched++;
    cJSON *fs = cJSON_GetObjectItem(L, "features"), *f;
    cJSON_ArrayForEach(f, fs) {
      cJSON *g = cJSON_GetObjectItem(f, "geometry");
      cJSON *c = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
      cJSON *x = c ? cJSON_GetArrayItem(c, 0) : NULL;
      cJSON *y = c ? cJSON_GetArrayItem(c, 1) : NULL;
      if (!x || !y || !cJSON_IsNumber(x) || !cJSON_IsNumber(y)) continue;
      if (!in_jp(x->valuedouble, y->valuedouble)) continue;
      n += emit_one(sink, "landing", cJSON_GetObjectItem(f, "properties"),
                    y->valuedouble, x->valuedouble, 1, NULL);
    }
    cJSON_Delete(L);
  }
  cJSON *C = feed_get_json(ctx->http, CABLE_URL, 12000);
  if (C) {
    fetched++;
    cJSON *fs = cJSON_GetObjectItem(C, "features"), *f;
    cJSON_ArrayForEach(f, fs) {
      cJSON *g = cJSON_GetObjectItem(f, "geometry");
      cJSON *ty = g ? cJSON_GetObjectItem(g, "type") : NULL;
      cJSON *co = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
      if (!ty || !co) continue;
      int multi = strcmp(ty->valuestring, "MultiLineString") == 0;
      int line  = strcmp(ty->valuestring, "LineString") == 0;
      if (!multi && !line) continue;
      int touches = 0; double f0lon = 0, f0lat = 0; int got0 = 0;
      cJSON *segs = multi ? co : NULL;
      cJSON *seg;
      #define SCAN(SEG) do { cJSON *pt; cJSON_ArrayForEach(pt, (SEG)) { \
          cJSON *a=cJSON_GetArrayItem(pt,0),*b=cJSON_GetArrayItem(pt,1); \
          if(a&&b&&cJSON_IsNumber(a)&&cJSON_IsNumber(b)){ \
            if(!got0){f0lon=a->valuedouble;f0lat=b->valuedouble;got0=1;} \
            if(in_jp(a->valuedouble,b->valuedouble)){touches=1;break;} } } } while(0)
      if (multi) { cJSON_ArrayForEach(seg, segs) { SCAN(seg); if (touches) break; } }
      else { SCAN(co); }
      #undef SCAN
      if (!touches) continue;
      n += emit_one(sink, "cable", cJSON_GetObjectItem(f, "properties"),
                    f0lat, f0lon, got0, g);
    }
    cJSON_Delete(C);
  }
  fprintf(stderr, "[telegeography-cables] emitted %d (endpoints ok %d/2)\n",
          n, fetched);
  /* Only a total fetch failure is an error; "both files fetched, nothing
   * inside the JP bbox" must be an honest empty or the scheduler's anomaly
   * detector quarantines the source. */
  return fetched ? 0 : -1;
}

static const source_def telegeography_cables_def = {
  .id = "telegeography-cables", .collector = "infrastructure",
  .name = "TeleGeography Submarine Cables", .name_ja = "TeleGeography 海底ケーブル",
   .update_interval_sec = 86400, .run = run,
};
REGISTER_SOURCE(telegeography_cables_def)
