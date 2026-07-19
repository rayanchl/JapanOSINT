/* collectors/sources/country_geo2.c
 * OSINT services — more keyless country address/postal geocoders, pivot on
 * ctx->entity. Honest-empty on failure. No API keys.
 *
 *   AT_POSTAL   openplzapi.org/at/Localities?postalCode=<code>
 *   CH_POSTAL   openplzapi.org/ch/Localities?postalCode=<code>
 *   CH_ADDRESS  api3.geo.admin.ch/rest/services/api/SearchServer?searchText=
 *   NO_ADDRESS  ws.geonorge.no/adresser/v1/sok?sok= */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *CG_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

/* first non-null nested {obj}.name among candidate keys */
static const char *nested_name(const cJSON *o, const char *const *keys) {
  for (int i = 0; keys[i]; i++) {
    const cJSON *sub = cJSON_GetObjectItem(o, keys[i]);
    if (sub) { const char *nm = jo_sv(sub, "name"); if (nm) return nm; }
  }
  return NULL;
}
static int run_openplz(const source_ctx *ctx, intel_sink *sink, const char *service,
                       const char *cc, const char *raw) {
  char code[16]; int i = 0;
  for (const char *p = raw; *p && i < 15; p++) if (isdigit((unsigned char)*p)) code[i++] = *p;
  code[i] = 0;
  if (i < 4) { fprintf(stderr, "[%s] entity must be a postal code\n", service); return 0; }
  char url[320];
  snprintf(url, sizeof url, "https://openplzapi.org/%s/Localities?postalCode=%s&page=1&pageSize=20", cc, code);
  const char *hdrs[] = { "Accept: application/json", CG_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, service);
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!cJSON_IsArray(root)) { if (root) cJSON_Delete(root); return 0; }
  static const char *muni_keys[] = { "municipality", "commune", "district", NULL };
  static const char *reg_keys[]  = { "federalState", "federalProvince", "canton", NULL };
  int n = 0;
  const cJSON *loc;
  cJSON_ArrayForEach(loc, root) {
    const char *name = jo_sv(loc, "name");
    const char *pc = jo_sv(loc, "postalCode");
    const char *mun = nested_name(loc, muni_keys);
    const char *reg = nested_name(loc, reg_keys);
    if (!name) continue;
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "locality", name);
    if (pc) cJSON_AddStringToObject(d, "postal_code", pc);
    if (mun) cJSON_AddStringToObject(d, "municipality", mun);
    if (reg) cJSON_AddStringToObject(d, "region", reg);
    cJSON_AddStringToObject(d, "source", "OpenPLZ");
    char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "service", service); cJSON_AddBoolToObject(p, "success", 1);
    char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
    char rk[96]; snprintf(rk, sizeof rk, "%s:%s:%s", cc, pc ? pc : code, name);
    char summ[160]; snprintf(summ, sizeof summ, "%s%s%s", mun ? mun : "", reg ? ", " : "", reg ? reg : "");
    char tags[64]; snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", service);
    intel_item it = {0};
    it.remote_key = rk; it.title = name; it.summary = summ[0] ? summ : NULL; it.body = bj; it.lang = "de";
    it.record_type = "locality"; it.properties_json = pj; it.tags_json = tags;
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj); free(pj);
  }
  cJSON_Delete(root);
  return n;
}

static void strip_html(char *s) { if (!s) return; char *w = s; int in = 0; for (char *r = s; *r; r++) { if (*r=='<') in=1; else if (*r=='>') in=0; else if (!in) *w++=*r; } *w=0; }

static int run_ch_address(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url,
    "https://api3.geo.admin.ch/rest/services/api/SearchServer?searchText=%s&type=locations&limit=10", enc);
  const char *hdrs[] = { "Accept: application/json", CG_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "CH_ADDRESS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *results = cJSON_GetObjectItem(root, "results");
  int n = 0;
  if (cJSON_IsArray(results)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const cJSON *attrs = cJSON_GetObjectItem(r, "attrs");
      if (!attrs) continue;
      const char *label0 = jo_sv(attrs, "label");
      const cJSON *lat = cJSON_GetObjectItem(attrs, "lat");
      const cJSON *lon = cJSON_GetObjectItem(attrs, "lon");
      const char *detail = jo_sv(attrs, "detail");
      if (!label0) continue;
      char *label = strdup(label0); strip_html(label);
      int hg = (cJSON_IsNumber(lat) && cJSON_IsNumber(lon));
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "label", label);
      if (detail) cJSON_AddStringToObject(d, "detail", detail);
      if (hg) { cJSON_AddNumberToObject(d, "lat", lat->valuedouble); cJSON_AddNumberToObject(d, "lon", lon->valuedouble); }
      cJSON_AddStringToObject(d, "source", "geo.admin.ch");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "CH_ADDRESS"); cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      intel_item it = {0};
      it.remote_key = detail ? detail : label; it.title = label; it.body = bj; it.lang = "de";
      it.has_geo = hg; it.lat = hg ? lat->valuedouble : 0; it.lon = hg ? lon->valuedouble : 0;
      it.record_type = "ch-location"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"CH_ADDRESS\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(bj); free(pj); free(label);
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run_no_address(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://ws.geonorge.no/adresser/v1/sok?sok=%s&treffPerSide=10", enc);
  const char *hdrs[] = { "Accept: application/json", CG_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "NO_ADDRESS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *adr = cJSON_GetObjectItem(root, "adresser");
  int n = 0;
  if (cJSON_IsArray(adr)) {
    const cJSON *a;
    cJSON_ArrayForEach(a, adr) {
      const char *text = jo_sv(a, "adressetekst");
      const char *poststed = jo_sv(a, "poststed");
      const char *postnr = jo_sv(a, "postnummer");
      const char *kommune = jo_sv(a, "kommunenavn");
      const cJSON *rp = cJSON_GetObjectItem(a, "representasjonspunkt");
      const cJSON *lat = rp ? cJSON_GetObjectItem(rp, "lat") : NULL;
      const cJSON *lon = rp ? cJSON_GetObjectItem(rp, "lon") : NULL;
      if (!text) continue;
      int hg = (cJSON_IsNumber(lat) && cJSON_IsNumber(lon));
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "address", text);
      if (poststed) cJSON_AddStringToObject(d, "post_place", poststed);
      if (postnr) cJSON_AddStringToObject(d, "postal_code", postnr);
      if (kommune) cJSON_AddStringToObject(d, "municipality", kommune);
      cJSON_AddStringToObject(d, "source", "Kartverket");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "NO_ADDRESS"); cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char summ[160]; snprintf(summ, sizeof summ, "%s%s%s", poststed ? poststed : "", kommune ? " · " : "", kommune ? kommune : "");
      intel_item it = {0};
      it.remote_key = text; it.title = text; it.summary = summ[0] ? summ : NULL; it.body = bj; it.lang = "no";
      it.has_geo = hg; it.lat = hg ? lat->valuedouble : 0; it.lon = hg ? lon->valuedouble : 0;
      it.record_type = "no-address"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"NO_ADDRESS\"]";
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
  if      (!strcmp(sid, "AT_POSTAL")) n = run_openplz(ctx, sink, "AT_POSTAL", "at", q);
  else if (!strcmp(sid, "CH_POSTAL")) n = run_openplz(ctx, sink, "CH_POSTAL", "ch", q);
  else {
    char *enc = jo_urlencode(q); if (!enc) return 0;
    if      (!strcmp(sid, "CH_ADDRESS")) n = run_ch_address(ctx, sink, enc);
    else if (!strcmp(sid, "NO_ADDRESS")) n = run_no_address(ctx, sink, enc);
    free(enc);
  }
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define CG_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "geospatial", \
    .type = "api", .url = "internal://osint/geo2", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

CG_DEF(cg_at_def, "AT_POSTAL", "Austria Postal (OpenPLZ)", "OpenPLZ keyless Austrian postal code → locality/municipality/province.");
CG_DEF(cg_ch_def, "CH_POSTAL", "Switzerland Postal (OpenPLZ)", "OpenPLZ keyless Swiss postal code → locality/commune/canton.");
CG_DEF(cg_chadr_def, "CH_ADDRESS", "Switzerland Geocode", "geo.admin.ch keyless Swiss location search with coordinates.");
CG_DEF(cg_noadr_def, "NO_ADDRESS", "Norway Address (Kartverket)", "Kartverket keyless Norwegian address search with coordinates.");
