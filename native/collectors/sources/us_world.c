/* collectors/sources/us_world.c
 * OSINT services — net-new keyless US public-records pivots on ctx->entity.
 * Real fetch or honest-empty. No API keys.
 *
 *   FEDERAL_REGISTER  federalregister.gov/api/v1/documents.json?conditions[term]=
 *   US_NPI_PROVIDER   npiregistry.cms.hhs.gov/api/?version=2.1&...   (healthcare)
 *   COURTLISTENER     courtlistener.com/api/rest/v4/search/?q=       (court opinions) */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *US_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run_fedreg(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url,
    "https://www.federalregister.gov/api/v1/documents.json?conditions[term]=%s&per_page=20&order=newest", enc);
  const char *hdrs[] = { "Accept: application/json", US_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "FEDERAL_REGISTER");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *results = cJSON_GetObjectItem(root, "results");
  int n = 0;
  if (cJSON_IsArray(results)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const char *title = jo_sv(r, "title");
      const char *type = jo_sv(r, "type");
      const char *date = jo_sv(r, "publication_date");
      const char *html = jo_sv(r, "html_url");
      const char *docnum = jo_sv(r, "document_number");
      const char *agency = NULL;
      const cJSON *ags = cJSON_GetObjectItem(r, "agencies");
      if (cJSON_IsArray(ags) && cJSON_GetArraySize(ags)) { const cJSON *a0 = cJSON_GetArrayItem(ags, 0);  /* exhaustive-ok: display pick; agencies_all carries every issuing agency */ if (a0) agency = jo_sv(a0, "name"); }
      if (!title) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "title", title);
      if (type) cJSON_AddStringToObject(d, "type", type);
      if (cJSON_IsArray(ags) && cJSON_GetArraySize(ags) > 1)
        cJSON_AddItemToObject(d, "agencies_all", cJSON_Duplicate(ags, 1));
      if (agency) cJSON_AddStringToObject(d, "agency", agency);
      cJSON_AddStringToObject(d, "source", "Federal Register");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "FEDERAL_REGISTER"); cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      intel_item it = {0};
      it.remote_key = docnum ? docnum : title; it.title = title; it.summary = agency;
      it.body = bj; it.link = html; it.lang = "en"; it.published_at = date;
      it.record_type = "federal-register-doc"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"FEDERAL_REGISTER\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run_npi(const source_ctx *ctx, intel_sink *sink, const char *raw) {
  /* Name entity: split on space → first/last; else treat as organization. */
  char first[128] = {0}, last[128] = {0};
  const char *sp = strchr(raw, ' ');
  char *encq;
  char url[640];
  if (sp) {
    snprintf(first, sizeof first, "%.*s", (int)(sp - raw), raw);
    snprintf(last, sizeof last, "%s", sp + 1);
    char *ef = jo_urlencode(first), *el = jo_urlencode(last);
    snprintf(url, sizeof url,
      "https://npiregistry.cms.hhs.gov/api/?version=2.1&first_name=%s&last_name=%s&limit=20",
      ef ? ef : "", el ? el : "");
    free(ef); free(el);
  } else {
    encq = jo_urlencode(raw);
    snprintf(url, sizeof url,
      "https://npiregistry.cms.hhs.gov/api/?version=2.1&organization_name=%s*&limit=20", encq ? encq : "");
    free(encq);
  }
  const char *hdrs[] = { "Accept: application/json", US_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "US_NPI_PROVIDER");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *results = cJSON_GetObjectItem(root, "results");
  int n = 0;
  if (cJSON_IsArray(results)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const cJSON *num = cJSON_GetObjectItem(r, "number");
      const cJSON *basic = cJSON_GetObjectItem(r, "basic");
      const char *fn = basic ? jo_sv(basic, "first_name") : NULL;
      const char *ln = basic ? jo_sv(basic, "last_name") : NULL;
      const char *org = basic ? jo_sv(basic, "organization_name") : NULL;
      const cJSON *addrs = cJSON_GetObjectItem(r, "addresses");
      const char *city = NULL, *state = NULL;
      if (cJSON_IsArray(addrs) && cJSON_GetArraySize(addrs)) { const cJSON *a0 = cJSON_GetArrayItem(addrs, 0);  /* exhaustive-ok: display pick; addresses_all carries every address */ city = jo_sv(a0, "city"); state = jo_sv(a0, "state"); }
      const cJSON *taxo = cJSON_GetObjectItem(r, "taxonomies");
      const char *desc = NULL;
      if (cJSON_IsArray(taxo) && cJSON_GetArraySize(taxo)) { const cJSON *t0 = cJSON_GetArrayItem(taxo, 0);  /* exhaustive-ok: display pick; taxonomies_all carries every specialty */ desc = jo_sv(t0, "desc"); }
      /* NPPES lists practice AND mailing addresses; both are carried below. */
      /* a provider can hold several taxonomies (specialties). */
      char npi[32] = {0}; if (num && cJSON_IsNumber(num)) snprintf(npi, sizeof npi, "%.0f", num->valuedouble);
      char name[256];
      if (org) snprintf(name, sizeof name, "%s", org);
      else snprintf(name, sizeof name, "%s %s", fn ? fn : "", ln ? ln : "");
      if (!name[0] && !npi[0]) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "name", name);
      if (npi[0]) cJSON_AddStringToObject(d, "npi", npi);
      if (desc) cJSON_AddStringToObject(d, "taxonomy", desc);
      if (city) cJSON_AddStringToObject(d, "city", city);
      if (state) cJSON_AddStringToObject(d, "state", state);
      cJSON_AddStringToObject(d, "source", "NPPES NPI Registry");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "US_NPI_PROVIDER"); if (npi[0]) cJSON_AddStringToObject(p, "npi", npi);
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char summ[160]; snprintf(summ, sizeof summ, "%s%s%s", desc ? desc : "", (city && state) ? " · " : "", (city && state) ? city : "");
      intel_item it = {0};
      it.remote_key = npi[0] ? npi : name; it.title = name; it.summary = summ[0] ? summ : NULL;
      it.body = bj; it.lang = "en"; it.record_type = "us-healthcare-provider";
      it.properties_json = pj; it.tags_json = "[\"osint-search\",\"US_NPI_PROVIDER\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(bj); free(pj);
    }
  }
  cJSON_Delete(root);
  return n;
}

static int run_courtlistener(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://www.courtlistener.com/api/rest/v4/search/?q=%s&type=o", enc);
  const char *hdrs[] = { "Accept: application/json", US_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "COURTLISTENER");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *results = cJSON_GetObjectItem(root, "results");
  int n = 0;
  if (cJSON_IsArray(results)) {
    const cJSON *r;
    cJSON_ArrayForEach(r, results) {
      const char *cn = jo_sv(r, "caseName");
      const char *court = jo_sv(r, "court");
      const char *date = jo_sv(r, "dateFiled");
      const char *absu = jo_sv(r, "absolute_url");
      if (!cn) continue;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "case_name", cn);
      if (court) cJSON_AddStringToObject(d, "court", court);
      if (date) cJSON_AddStringToObject(d, "date_filed", date);
      cJSON_AddStringToObject(d, "source", "CourtListener");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "COURTLISTENER"); cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char link[320] = {0};
      if (absu) snprintf(link, sizeof link, "https://www.courtlistener.com%s", absu);
      intel_item it = {0};
      it.remote_key = absu ? absu : cn; it.title = cn; it.summary = court;
      it.body = bj; it.link = link[0] ? link : NULL; it.lang = "en"; it.published_at = date;
      it.record_type = "us-court-opinion"; it.properties_json = pj; it.tags_json = "[\"osint-search\",\"COURTLISTENER\"]";
      if (sink->emit(sink, &it) >= 0) n++;
      free(bj); free(pj);
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
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
  if (!strcmp(sid, "US_NPI_PROVIDER")) { n = run_npi(ctx, sink, q); }
  else {
    char *enc = jo_urlencode(q); if (!enc) return 0;
    if      (!strcmp(sid, "FEDERAL_REGISTER")) n = run_fedreg(ctx, sink, enc);
    else if (!strcmp(sid, "COURTLISTENER"))    n = run_courtlistener(ctx, sink, enc);
    free(enc);
  }
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define US_DEF(sym, ID, NM, CAT, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = CAT, \
    .type = "api", .url = "internal://osint/us", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

US_DEF(us_fedreg_def, "FEDERAL_REGISTER", "US Federal Register", "government",
  "Federal Register keyless document search (title, agency, type, date).");
US_DEF(us_npi_def, "US_NPI_PROVIDER", "US NPI Healthcare Provider", "government",
  "NPPES keyless US healthcare provider lookup by name/org (NPI, taxonomy, location).");
US_DEF(us_cl_def, "COURTLISTENER", "US CourtListener Opinions", "government",
  "CourtListener keyless US court-opinion search (case, court, date).");
