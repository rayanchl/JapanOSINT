/* lib/mlit_ksj.c — see header. Faithful port of mlitNormalizer.js. */
#include "mlit_ksj.h"
#include "feedlib.h"
#include "geojson.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── KSJ_CONFIG ─────────────────────────────────────────────────────────── */
typedef struct { const char *key; const char *aliases[6]; } ksj_field;
typedef enum { GEO_POINT, GEO_MIDPOINT, GEO_CENTROID, GEO_PRESERVE } ksj_geo;
typedef struct {
  const char *code, *id_prefix, *source_tag, *id_field;
  ksj_geo geometry;
  ksj_field fields[6];   /* key=NULL terminates */
  /* extras (post-norm computed keys), in JS Object.entries order, with the
   * *_field metadata key already stripped: */
  const char *extra_name_ja;   /* "name_ja" => value := norm.name, or NULL */
  const char *extra_status;    /* "status"  => closed_year?abolished:active */
  const char *extra_route_type;/* "route_type" => norm.route_type||"bus" */
} ksj_cfg;

static const ksj_cfg KSJ[] = {
  { "N02","MLIT_N02","mlit_n02","station_id", GEO_MIDPOINT, {
      { "name", {"stationName","station_name","name","N02_005",NULL} },
      { "line", {"railwayLineName","line_name","line","N02_003",NULL} },
      { "operator", {"operatorCompany","operator","company","N02_004",NULL} },
      { "classification", {"railwayType","N02_001",NULL} },
      { "institution_type", {"institutionType","N02_002",NULL} },
      { NULL,{NULL} } }, "name_ja", NULL, NULL },
  { "P02","MLIT_P02","mlit_p02","airport_id", GEO_CENTROID, {
      { "name", {"airportName","name","P02_004","P02_001",NULL} },
      { "icao", {"icao","P02_002",NULL} },
      { "iata", {"iata","P02_003",NULL} },
      { "classification", {"airportType","P02_005",NULL} },
      { NULL,{NULL} } }, NULL, NULL, NULL },
  { "N05","MLIT_N05","mlit_n05","segment_id", GEO_MIDPOINT, {
      { "line", {"lineName","N05_002",NULL} },
      { "operator", {"operatorCompany","operator","N05_003",NULL} },
      { "classification", {"railwayType","N05_001",NULL} },
      { "opened_year", {"openedYear","N05_004",NULL} },
      { "closed_year", {"closedYear","N05_005",NULL} },
      { NULL,{NULL} } }, NULL, "status", NULL },
  { "N07","MLIT_N07","mlit_n07","route_id", GEO_PRESERVE, {
      { "name", {"routeName","route_name","name","N07_002",NULL} },
      { "operator", {"operatorName","operator","company","N07_003",NULL} },
      { "route_type", {"routeType","N07_001",NULL} },
      { NULL,{NULL} } }, NULL, NULL, "route_type" },
  { "P11","MLIT_P11","mlit_p11","stop_id", GEO_PRESERVE, {
      { "name", {"busStopName","stop_name","name","P11_001",NULL} },
      { "operator", {"operator","company","busOperator","P11_002",NULL} },
      { "route", {"routeName","P11_003",NULL} },
      { NULL,{NULL} } }, "name_ja", NULL, NULL },
  { "C02","MLIT_C02","mlit_c02","port_id", GEO_CENTROID, {
      { "name", {"portName","name","C02_003","C02_001",NULL} },
      { "classification", {"portClass","classification","C02_002",NULL} },
      { "administrator", {"administrator","C02_004",NULL} },
      { NULL,{NULL} } }, "name_ja", NULL, NULL },
};

static const ksj_cfg *ksj_find(const char *code) {
  for (size_t i = 0; i < sizeof KSJ / sizeof KSJ[0]; i++)
    if (!strcmp(KSJ[i].code, code)) return &KSJ[i];
  return NULL;
}

/* JS pickField: first alias whose value is != null && !== '' */
static cJSON *pick_field(cJSON *props, const char *const *aliases) {
  for (int i = 0; aliases[i]; i++) {
    cJSON *v = cJSON_GetObjectItemCaseSensitive(props, aliases[i]);
    if (!v || cJSON_IsNull(v)) continue;
    if (cJSON_IsString(v) && (!v->valuestring || !v->valuestring[0])) continue;
    return v;
  }
  return NULL;
}

/* geometryToPoint(geom,mode) → writes lon,lat; returns 1 on success. */
static int geom_to_point(cJSON *geom, ksj_geo mode, double *lon, double *lat) {
  if (!geom) return 0;
  cJSON *t = cJSON_GetObjectItemCaseSensitive(geom, "type");
  cJSON *c = cJSON_GetObjectItemCaseSensitive(geom, "coordinates");
  if (!cJSON_IsString(t) || !cJSON_IsArray(c)) return 0;
  const char *ty = t->valuestring;

  if (!strcmp(ty, "Point")) {
    cJSON *x = cJSON_GetArrayItem(c, 0), *y = cJSON_GetArrayItem(c, 1);
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y)) return 0;
    *lon = x->valuedouble; *lat = y->valuedouble; return 1;
  }
  if (mode == GEO_MIDPOINT) {
    cJSON *line = NULL;
    if (!strcmp(ty, "LineString")) line = c;
    else if (!strcmp(ty, "MultiLineString")) line = cJSON_GetArrayItem(c, 0);
    int n = line ? cJSON_GetArraySize(line) : 0;
    if (n <= 0) return 0;
    cJSON *pt = cJSON_GetArrayItem(line, n / 2);            /* floor(len/2) */
    cJSON *x = cJSON_GetArrayItem(pt, 0), *y = cJSON_GetArrayItem(pt, 1);
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y)) return 0;
    *lon = x->valuedouble; *lat = y->valuedouble; return 1;
  }
  if (mode == GEO_CENTROID) {
    cJSON *ring = NULL;
    if (!strcmp(ty, "Polygon")) ring = cJSON_GetArrayItem(c, 0);
    else if (!strcmp(ty, "MultiPolygon"))
      ring = cJSON_GetArrayItem(cJSON_GetArrayItem(c, 0), 0);
    int n = ring ? cJSON_GetArraySize(ring) : 0;
    if (n <= 0) return 0;
    double sx = 0, sy = 0;
    for (int i = 0; i < n; i++) {
      cJSON *p = cJSON_GetArrayItem(ring, i);
      sx += cJSON_GetArrayItem(p, 0)->valuedouble;
      sy += cJSON_GetArrayItem(p, 1)->valuedouble;
    }
    *lon = sx / n; *lat = sy / n; return 1;
  }
  return 0;
}

/* Set/overwrite a string-or-original-typed property, preserving JS spread
 * semantics: if key already present its value updates in place (position
 * stays); else it is appended. */
static void prop_set(cJSON *p, const char *k, cJSON *val /*takes ownership*/) {
  if (cJSON_GetObjectItemCaseSensitive(p, k))
    cJSON_ReplaceItemInObjectCaseSensitive(p, k, val);
  else
    cJSON_AddItemToObject(p, k, val);
}

/* normalizeKsjFeature(code,f,i) → Feature cJSON, or NULL to drop. */
static cJSON *normalize(const ksj_cfg *cfg, cJSON *f, int i) {
  if (!f) return NULL;
  cJSON *geom = cJSON_GetObjectItemCaseSensitive(f, "geometry");
  if (!geom || cJSON_IsNull(geom)) return NULL;
  cJSON *props = cJSON_GetObjectItemCaseSensitive(f, "properties");

  /* resolve norm fields (alias chain) */
  cJSON *resolved[6] = {0};
  for (int k = 0; cfg->fields[k].key; k++)
    resolved[k] = pick_field(props, cfg->fields[k].aliases);

  cJSON *geometry;
  if (cfg->geometry == GEO_PRESERVE) {
    geometry = cJSON_Duplicate(geom, 1);
  } else {
    double lon, lat;
    if (!geom_to_point(geom, cfg->geometry, &lon, &lat)) return NULL;
    geometry = cJSON_CreateObject();
    cJSON_AddStringToObject(geometry, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(geometry, "coordinates", co);
  }

  cJSON *feat = cJSON_CreateObject();
  cJSON_AddStringToObject(feat, "type", "Feature");
  cJSON_AddItemToObject(feat, "geometry", geometry);

  cJSON *p = cJSON_CreateObject();
  char idv[64];
  snprintf(idv, sizeof idv, "%s_%d", cfg->id_prefix, i);
  cJSON_AddStringToObject(p, cfg->id_field, idv);            /* [idField] */

  const char *norm_name = NULL;                              /* ...norm */
  for (int k = 0; cfg->fields[k].key; k++) {
    const char *key = cfg->fields[k].key;
    cJSON *v = resolved[k] ? cJSON_Duplicate(resolved[k], 1)
                           : cJSON_CreateNull();
    cJSON_AddItemToObject(p, key, v);
    if (!strcmp(key, "name") && cJSON_IsString(v)) norm_name = v->valuestring;
  }

  /* ...extras (Object.entries order; *_field stripped already) */
  if (cfg->extra_name_ja)
    prop_set(p, "name_ja", norm_name ? cJSON_CreateString(norm_name)
                                     : cJSON_CreateNull());
  if (cfg->extra_status) {
    cJSON *cy = cJSON_GetObjectItemCaseSensitive(p, "closed_year");
    int abolished = cy && !cJSON_IsNull(cy) &&
                    !(cJSON_IsString(cy) && !cy->valuestring[0]);
    prop_set(p, "status", cJSON_CreateString(abolished ? "abolished"
                                                       : "active"));
  }
  if (cfg->extra_route_type) {
    cJSON *rt = cJSON_GetObjectItemCaseSensitive(p, "route_type");
    int has = rt && cJSON_IsString(rt) && rt->valuestring[0];
    prop_set(p, "route_type",
             cJSON_CreateString(has ? rt->valuestring : "bus"));
  }
  cJSON_AddStringToObject(p, "source", cfg->source_tag);
  cJSON_AddItemToObject(feat, "properties", p);
  return feat;
}

static cJSON *try_url(http_client *http, const char *url, const ksj_cfg *cfg,
                      int abolished_only) {
  cJSON *data = feed_get_json(http, url, 30000);
  if (!data) return NULL;
  cJSON *raw = cJSON_IsArray(data) ? data
             : cJSON_GetObjectItemCaseSensitive(data, "features");
  if (!cJSON_IsArray(raw)) { cJSON_Delete(data); return NULL; }

  cJSON *out = cJSON_CreateArray();
  int i = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, raw) {
    cJSON *nf = normalize(cfg, f, i++);
    if (!nf) continue;
    if (abolished_only) {
      cJSON *pp = cJSON_GetObjectItemCaseSensitive(nf, "properties");
      cJSON *st = cJSON_GetObjectItemCaseSensitive(pp, "status");
      if (!cJSON_IsString(st) || strcmp(st->valuestring, "abolished")) {
        cJSON_Delete(nf); continue;
      }
    }
    cJSON_AddItemToArray(out, nf);
  }
  cJSON_Delete(data);
  if (cJSON_GetArraySize(out) > 0) return out;
  cJSON_Delete(out);
  return NULL;                                  /* JS: length>0 ? : null */
}

int mlit_ksj_collect(const source_ctx *ctx, intel_sink *sink,
                      const char *code, const char *env_key,
                      const char *const *mirrors, int n_mirrors,
                      int abolished_only) {
  const ksj_cfg *cfg = ksj_find(code);
  if (!cfg) { fprintf(stderr, "[mlit] unknown code %s\n", code); return -1; }

  cJSON *features = NULL;
  const char *env_url = env_key ? getenv(env_key) : NULL;
  if (env_url && *env_url)
    features = try_url(ctx->http, env_url, cfg, abolished_only);
  for (int m = 0; !features && m < n_mirrors; m++)
    features = try_url(ctx->http, mirrors[m], cfg, abolished_only);

  if (!features) features = cJSON_CreateArray();   /* live=false → 0 rows */
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[%s] emitted %d\n", ctx->source_id, n);
  return n >= 0 ? n : -1;
}
