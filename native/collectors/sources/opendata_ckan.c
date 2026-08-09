/* collectors/sources/opendata_ckan.c
 * OSINT service — OPENDATA_CKAN. On-demand entity pivot across ~40 national
 * open-data portals that all run CKAN, which exposes a uniform Action API:
 *   GET <portal>/api/3/action/package_search?q=<entity>&rows=5
 * For each portal we real-fetch that endpoint, parse result.results[], and emit
 * one intel_item per dataset (title + owning organization + portal + canonical
 * dataset URL), capped to a few per portal. Keyless and LIVE.
 *
 * Only real, parsed API fields are emitted; a portal that errors, is offline,
 * JS-only, or returns no matches simply contributes nothing (honest empty per
 * portal). Nothing here fabricates dataset titles, orgs, or URLs. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Per-portal caps to keep total output bounded. */
#define CKAN_PER_PORTAL 3

/* A national CKAN open-data portal. `base` has NO trailing slash; `site` is the
 * human-facing base a dataset slug hangs off of (usually base + "/dataset/"). */
typedef struct {
  const char *base;   /* API base (protocol + host [+ path]) */
  const char *label;  /* short human name of the portal      */
  const char *country;
} ckan_portal;

/* CKAN-powered national/regional open-data portals. All expose
 * /api/3/action/package_search. Some (data.gov.uk, data.europa.eu) proxy CKAN
 * behind their own path; the ones listed use the canonical action path. */
static const ckan_portal PORTALS[] = {
  { "https://catalog.data.gov",          "data.gov",           "US" },
  { "https://data.gov.au/data",          "data.gov.au",        "AU" },
  { "https://catalogue.data.govt.nz",    "data.govt.nz",       "NZ" },
  { "https://data.europa.eu/api/hub/search", "data.europa.eu", "EU" },
  { "https://open.canada.ca/data/en",    "open.canada.ca",     "CA" },
  { "https://www.dati.gov.it/opendata",  "dati.gov.it",        "IT" },
  { "https://datos.gob.es/apidata",      "datos.gob.es",       "ES" },
  { "https://www.data.gouv.fr",          "data.gouv.fr",       "FR" },
  { "https://ckan.govdata.de",           "govdata.de",         "DE" },
  { "https://data.overheid.nl/data",     "data.overheid.nl",   "NL" },
  { "https://dados.gov.br",              "dados.gov.br",       "BR" },
  { "https://data.gov.ie",               "data.gov.ie",        "IE" },
  { "https://data.gov.sk",               "data.gov.sk",        "SK" },
  { "https://data.gov.lv",               "data.gov.lv",        "LV" },
  { "https://data.gov.ro",               "data.gov.ro",        "RO" },
  { "https://opendata.riik.ee",          "opendata.riik.ee",   "EE" },
  { "https://data.gov.cy",               "data.gov.cy",        "CY" },
  { "https://data.gov.mt",               "data.gov.mt",        "MT" },
  { "https://www.datos.gov.co",          "datos.gov.co",       "CO" },
  { "https://datos.gob.cl",              "datos.gob.cl",       "CL" },
  { "https://datos.gob.mx",              "datos.gob.mx",       "MX" },
  { "https://data.gov.ua",               "data.gov.ua",        "UA" },
  { "https://data.gov.gr",               "data.gov.gr",        "GR" },
  { "https://data.gov.hr",               "data.gov.hr",        "HR" },
  { "https://data.gov.si",               "podatki.gov.si",     "SI" },
  { "https://data.norge.no",             "data.norge.no",      "NO" },
  { "https://www.opendata.dk",           "opendata.dk",        "DK" },
  { "https://www.suomi.fi/data",         "suomi.fi",           "FI" },
  { "https://data.gov.tw",               "data.gov.tw",        "TW" },
  { "https://data.gov.hk/en-data",       "data.gov.hk",        "HK" },
  { "https://data.gov.sg",               "data.gov.sg",        "SG" },
  { "https://data.gov.my",               "data.gov.my",        "MY" },
  { "https://data.gov.ph",               "data.gov.ph",        "PH" },
  { "https://opendata.go.ke",            "opendata.go.ke",     "KE" },
  { "https://data.gov.gh",               "data.gov.gh",        "GH" },
  { "https://data.gov.za",               "data.gov.za",        "ZA" },
  { "https://data.gov.in",               "data.gov.in",        "IN" },
  { "https://data.gov.bh",               "data.gov.bh",        "BH" },
  { "https://www.data.abudhabi",         "data.abudhabi",      "AE" },
  { "https://data.buenosaires.gob.ar",   "data.buenosaires",   "AR" },
};
static const int N_PORTALS = (int)(sizeof(PORTALS) / sizeof(PORTALS[0]));

/* one CKAN package (dataset) → one intel_item. Returns 1 if emitted. */
static int emit_dataset(intel_sink *sink, const cJSON *pkg,
                        const ckan_portal *pt) {
  if (!pkg) return 0;
  const char *title = jo_sv(pkg, "title");
  const char *name  = jo_sv(pkg, "name");          /* url slug */
  if (!title) title = name;
  if (!title) return 0;

  const char *notes = jo_sv(pkg, "notes");         /* description */
  const char *meta  = jo_sv(pkg, "metadata_modified");
  const char *lic   = jo_sv(pkg, "license_title");

  /* owning organization */
  const cJSON *org = cJSON_GetObjectItem(pkg, "organization");
  const char *orgname = org ? jo_sv(org, "title") : NULL;
  if (!orgname && org) orgname = jo_sv(org, "name");

  /* canonical dataset URL: <base>/dataset/<slug> */
  char link[900];
  if (name) snprintf(link, sizeof link, "%s/dataset/%s", pt->base, name);
  else      snprintf(link, sizeof link, "%s", pt->base);

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "title", title);
  if (orgname) cJSON_AddStringToObject(data, "organization", orgname);
  if (notes)   cJSON_AddStringToObject(data, "notes", notes);
  if (lic)     cJSON_AddStringToObject(data, "license", lic);
  cJSON_AddStringToObject(data, "portal", pt->label);
  cJSON_AddStringToObject(data, "country", pt->country);
  cJSON_AddStringToObject(data, "url", link);
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "OPENDATA_CKAN");
  cJSON_AddStringToObject(props, "portal", pt->label);
  cJSON_AddStringToObject(props, "country", pt->country);
  if (orgname) cJSON_AddStringToObject(props, "organization", orgname);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = link;
  it.title           = title;
  it.summary         = orgname ? orgname : pt->label;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = meta;
  it.link            = link;
  it.record_type     = "ckan-dataset";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"OPENDATA_CKAN\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Query one portal's package_search and emit up to CKAN_PER_PORTAL datasets.
 * Returns number emitted (0 = honest empty for this portal). */
static int query_portal(const source_ctx *ctx, intel_sink *sink,
                        const ckan_portal *pt, const char *enc) {
  char url[1200];
  snprintf(url, sizeof url,
    "%s/api/3/action/package_search?q=%s&rows=5", pt->base, enc);

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "ckan");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  /* CKAN envelope: { "success": true, "result": { "results": [...] } } */
  int emitted = 0;
  const cJSON *ok = cJSON_GetObjectItem(root, "success");
  const cJSON *result = cJSON_GetObjectItem(root, "result");
  if ((!ok || cJSON_IsTrue(ok)) && result) {
    const cJSON *results = cJSON_GetObjectItem(result, "results");
    if (cJSON_IsArray(results)) {
      const cJSON *r;
      cJSON_ArrayForEach(r, results) {
        if (emitted >= CKAN_PER_PORTAL) break;
        emitted += emit_dataset(sink, r, pt);
      }
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  /* Contract R3: no entity is a NO-OP, not a failure. This is an on-demand
   * pivot (update_interval_sec = 0) so the scheduler does not reach it today
   * — but scheduler.c logs any non-zero rc as status=error and feeds
   * anomaly_detect(), so the moment anyone gives this source an interval it
   * would quarantine itself on its very first run, for the crime of not
   * having been asked a question. */
  if (!q || !*q) return 0;

  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  int total = 0;
  for (int i = 0; i < N_PORTALS; i++) {
    if (ctx->cancel && *ctx->cancel) break;
    total += query_portal(ctx, sink, &PORTALS[i], enc);
  }

  free(enc);
  fprintf(stderr, "[ckan] emitted %d across %d portals\n", total, N_PORTALS);
  return 0;   /* honest empty is not an error */
}

static const source_def opendata_ckan_def = {
  .id = "OPENDATA_CKAN", .collector = "osint",
  .name = "CKAN Open-Data Portals", .name_ja = "CKAN オープンデータポータル",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "dataset",
  .url = "internal://osint/opendata_ckan",
  .description = "Federated dataset search across ~40 national CKAN open-data portals (keyless): title, org, portal, URL",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(opendata_ckan_def)
