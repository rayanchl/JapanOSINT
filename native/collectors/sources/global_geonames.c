/* collectors/sources/global_geonames.c
 * OSINT service — GEONAMES. On-demand geospatial pivot (entity = place name)
 * against the GeoNames search API (api.geonames.org/searchJSON). Free, but the
 * API requires a registered username sent as ?username= — gated on the
 * GEONAMES_USER env var (no username → honest empty, like gbizinfo.c). One
 * intel_item per matched place, carrying name + lat/lng (has_geo=1) + country.
 * No matches / fetch failure → emits nothing. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* one geoname record → one intel_item. Returns 1 if emitted. */
static int emit_place(intel_sink *sink, const cJSON *g) {
  if (!g) return 0;
  const char *name    = jo_sv(g, "name");
  const char *toponym = jo_sv(g, "toponymName");
  const char *lat_s   = jo_sv(g, "lat");
  const char *lng_s   = jo_sv(g, "lng");
  const char *country = jo_sv(g, "countryName");
  const char *cc      = jo_sv(g, "countryCode");
  const char *admin1  = jo_sv(g, "adminName1");
  const char *fcodenm = jo_sv(g, "fcodeName");
  const char *fclnm   = jo_sv(g, "fclName");
  const cJSON *gid    = cJSON_GetObjectItem(g, "geonameId");
  const cJSON *pop    = cJSON_GetObjectItem(g, "population");
  const char *disp    = name ? name : toponym;
  if (!disp || (!lat_s && !lng_s)) return 0;

  int have_geo = 0; double lat = 0, lon = 0;
  if (lat_s && lng_s) { lat = atof(lat_s); lon = atof(lng_s); have_geo = 1; }

  char keybuf[32];
  const char *rkey = disp;
  if (gid && cJSON_IsNumber(gid)) {
    snprintf(keybuf, sizeof keybuf, "%d", gid->valueint);
    rkey = keybuf;
  }

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "name", disp);
  if (toponym) cJSON_AddStringToObject(data, "toponym", toponym);
  if (admin1)  cJSON_AddStringToObject(data, "admin1", admin1);
  if (country) cJSON_AddStringToObject(data, "country", country);
  if (cc)      cJSON_AddStringToObject(data, "country_code", cc);
  if (fcodenm) cJSON_AddStringToObject(data, "feature", fcodenm);
  if (fclnm)   cJSON_AddStringToObject(data, "feature_class", fclnm);
  if (have_geo) { cJSON_AddNumberToObject(data, "lat", lat);
                  cJSON_AddNumberToObject(data, "lng", lon); }
  if (pop && cJSON_IsNumber(pop)) cJSON_AddNumberToObject(data, "population", pop->valuedouble);
  cJSON_AddStringToObject(data, "source", "GeoNames");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "GEONAMES");
  cJSON_AddStringToObject(props, "source", "GeoNames");
  if (country) cJSON_AddStringToObject(props, "country", country);
  if (cc)      cJSON_AddStringToObject(props, "country_code", cc);
  if (have_geo) { cJSON_AddNumberToObject(props, "lat", lat);
                  cJSON_AddNumberToObject(props, "lng", lon); }
  cJSON_AddBoolToObject(props, "has_geo", have_geo);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = rkey;
  it.title           = disp;
  it.summary         = country;
  it.body            = bj;
  it.has_geo         = have_geo;
  it.lat             = lat;
  it.lon             = lon;
  it.record_type     = "geonames-place";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"GEONAMES\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *user = jo_env("GEONAMES_USER");
  if (!user) {
    fprintf(stderr, "[geonames] gated (no GEONAMES_USER)\n");
    return 0;
  }

  char *eq = jo_urlencode(q);
  char *eu = jo_urlencode(user);
  if (!eq || !eu) { free(eq); free(eu); return 0; }
  char url[1024];
  snprintf(url, sizeof url,
    "http://api.geonames.org/searchJSON?q=%s&maxRows=15&username=%s", eq, eu);
  free(eq); free(eu);

  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "geonames");
  if (!body) return 0;

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  cJSON *arr = cJSON_GetObjectItem(root, "geonames");
  int emitted = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *g; cJSON_ArrayForEach(g, arr) emitted += emit_place(sink, g);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[geonames] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def global_geonames_def = {
  .id = "GEONAMES", .collector = "osint",
  .name = "GeoNames Place Search", .name_ja = "GeoNames 地名検索",
  .update_interval_sec = 0, .run = run,
  .category = "geospatial", .type = "api",
  .url = "internal://osint/geonames",
  .description = "GeoNames geocoding search — place name to lat/lng/country (needs GEONAMES_USER)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(global_geonames_def)
