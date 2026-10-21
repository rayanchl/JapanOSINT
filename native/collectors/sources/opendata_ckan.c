/* collectors/sources/opendata_ckan.c
 * OSINT service — OPENDATA_CKAN. On-demand entity pivot across ~40 national
 * open-data portals that all run CKAN, which exposes a uniform Action API:
 *   GET <portal>/api/3/action/package_search?q=<entity>&rows=1000&start=<n>
 * For each portal we real-fetch that endpoint, parse result.results[], and emit
 * one intel_item per dataset (title + owning organization + portal + canonical
 * dataset URL). Keyless and LIVE.
 *
 * EVERY MATCH, NOT THE FIRST FIVE (2026-10-21). This asked each portal for
 * `rows=5` and never paged, so an entity matching thousands of datasets on a
 * portal stored five of them and said nothing about the rest — the "limit
 * asked of the upstream" discard CLAUDE.md rule 2 names, forty portals over.
 * Each portal is now walked with `start` to its own `result.count`. The walk
 * steps by what the page HELD, not by `rows`, because a portal whose
 * ckan.search.rows_max is below 1000 answers with fewer and is still paging.
 * What a run does not reach — the page ceiling (jo_page_max, $JO_PAGE_MAX), a
 * later page that fails, a portal that ignores `start`, a cancel — is filed as
 * a collector-truncation-notice naming the portal, the records used and the
 * portal's own total. Portals that did not answer at all, and portals a
 * cancelled run never asked, go in one coverage notice for the query.
 *
 * Only real, parsed API fields are emitted; a portal that errors, is offline,
 * JS-only, or returns no matches simply contributes nothing (honest empty per
 * portal). Nothing here fabricates dataset titles, orgs, or URLs. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/jocore.h"
#include "lib/pagewalk.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Records asked for per request. 1000 is CKAN's default ckan.search.rows_max;
 * it is a page SIZE, not a bound — the walk below continues to result.count. */
#define CKAN_ROWS 1000

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

/* Walk one portal's package_search for `enc` to its own result.count.
 * Returns the number emitted, or -1 when the FIRST page did not answer
 * (*status_out then holds the HTTP status, 0 for a transport failure). */
static int query_portal(const source_ctx *ctx, intel_sink *sink,
                        const ckan_portal *pt, const char *enc, const char *q,
                        int page_max, long *status_out) {
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  long start = 0, total = -1, used = 0, fail_status = 0;
  int pages = 0, failed = 0;
  const char *reason = NULL, *remedy = NULL;
  char prev_first[300] = "", url[1200];
  *status_out = 0;

  for (;;) {
    if (ctx->cancel && *ctx->cancel) {
      reason = "the run was cancelled before this portal's matches were all read";
      remedy = "re-run the search";
      break;
    }
    if (pages >= page_max) {
      reason = "the page ceiling stopped the walk before the portal's own total";
      remedy = "raise JO_PAGE_MAX, or narrow the query";
      break;
    }
    snprintf(url, sizeof url,
             "%s/api/3/action/package_search?q=%s&rows=%d&start=%ld",
             pt->base, enc, CKAN_ROWS, start);
    http_response hr = {0};
    int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 30000, 1, &hr);
    cJSON *root = (rc == 0 && hr.status == 200 && hr.body)
                  ? cJSON_Parse(hr.body) : NULL;
    long st = hr.status;
    http_response_free(&hr);
    const cJSON *ok = root ? cJSON_GetObjectItem(root, "success") : NULL;
    const cJSON *result = root ? cJSON_GetObjectItem(root, "result") : NULL;
    const cJSON *results = result ? cJSON_GetObjectItem(result, "results") : NULL;
    if (!root || (ok && !cJSON_IsTrue(ok)) || !cJSON_IsArray(results)) {
      cJSON_Delete(root);
      if (pages == 0) { *status_out = st; return -1; }
      fail_status = st;
      failed = 1;
      reason = "a later page did not answer, so the walk stopped short";
      remedy = "re-run the search; a persistent failure needs the portal checked";
      break;
    }
    if (pages == 0) {
      const cJSON *cnt = cJSON_GetObjectItem(result, "count");
      if (cJSON_IsNumber(cnt)) total = (long)cnt->valuedouble;
    }
    int n = cJSON_GetArraySize(results);
    if (n == 0) { cJSON_Delete(root); break; }        /* the end of the data */

    /* A portal that ignores `start` serves page 1 again: stop rather than
     * re-emit it until the ceiling and report the multiple as coverage. */
    const cJSON *f = cJSON_GetArrayItem(results, 0);
    const char *fid = f ? jo_sv(f, "id") : NULL;
    if (!fid && f) fid = jo_sv(f, "name");
    if (fid && pages > 0 && !strcmp(fid, prev_first)) {
      cJSON_Delete(root);
      reason = "the portal ignored `start` and served the same page again";
      remedy = "this portal cannot be paged; only its first page is read";
      break;
    }
    snprintf(prev_first, sizeof prev_first, "%s", fid ? fid : "");

    const cJSON *r;
    cJSON_ArrayForEach(r, results) used += emit_dataset(sink, r, pt);
    cJSON_Delete(root);
    pages++;
    start += n;
    if (total >= 0 && start >= total) break;           /* its own total */
  }

  if (reason && (total < 0 || start < total)) {
    char qkey[600];
    snprintf(qkey, sizeof qkey, "%s: %s", pt->label, q);
    cJSON *extra = cJSON_CreateObject();
    if (extra) {
      cJSON_AddStringToObject(extra, "portal", pt->label);
      cJSON_AddStringToObject(extra, "country", pt->country);
      cJSON_AddNumberToObject(extra, "pages_read", pages);
      cJSON_AddNumberToObject(extra, "next_start", (double)start);
      cJSON_AddStringToObject(extra, "url", url);
      if (failed)
        cJSON_AddNumberToObject(extra, "failed_page_status", (double)fail_status);
    }
    jo_truncation_notice_ex(sink, "OPENDATA_CKAN", qkey, used, total, reason,
                            remedy, extra);
  }
  return (int)used;
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

  int page_max = jo_page_max(NULL);
  int total = 0, answered = 0;
  cJSON *unreachable = cJSON_CreateArray(), *not_asked = cJSON_CreateArray();
  for (int i = 0; i < N_PORTALS; i++) {
    if (ctx->cancel && *ctx->cancel) {
      if (not_asked) cJSON_AddItemToArray(not_asked,
                                          cJSON_CreateString(PORTALS[i].label));
      continue;
    }
    long status = 0;
    int n = query_portal(ctx, sink, &PORTALS[i], enc, q, page_max, &status);
    if (n < 0) {
      cJSON *u = cJSON_CreateObject();
      if (u && unreachable) {
        cJSON_AddStringToObject(u, "portal", PORTALS[i].label);
        cJSON_AddNumberToObject(u, "status", (double)status);
        cJSON_AddItemToArray(unreachable, u);
      } else cJSON_Delete(u);
      continue;
    }
    answered++;
    total += n;
  }

  /* Rule 1: a portal that did not answer is said, not silently skipped, and
   * neither is one a cancelled run never asked. One row per query. */
  int nu = unreachable ? cJSON_GetArraySize(unreachable) : 0;
  int nn = not_asked ? cJSON_GetArraySize(not_asked) : 0;
  if (nu || nn) {
    char qkey[600], why[200];
    snprintf(qkey, sizeof qkey, "coverage: %s", q);
    snprintf(why, sizeof why, "%d of %d portals answered (%d did not, %d not asked"
             " because the run was cancelled)", answered, N_PORTALS, nu, nn);
    cJSON *extra = cJSON_CreateObject();
    if (extra) {
      cJSON_AddNumberToObject(extra, "portals_total", N_PORTALS);
      cJSON_AddNumberToObject(extra, "portals_answered", answered);
      cJSON_AddItemToObject(extra, "unreachable", unreachable); unreachable = NULL;
      cJSON_AddItemToObject(extra, "not_asked", not_asked); not_asked = NULL;
    }
    jo_truncation_notice_ex(sink, "OPENDATA_CKAN", qkey, total, -1, why,
                            nn ? "re-run the search" :
                            "a portal that never answers needs its entry checked",
                            extra);
  }
  cJSON_Delete(unreachable);
  cJSON_Delete(not_asked);

  free(enc);
  fprintf(stderr, "[ckan] emitted %d across %d of %d portals\n",
          total, answered, N_PORTALS);
  return 0;   /* honest empty is not an error */
}

static const source_def opendata_ckan_def = {
  .id = "OPENDATA_CKAN", .collector = "osint",
  .name = "CKAN Open-Data Portals", .name_ja = "CKAN オープンデータポータル",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "dataset",
  .url = "internal://osint/opendata_ckan",
  .description = "Federated dataset search across ~40 national CKAN open-data portals (keyless): every matching dataset per portal, paged to the portal's own total; title, org, portal, URL",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(opendata_ckan_def)
