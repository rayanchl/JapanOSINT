/* collectors/sources/country_opendata.c
 * OSINT services — net-new keyless national open-data (CKAN) portal search,
 * pivot on ctx->entity (keyword). One shared CKAN package_search() over several
 * countries' portals. Real fetch or honest-empty. No API keys.
 *
 *   DE_GOVDATA / CA_OPENDATA / AU_OPENDATA / NL_OPENDATA / IT_OPENDATA / IE_OPENDATA */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *CO_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int ckan_search(const source_ctx *ctx, intel_sink *sink, const char *service,
                       const char *api_base, const char *web_base, const char *enc) {
  char url[640];
  snprintf(url, sizeof url, "%s/api/3/action/package_search?q=%s&rows=20", api_base, enc);
  const char *hdrs[] = { "Accept: application/json", CO_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, service);
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *result = cJSON_GetObjectItem(root, "result");
  const cJSON *results = result ? cJSON_GetObjectItem(result, "results") : NULL;
  int n = 0;
  if (cJSON_IsArray(results)) {
    const cJSON *ds;
    cJSON_ArrayForEach(ds, results) {
      const char *title = jo_sv(ds, "title");
      const char *name = jo_sv(ds, "name");
      const char *notes = jo_sv(ds, "notes");
      const cJSON *org = cJSON_GetObjectItem(ds, "organization");
      const char *orgt = org ? jo_sv(org, "title") : NULL;
      if (!title && !name) continue;
      cJSON *d = cJSON_CreateObject();
      if (title) cJSON_AddStringToObject(d, "title", title);
      if (orgt) cJSON_AddStringToObject(d, "organization", orgt);
      if (notes) { char nn[241]; snprintf(nn, sizeof nn, "%.240s", notes); cJSON_AddStringToObject(d, "notes", nn); }
      cJSON_AddStringToObject(d, "source", service);
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", service); cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char tags[96]; snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", service);
      char link[512] = {0};
      if (name && web_base) snprintf(link, sizeof link, "%s/dataset/%s", web_base, name);
      intel_item it = {0};
      it.remote_key = name ? name : title; it.title = title ? title : name; it.summary = orgt;
      it.body = bj; it.link = link[0] ? link : NULL; it.lang = "en";
      it.record_type = "open-dataset"; it.properties_json = pj; it.tags_json = tags;
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
  if      (!strcmp(sid, "DE_GOVDATA"))  n = ckan_search(ctx, sink, "DE_GOVDATA", "https://www.govdata.de/ckan", "https://www.govdata.de", enc);
  else if (!strcmp(sid, "CA_OPENDATA")) n = ckan_search(ctx, sink, "CA_OPENDATA", "https://open.canada.ca/data", "https://open.canada.ca/data/en", enc);
  else if (!strcmp(sid, "AU_OPENDATA")) n = ckan_search(ctx, sink, "AU_OPENDATA", "https://data.gov.au/data", "https://data.gov.au/dataset", enc);
  else if (!strcmp(sid, "NL_OPENDATA")) n = ckan_search(ctx, sink, "NL_OPENDATA", "https://data.overheid.nl/data", "https://data.overheid.nl/dataset", enc);
  else if (!strcmp(sid, "IT_OPENDATA")) n = ckan_search(ctx, sink, "IT_OPENDATA", "https://www.dati.gov.it/opendata", "https://www.dati.gov.it", enc);
  else if (!strcmp(sid, "IE_OPENDATA")) n = ckan_search(ctx, sink, "IE_OPENDATA", "https://data.gov.ie", "https://data.gov.ie", enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define CO_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "government", \
    .type = "api", .url = "internal://osint/opendata", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

CO_DEF(co_de_def, "DE_GOVDATA", "Germany GovData", "GovData.de keyless CKAN open-dataset search (title, organization).");
CO_DEF(co_ca_def, "CA_OPENDATA", "Canada Open Data", "open.canada.ca keyless CKAN open-dataset search.");
CO_DEF(co_au_def, "AU_OPENDATA", "Australia Open Data", "data.gov.au keyless CKAN open-dataset search.");
CO_DEF(co_nl_def, "NL_OPENDATA", "Netherlands Open Data", "data.overheid.nl keyless CKAN open-dataset search.");
CO_DEF(co_it_def, "IT_OPENDATA", "Italy Open Data", "dati.gov.it keyless CKAN open-dataset search.");
CO_DEF(co_ie_def, "IE_OPENDATA", "Ireland Open Data", "data.gov.ie keyless CKAN open-dataset search.");
