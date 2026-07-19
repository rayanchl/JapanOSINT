/* collectors/sources/uk_world.c
 * OSINT services — net-new keyless UK public-records pivots on ctx->entity.
 * Real fetch or honest-empty. No API keys.
 *
 *   UK_POLICE_CRIME  data.police.uk/api/crimes-street/all-crime?lat=&lng= (lat,lon)
 *   UK_POSTCODES     api.postcodes.io/postcodes/<postcode>
 *   UK_FSA_RATINGS   api.ratings.food.gov.uk/Establishments?name= */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *UK_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run_police(const source_ctx *ctx, intel_sink *sink, const char *raw) {
  double lat, lon;
  if (sscanf(raw, "%lf , %lf", &lat, &lon) != 2 && sscanf(raw, "%lf/%lf", &lat, &lon) != 2) {
    fprintf(stderr, "[UK_POLICE_CRIME] entity must be 'lat,lon'\n"); return 0;
  }
  char url[320];
  snprintf(url, sizeof url, "https://data.police.uk/api/crimes-street/all-crime?lat=%.6f&lng=%.6f", lat, lon);
  const char *hdrs[] = { "Accept: application/json", UK_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "UK_POLICE_CRIME");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!cJSON_IsArray(root)) { if (root) cJSON_Delete(root); return 0; }
  int n = 0;
  const cJSON *c;
  cJSON_ArrayForEach(c, root) {
    const char *cat = jo_sv(c, "category");
    const char *month = jo_sv(c, "month");
    const char *pid = jo_sv(c, "persistent_id");
    const cJSON *loc = cJSON_GetObjectItem(c, "location");
    const cJSON *street = loc ? cJSON_GetObjectItem(loc, "street") : NULL;
    const char *stname = street ? jo_sv(street, "name") : NULL;
    double clat = 0, clon = 0; int hg = 0;
    if (loc) { const char *la = jo_sv(loc, "latitude"), *lo = jo_sv(loc, "longitude"); if (la && lo) { clat = atof(la); clon = atof(lo); hg = 1; } }
    if (!cat) continue;
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "category", cat);
    if (month) cJSON_AddStringToObject(d, "month", month);
    if (stname) cJSON_AddStringToObject(d, "street", stname);
    cJSON_AddStringToObject(d, "source", "data.police.uk");
    char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "service", "UK_POLICE_CRIME"); cJSON_AddStringToObject(p, "category", cat);
    cJSON_AddBoolToObject(p, "success", 1);
    char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
    char title[200]; snprintf(title, sizeof title, "%s%s%s", cat, stname ? " · " : "", stname ? stname : "");
    char rk[128]; snprintf(rk, sizeof rk, "ukcrime:%s", pid ? pid : title);
    intel_item it = {0};
    it.remote_key = rk; it.title = title; it.summary = month; it.body = bj; it.lang = "en";
    it.has_geo = hg; it.lat = clat; it.lon = clon;
    it.record_type = "uk-crime"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"UK_POLICE_CRIME\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj); free(pj);
    if (n >= 50) break;
  }
  cJSON_Delete(root);
  return n;
}

static int run_postcode(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[320];
  snprintf(url, sizeof url, "https://api.postcodes.io/postcodes/%s", enc);
  const char *hdrs[] = { "Accept: application/json", UK_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "UK_POSTCODES");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *res = cJSON_GetObjectItem(root, "result");
  if (!res || cJSON_IsNull(res)) { cJSON_Delete(root); return 0; }
  const char *pc = jo_sv(res, "postcode");
  const cJSON *lat = cJSON_GetObjectItem(res, "latitude");
  const cJSON *lon = cJSON_GetObjectItem(res, "longitude");
  const char *district = jo_sv(res, "admin_district");
  const char *region = jo_sv(res, "region");
  const char *constit = jo_sv(res, "parliamentary_constituency");
  if (!pc) { cJSON_Delete(root); return 0; }
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "postcode", pc);
  if (district) cJSON_AddStringToObject(d, "admin_district", district);
  if (region) cJSON_AddStringToObject(d, "region", region);
  if (constit) cJSON_AddStringToObject(d, "constituency", constit);
  cJSON_AddStringToObject(d, "source", "postcodes.io");
  cJSON *p = cJSON_CreateObject();
  int hg = (cJSON_IsNumber(lat) && cJSON_IsNumber(lon));
  char summ[200]; snprintf(summ, sizeof summ, "%s%s%s", district ? district : "", region ? ", " : "", region ? region : "");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON_AddStringToObject(p, "service", "UK_POSTCODES"); cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  intel_item it = {0};
  it.remote_key = pc; it.title = pc; it.summary = summ[0] ? summ : NULL; it.body = bj; it.lang = "en";
  it.has_geo = hg; it.lat = hg ? lat->valuedouble : 0; it.lon = hg ? lon->valuedouble : 0;
  it.record_type = "uk-postcode"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"UK_POSTCODES\"]";
  int rc = sink->emit(sink, &it); free(bj); free(pj); cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run_fsa(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://api.ratings.food.gov.uk/Establishments?name=%s&pageSize=20", enc);
  const char *hdrs[] = { "Accept: application/json", "x-api-version: 2", UK_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "UK_FSA_RATINGS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *ests = cJSON_GetObjectItem(root, "establishments");
  int n = 0;
  if (cJSON_IsArray(ests)) {
    const cJSON *e;
    cJSON_ArrayForEach(e, ests) {
      const char *name = jo_sv(e, "BusinessName");
      const char *rating = jo_sv(e, "RatingValue");
      const char *pc = jo_sv(e, "PostCode");
      const char *addr = jo_sv(e, "AddressLine1");
      const char *fhrsid = jo_sv(e, "FHRSID");
      if (!name) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "business", name);
      if (rating) cJSON_AddStringToObject(d, "rating", rating);
      if (addr) cJSON_AddStringToObject(d, "address", addr);
      if (pc) cJSON_AddStringToObject(d, "postcode", pc);
      cJSON_AddStringToObject(d, "source", "FSA Food Hygiene");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "UK_FSA_RATINGS"); cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char title[220]; snprintf(title, sizeof title, "%s — rating %s", name, rating ? rating : "?");
      intel_item it = {0};
      it.remote_key = fhrsid ? fhrsid : name; it.title = title; it.summary = pc; it.body = bj; it.lang = "en";
      it.record_type = "uk-food-rating"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"UK_FSA_RATINGS\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  int n = 0;
  if (!strcmp(sid, "UK_POLICE_CRIME")) { n = run_police(ctx, sink, q); }
  else {
    char *enc = jo_urlencode(q); if (!enc) return 0;
    if      (!strcmp(sid, "UK_POSTCODES"))   n = run_postcode(ctx, sink, enc);
    else if (!strcmp(sid, "UK_FSA_RATINGS")) n = run_fsa(ctx, sink, enc);
    free(enc);
  }
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define UK_DEF(sym, ID, NM, CAT, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = CAT, \
    .type = "api", .url = "internal://osint/uk", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

UK_DEF(uk_police_def, "UK_POLICE_CRIME", "UK Street-Level Crime", "government",
  "data.police.uk keyless street-level crimes near a lat,lon (category, street, month).");
UK_DEF(uk_postcode_def, "UK_POSTCODES", "UK Postcode Lookup", "geospatial",
  "postcodes.io keyless UK postcode → coordinates, district, region, constituency.");
UK_DEF(uk_fsa_def, "UK_FSA_RATINGS", "UK Food Hygiene Ratings", "government",
  "FSA keyless food-hygiene ratings by business name (rating, address, postcode).");
