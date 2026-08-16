/* collectors/sources/country_opendata2.c
 * OSINT services — more keyless national open-data (CKAN) portals, pivot on
 * ctx->entity (keyword). Shared CKAN package_search(). Honest-empty on failure.
 *
 *   CH_OPENDATA / NZ_OPENDATA / AT_OPENDATA / FI_OPENDATA / UK_DATAGOV / US_DATAGOV */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *CO2_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int ckan2_search(const source_ctx *ctx, intel_sink *sink, const char *service,
                        const char *api_base, const char *web_base, const char *enc) {
  char url[640];
  snprintf(url, sizeof url, "%s/api/3/action/package_search?q=%s&rows=20", api_base, enc);
  const char *hdrs[] = { "Accept: application/json", CO2_UA, NULL };
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
      const cJSON *tt = cJSON_GetObjectItem(ds, "title");
      const char *title = NULL;
      /* some portals give localized title objects; prefer a plain string */
      if (cJSON_IsString(tt)) title = tt->valuestring;
      const char *name = jo_sv(ds, "name");
      const cJSON *org = cJSON_GetObjectItem(ds, "organization");
      const char *orgt = org ? jo_sv(org, "title") : NULL;
      if (!title && !name) continue;
      cJSON *d = cJSON_CreateObject();
      if (title) cJSON_AddStringToObject(d, "title", title);
      if (orgt) cJSON_AddStringToObject(d, "organization", orgt);
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
  if      (!strcmp(sid, "CH_OPENDATA")) n = ckan2_search(ctx, sink, "CH_OPENDATA", "https://opendata.swiss", "https://opendata.swiss/en", enc);
  else if (!strcmp(sid, "FI_OPENDATA")) n = ckan2_search(ctx, sink, "FI_OPENDATA", "https://www.avoindata.fi/data", "https://www.avoindata.fi/data/en_GB", enc);
  else if (!strcmp(sid, "UK_DATAGOV"))  n = ckan2_search(ctx, sink, "UK_DATAGOV", "https://data.gov.uk", "https://data.gov.uk", enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define CO2_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "government", \
    .type = "api", .url = "internal://osint/opendata2", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

CO2_DEF(co2_ch_def, "CH_OPENDATA", "Switzerland Open Data", "opendata.swiss keyless CKAN open-dataset search.");
CO2_DEF(co2_fi_def, "FI_OPENDATA", "Finland Open Data", "avoindata.fi keyless CKAN open-dataset search.");
CO2_DEF(co2_uk_def, "UK_DATAGOV", "UK data.gov.uk", "data.gov.uk keyless CKAN open-dataset search.");
