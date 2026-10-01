/* collectors/sources/fr_world.c
 * OSINT services — net-new keyless France public-data pivots on ctx->entity.
 * Real fetch or honest-empty. No API keys.
 *
 *   FR_ADDRESS_BAN  api-adresse.data.gouv.fr/search/?q=   (Base Adresse Nationale)
 *   FR_BODACC       bodacc-datadila.opendatasoft.com (annonces-commerciales)
 *   FR_DATAGOUV     data.gouv.fr/api/1/datasets/?q= */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *FR_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run_ban(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://api-adresse.data.gouv.fr/search/?q=%s&limit=10", enc);
  const char *hdrs[] = { "Accept: application/json", FR_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "FR_ADDRESS_BAN");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *feats = cJSON_GetObjectItem(root, "features");
  int n = 0;
  if (cJSON_IsArray(feats)) {
    const cJSON *f;
    cJSON_ArrayForEach(f, feats) {
      const cJSON *props = cJSON_GetObjectItem(f, "properties");
      const cJSON *geom = cJSON_GetObjectItem(f, "geometry");
      const cJSON *coords = geom ? cJSON_GetObjectItem(geom, "coordinates") : NULL;
      if (!props || !cJSON_IsArray(coords) || cJSON_GetArraySize(coords) < 2) continue;
      double lon = cJSON_GetArrayItem(coords, 0)->valuedouble;
      double lat = cJSON_GetArrayItem(coords, 1)->valuedouble;
      const char *label = jo_sv(props, "label");
      const char *city = jo_sv(props, "city");
      const char *pc = jo_sv(props, "postcode");
      const char *ctx2 = jo_sv(props, "context");
      if (!label) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "label", label);
      if (city) cJSON_AddStringToObject(d, "city", city);
      if (pc) cJSON_AddStringToObject(d, "postcode", pc);
      if (ctx2) cJSON_AddStringToObject(d, "context", ctx2);
      cJSON_AddNumberToObject(d, "lat", lat); cJSON_AddNumberToObject(d, "lon", lon);
      cJSON_AddStringToObject(d, "source", "BAN (data.gouv.fr)");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "FR_ADDRESS_BAN"); cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char rk[96]; snprintf(rk, sizeof rk, "ban:%.6f,%.6f", lat, lon);
      intel_item it = {0};
      it.remote_key = rk; it.title = label; it.summary = pc; it.body = bj; it.lang = "fr";
      it.has_geo = 1; it.lat = lat; it.lon = lon;
      it.record_type = "fr-address"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"FR_ADDRESS_BAN\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run_bodacc(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[640];
  snprintf(url, sizeof url,
    "https://bodacc-datadila.opendatasoft.com/api/explore/v2.1/catalog/datasets/annonces-commerciales/records?q=%s&limit=20", enc);
  const char *hdrs[] = { "Accept: application/json", FR_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "FR_BODACC");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *results = cJSON_GetObjectItem(root, "results");
  int n = 0;
  if (cJSON_IsArray(results)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const char *commercant = jo_sv(r, "commercant");
      const char *ville = jo_sv(r, "ville");
      const char *famille = jo_sv(r, "familleavis_lib");
      const char *date = jo_sv(r, "dateparution");
      const char *registre = jo_sv(r, "registre");
      const char *id = jo_sv(r, "id");
      if (!commercant && !registre) continue;
      cJSON *d = cJSON_CreateObject();
      if (commercant) cJSON_AddStringToObject(d, "commercant", commercant);
      if (ville) cJSON_AddStringToObject(d, "ville", ville);
      if (famille) cJSON_AddStringToObject(d, "type", famille);
      if (registre) cJSON_AddStringToObject(d, "registre", registre);
      cJSON_AddStringToObject(d, "source", "BODACC");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "FR_BODACC"); cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char summ[200]; snprintf(summ, sizeof summ, "%s%s%s", famille ? famille : "", ville ? " · " : "", ville ? ville : "");
      intel_item it = {0};
      it.remote_key = id ? id : (commercant ? commercant : registre);
      it.title = commercant ? commercant : "BODACC annonce"; it.summary = summ[0] ? summ : NULL;
      it.body = bj; it.lang = "fr"; it.published_at = date;
      it.record_type = "fr-bodacc-annonce"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"FR_BODACC\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run_datagouv(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://www.data.gouv.fr/api/1/datasets/?q=%s&page_size=20", enc);
  const char *hdrs[] = { "Accept: application/json", FR_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "FR_DATAGOUV");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *data = cJSON_GetObjectItem(root, "data");
  int n = 0;
  if (cJSON_IsArray(data)) {
    const cJSON *ds;
    cJSON_ArrayForEach(ds, data) {
      const char *title = jo_sv(ds, "title");
      const char *page = jo_sv(ds, "page");
      const char *id = jo_sv(ds, "id");
      const cJSON *org = cJSON_GetObjectItem(ds, "organization");
      const char *orgn = org ? jo_sv(org, "name") : NULL;
      if (!title) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "title", title);
      if (orgn) cJSON_AddStringToObject(d, "organization", orgn);
      cJSON_AddStringToObject(d, "source", "data.gouv.fr");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "FR_DATAGOUV"); cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      intel_item it = {0};
      it.remote_key = id ? id : title; it.title = title; it.summary = orgn;
      it.body = bj; it.link = page; it.lang = "fr";
      it.record_type = "fr-dataset"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"FR_DATAGOUV\"]";
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
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "FR_ADDRESS_BAN")) n = run_ban(ctx, sink, enc);
  else if (!strcmp(sid, "FR_BODACC"))      n = run_bodacc(ctx, sink, enc);
  else if (!strcmp(sid, "FR_DATAGOUV"))    n = run_datagouv(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define FR_DEF(sym, ID, NM, CAT, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = CAT, \
    .type = "api", .url = "internal://osint/fr", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

FR_DEF(fr_ban_def, "FR_ADDRESS_BAN", "France Address (BAN)", "geospatial",
  "Base Adresse Nationale keyless French address geocoding (label, city, postcode, coords).");
FR_DEF(fr_bodacc_def, "FR_BODACC", "France BODACC Announcements", "government",
  "BODACC keyless French company legal announcements (commercant, ville, type).");
FR_DEF(fr_datagouv_def, "FR_DATAGOUV", "data.gouv.fr Datasets", "government",
  "data.gouv.fr keyless open-dataset search (title, organization).");
