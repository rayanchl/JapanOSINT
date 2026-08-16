/* collectors/sources/country_extras.c
 * OSINT services — net-new keyless country-specific address/politician pivots on
 * ctx->entity. Real fetch or honest-empty. No API keys.
 *
 *   DE_POSTAL       openplzapi.org/de/Localities?postalCode=<code>
 *   NL_ADDRESS      api.pdok.nl/bzk/locatieserver/search/v3_1/free?q=
 *   DE_POLITICIANS  abgeordnetenwatch.de/api/v2/politicians?politician[label][cn]=
 *   CA_PARLIAMENT   api.openparliament.ca/politicians/?q= */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *CE_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run_de_postal(const source_ctx *ctx, intel_sink *sink, const char *raw) {
  char code[16]; int i = 0;
  for (const char *p = raw; *p && i < 15; p++) if (isdigit((unsigned char)*p)) code[i++] = *p;
  code[i] = 0;
  if (i < 4) { fprintf(stderr, "[DE_POSTAL] entity must be a German postal code\n"); return 0; }
  int n = 0;
  /* Page walk: OpenPLZ paginates, and reading page 1 only threw away
   * every locality past the 20th (docs/SOURCE_EXHAUSTIVENESS.md). Stop
   * on the first empty page. */
  for (int page = 1; page <= 20; page++) {
    char url[320];
    snprintf(url, sizeof url, "https://openplzapi.org/de/Localities?postalCode=%s&page=%d&pageSize=20", code, page);
    const char *hdrs[] = { "Accept: application/json", CE_UA, NULL };
    char *body = jo_get(ctx, url, hdrs, "DE_POSTAL");
    if (!body) break;
    cJSON *root = cJSON_Parse(body); free(body);
    if (!cJSON_IsArray(root)) { if (root) cJSON_Delete(root); break; }
    int page_n = 0;
    const cJSON *loc;
    cJSON_ArrayForEach(loc, root) {
      const char *name = jo_sv(loc, "name");
      const char *pc = jo_sv(loc, "postalCode");
      const cJSON *muni = cJSON_GetObjectItem(loc, "municipality");
      const char *mun = muni ? jo_sv(muni, "name") : NULL;
      const cJSON *state = cJSON_GetObjectItem(loc, "federalState");
      const char *st = state ? jo_sv(state, "name") : NULL;
      if (!name) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "locality", name);
      if (pc) cJSON_AddStringToObject(d, "postal_code", pc);
      if (mun) cJSON_AddStringToObject(d, "municipality", mun);
      if (st) cJSON_AddStringToObject(d, "federal_state", st);
      cJSON_AddStringToObject(d, "source", "OpenPLZ");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "DE_POSTAL"); cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char rk[96]; snprintf(rk, sizeof rk, "deplz:%s:%s", pc ? pc : code, name);
      char summ[160]; snprintf(summ, sizeof summ, "%s%s%s", mun ? mun : "", st ? ", " : "", st ? st : "");
      intel_item it = {0};
      it.remote_key = rk; it.title = name; it.summary = summ[0] ? summ : NULL; it.body = bj; it.lang = "de";
      it.record_type = "de-locality"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"DE_POSTAL\"]";
      if (sink->emit(sink, &it) >= 0) { n++; page_n++; }
      free(bj); free(pj);
    }
    cJSON_Delete(root);
    if (page_n == 0) break;          /* upstream exhausted */
  }
  return n;
}

/* PDOK centroide_ll is "POINT(lon lat)". */
static int parse_point(const char *s, double *lat, double *lon) {
  const char *o = s ? strchr(s, '(') : NULL;
  if (!o) return 0;
  return sscanf(o + 1, "%lf %lf", lon, lat) == 2;
}
static int run_nl_address(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://api.pdok.nl/bzk/locatieserver/search/v3_1/free?q=%s&rows=10", enc);
  const char *hdrs[] = { "Accept: application/json", CE_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "NL_ADDRESS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *resp = cJSON_GetObjectItem(root, "response");
  const cJSON *docs = resp ? cJSON_GetObjectItem(resp, "docs") : NULL;
  int n = 0;
  if (cJSON_IsArray(docs)) {
    const cJSON *doc;
    cJSON_ArrayForEach(doc, docs) {
      const char *naam = jo_sv(doc, "weergavenaam");
      const char *type = jo_sv(doc, "type");
      const char *woon = jo_sv(doc, "woonplaatsnaam");
      const char *pt = jo_sv(doc, "centroide_ll");
      const char *id = jo_sv(doc, "id");
      if (!naam) continue;
      double lat = 0, lon = 0; int hg = parse_point(pt, &lat, &lon);
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "name", naam);
      if (type) cJSON_AddStringToObject(d, "type", type);
      if (woon) cJSON_AddStringToObject(d, "place", woon);
      if (hg) { cJSON_AddNumberToObject(d, "lat", lat); cJSON_AddNumberToObject(d, "lon", lon); }
      cJSON_AddStringToObject(d, "source", "PDOK Locatieserver");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "NL_ADDRESS"); cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      intel_item it = {0};
      it.remote_key = id ? id : naam; it.title = naam; it.summary = type; it.body = bj; it.lang = "nl";
      it.has_geo = hg; it.lat = lat; it.lon = lon;
      it.record_type = "nl-address"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"NL_ADDRESS\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run_ca_parliament(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://api.openparliament.ca/politicians/?q=%s&format=json&limit=20", enc);
  const char *hdrs[] = { "Accept: application/json", "API-Version: v1", CE_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "CA_PARLIAMENT");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *objs = cJSON_GetObjectItem(root, "objects");
  int n = 0;
  if (cJSON_IsArray(objs)) {
    const cJSON *o;
    cJSON_ArrayForEach(o, objs) {
      const char *name = jo_sv(o, "name");
      const char *urlf = jo_sv(o, "url");
      const char *party = jo_sv(o, "current_party");
      const char *riding = jo_sv(o, "current_riding");
      if (!name) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "name", name);
      if (party) cJSON_AddStringToObject(d, "party", party);
      if (riding) cJSON_AddStringToObject(d, "riding", riding);
      cJSON_AddStringToObject(d, "source", "OpenParliament.ca");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "CA_PARLIAMENT"); cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char link[256] = {0};
      if (urlf) snprintf(link, sizeof link, "https://openparliament.ca%s", urlf);
      intel_item it = {0};
      it.remote_key = urlf ? urlf : name; it.title = name; it.summary = party;
      it.body = bj; it.link = link[0] ? link : NULL; it.lang = "en";
      it.record_type = "ca-politician"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"CA_PARLIAMENT\"]";
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
  if (!strcmp(sid, "DE_POSTAL")) { n = run_de_postal(ctx, sink, q); }
  else {
    char *enc = jo_urlencode(q); if (!enc) return 0;
    if      (!strcmp(sid, "NL_ADDRESS"))     n = run_nl_address(ctx, sink, enc);
    else if (!strcmp(sid, "CA_PARLIAMENT"))  n = run_ca_parliament(ctx, sink, enc);
    free(enc);
  }
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define CE_DEF(sym, ID, NM, CAT, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = CAT, \
    .type = "api", .url = "internal://osint/country", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

CE_DEF(ce_de_postal_def, "DE_POSTAL", "Germany Postal (OpenPLZ)", "geospatial",
  "OpenPLZ keyless German postal code → locality, municipality, federal state.");
CE_DEF(ce_nl_addr_def, "NL_ADDRESS", "Netherlands Address (PDOK)", "geospatial",
  "PDOK Locatieserver keyless Dutch address/place search with coordinates.");
CE_DEF(ce_ca_parl_def, "CA_PARLIAMENT", "Canada Parliament", "government",
  "OpenParliament.ca keyless Canadian politician search (party, riding).");
