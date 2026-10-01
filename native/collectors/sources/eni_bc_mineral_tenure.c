/* British Columbia mineral and placer tenure register (BC Data Catalogue WFS).
 * Endpoint: https://openmaps.gov.bc.ca/geo/pub/wfs?service=WFS&version=2.0.0
 *   &request=GetFeature&typeName=WHSE_MINERAL_TENURE.MTA_ACQUIRED_TENURE_SVW
 *   &count=500&outputFormat=application/json&srsName=EPSG:4326      (keyless)
 * Emits one row per active claim: tenure number, claim name, tenure type/
 * sub-type, title type, issue and expiry dates, area, and the claim POLYGON as
 * geometry_geojson plus a representative point from the polygon's own vertices.
 *
 * PROJECTION TRAP, quoted from the source survey: "srsName=EPSG:4326 IS
 * MANDATORY: without it the geometry comes back in BC Albers metres
 * (EPSG:3005, coordinates like 1256346.655 / 833380.496) which, emitted as
 * lat/lon, would place every claim off the coast of Africa." The srsName is in
 * the URL, and every coordinate is additionally range-checked to degrees before
 * it is emitted — a metre-valued coordinate is rejected, never wrapped.
 * Licence: Open Government Licence - British Columbia. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "bc-mineral-tenure"

static void addstr(cJSON *p, const cJSON *o, const char *k, const char *out) {
  const char *s = jo_sv(o, k);
  if (s) cJSON_AddStringToObject(p, out, s);
}

/* Mean of the outer-ring vertices of the first polygon, in DEGREES. Returns 0
 * unless every visited coordinate is a plausible lat/lon — a BC Albers metre
 * value (|x| in the millions) fails the check and yields no geometry. */
static int poly_point(const cJSON *geom, double *olat, double *olon) {
  if (!geom) return 0;
  const char *t = jo_sv(geom, "type");
  cJSON *co = cJSON_GetObjectItem(geom, "coordinates");
  if (!t || !cJSON_IsArray(co)) return 0;
  cJSON *ring = NULL;
  if (strcmp(t, "Polygon") == 0) ring = cJSON_GetArrayItem(co, 0);
  else if (strcmp(t, "MultiPolygon") == 0) {
    cJSON *poly = cJSON_GetArrayItem(co, 0);
    ring = poly ? cJSON_GetArrayItem(poly, 0) : NULL;
  } else if (strcmp(t, "Point") == 0) {
    cJSON *x = cJSON_GetArrayItem(co, 0), *y = cJSON_GetArrayItem(co, 1);
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y)) return 0;
    *olon = x->valuedouble; *olat = y->valuedouble;
    return (*olat >= -90 && *olat <= 90 && *olon >= -180 && *olon <= 180);
  }
  if (!cJSON_IsArray(ring)) return 0;
  double sx = 0, sy = 0; int m = 0;
  cJSON *pt;
  cJSON_ArrayForEach(pt, ring) {
    if (!cJSON_IsArray(pt) || cJSON_GetArraySize(pt) < 2) continue;
    cJSON *x = cJSON_GetArrayItem(pt, 0), *y = cJSON_GetArrayItem(pt, 1);
    if (!cJSON_IsNumber(x) || !cJSON_IsNumber(y)) continue;
    double lon = x->valuedouble, lat = y->valuedouble;
    /* degrees, not BC Albers metres */
    if (lat < -90 || lat > 90 || lon < -180 || lon > 180) return 0;
    sx += lon; sy += lat; m++;
  }
  if (m == 0) return 0;
  *olon = sx / m; *olat = sy / m;
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url =
    "https://openmaps.gov.bc.ca/geo/pub/wfs?service=WFS&version=2.0.0"
    "&request=GetFeature&typeName=WHSE_MINERAL_TENURE.MTA_ACQUIRED_TENURE_SVW"
    "&count=500&outputFormat=application/json&srsName=EPSG:4326";
  cJSON *doc = feed_get_json(ctx->http, url, 60000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  if (!cJSON_IsArray(feats)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no features[]\n"); return -1; }

  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    cJSON *tn = cJSON_GetObjectItem(pr, "TENURE_NUMBER_ID");
    char tenure[48] = {0};
    if (cJSON_IsNumber(tn)) snprintf(tenure, sizeof tenure, "%lld", (long long)tn->valuedouble);
    else if (cJSON_IsString(tn)) snprintf(tenure, sizeof tenure, "%s", tn->valuestring);
    if (!tenure[0]) continue;
    const char *claim = jo_sv(pr, "CLAIM_NAME");

    cJSON *geom = cJSON_GetObjectItem(f, "geometry");
    double lat = 0, lon = 0;
    int has_geo = poly_point(geom, &lat, &lon);
    char *gj = (has_geo && geom) ? cJSON_PrintUnformatted(geom) : NULL;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "tenure_number_id", tenure);
    if (claim) cJSON_AddStringToObject(p, "claim_name", claim);
    addstr(p, pr, "TENURE_TYPE_DESCRIPTION", "tenure_type");
    addstr(p, pr, "TENURE_SUB_TYPE_DESCRIPTION", "tenure_sub_type");
    addstr(p, pr, "TITLE_TYPE_CODE", "title_type_code");
    addstr(p, pr, "ISSUE_DATE", "issue_date");
    addstr(p, pr, "GOOD_TO_DATE", "good_to_date");
    addstr(p, pr, "OWNER_NAME", "owner_name");
    cJSON *area = cJSON_GetObjectItem(pr, "AREA_IN_HECTARES");
    if (cJSON_IsNumber(area)) {
      cJSON_AddNumberToObject(p, "area", area->valuedouble);
      cJSON_AddStringToObject(p, "area_unit", "hectares");
    }
    if (has_geo) cJSON_AddStringToObject(p, "geo_precision", "claim-polygon");
    cJSON_AddStringToObject(p, "crs", "EPSG:4326");
    cJSON_AddStringToObject(p, "region", "CA-BC");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *issue = jo_sv(pr, "ISSUE_DATE");
    char title[288];
    snprintf(title, sizeof title, "BC tenure %s%s%s", tenure,
             claim ? " — " : "", claim ? claim : "");

    intel_item row = {0};
    row.remote_key      = tenure;
    row.title           = title;
    row.summary         = title;
    row.published_at    = issue;
    row.lang            = "en";
    row.link            = "https://mtonline.gov.bc.ca/";
    row.record_type     = "mineral-tenure";
    row.has_geo         = has_geo;
    row.lat = lat; row.lon = lon;
    row.geometry_geojson= gj;
    row.properties_json = pj;
    row.tags_json       = "[\"industry\",\"mining\",\"tenure\",\"canada\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_bc_mineral_tenure_def = {
  .id = SRC, .collector = "industry",
  .name = "British Columbia mineral and placer tenure register",
  .update_interval_sec = 604800, .run = run,
  .category = "industry", .type = "api",
  .url = "https://openmaps.gov.bc.ca/geo/pub/wfs?typeName=WHSE_MINERAL_TENURE.MTA_ACQUIRED_TENURE_SVW&srsName=EPSG:4326",
  .description = "Every active mineral and placer claim in British Columbia with claim polygon, tenure type, issue and expiry dates.",
  .license = "Open Government Licence - British Columbia (BC Data Catalogue WFS), keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_bc_mineral_tenure_def)
