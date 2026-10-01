/* collectors/sources/opendata_socrata.c
 * OSINT service — OPENDATA_SOCRATA. On-demand entity pivot against the public
 * Socrata (Tyler Technologies) open-data ecosystem via its keyless Discovery
 * API (api.us.socrata.com/api/catalog/v1). We fan the entity query across a
 * curated table of ~20 well-known Socrata portals (NYC, CDC, WA, Chicago, …)
 * and emit one intel_item per matched dataset (name + owning domain + real
 * resolvable permalink).
 *
 * Discovery request per portal:
 *   GET https://api.us.socrata.com/api/catalog/v1?q=<entity>&domains=<domain>&limit=5
 * Response shape: { "results": [ { "resource": {name,description,id,...},
 *   "metadata": {domain}, "permalink": "...", "link": "..." }, ... ] }
 *
 * Keyless / LIVE. Only real parsed API fields are emitted; fetch failure or no
 * matches → honest empty (return 0). Nothing synthesized. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Curated set of major public Socrata portals (host = catalog "domain"). */
static const char *SOCRATA_DOMAINS[] = {
  "data.cityofnewyork.us",   /* NYC Open Data              */
  "data.cdc.gov",            /* US CDC                     */
  "data.wa.gov",             /* Washington State           */
  "data.cityofchicago.org",  /* Chicago                    */
  "data.seattle.gov",        /* Seattle                    */
  "data.sfgov.org",          /* San Francisco              */
  "data.lacity.org",         /* Los Angeles                */
  "data.austintexas.gov",    /* Austin                     */
  "data.texas.gov",          /* Texas                      */
  "data.ny.gov",             /* New York State             */
  "data.ct.gov",             /* Connecticut                */
  "data.colorado.gov",       /* Colorado                   */
  "data.oregon.gov",         /* Oregon                     */
  "data.maryland.gov",       /* Maryland                   */
  "data.montgomerycountymd.gov",
  "data.baltimorecity.gov",  /* Baltimore                  */
  "data.nola.gov",           /* New Orleans                */
  "data.honolulu.gov",       /* Honolulu                   */
  "data.kcmo.org",           /* Kansas City                */
  "opendata.dc.gov",         /* Washington DC              */
  "healthdata.gov",          /* US HHS HealthData          */
  "data.medicare.gov",       /* CMS / Medicare             */
};
static const int SOCRATA_N =
  (int)(sizeof(SOCRATA_DOMAINS) / sizeof(SOCRATA_DOMAINS[0]));

/* one Discovery result → one intel_item. Returns 1 if emitted. */
static int emit_dataset(intel_sink *sink, const cJSON *r) {
  if (!r) return 0;
  const cJSON *res  = cJSON_GetObjectItem(r, "resource");
  const cJSON *meta = cJSON_GetObjectItem(r, "metadata");
  const char *name  = res ? jo_sv(res, "name") : NULL;
  const char *perma = jo_sv(r, "permalink");
  const char *link  = jo_sv(r, "link");
  const char *url   = perma ? perma : link;
  if (!name || !url) return 0;

  const char *desc   = res ? jo_sv(res, "description") : NULL;
  const char *dsid   = res ? jo_sv(res, "id") : NULL;
  const char *dtype  = res ? jo_sv(res, "type") : NULL;
  const char *updat  = res ? jo_sv(res, "updatedAt") : NULL;
  const char *domain = meta ? jo_sv(meta, "domain") : NULL;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "name", name);
  if (domain) cJSON_AddStringToObject(data, "domain", domain);
  if (desc)   cJSON_AddStringToObject(data, "description", desc);
  if (dsid)   cJSON_AddStringToObject(data, "dataset_id", dsid);
  if (dtype)  cJSON_AddStringToObject(data, "resource_type", dtype);
  cJSON_AddStringToObject(data, "link", url);
  cJSON_AddStringToObject(data, "source", "Socrata Open Data");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "OPENDATA_SOCRATA");
  cJSON_AddStringToObject(props, "source", "Socrata Open Data");
  if (domain) cJSON_AddStringToObject(props, "domain", domain);
  if (dsid)   cJSON_AddStringToObject(props, "dataset_id", dsid);
  cJSON_AddStringToObject(props, "link", url);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = dsid ? dsid : url;
  it.title           = name;
  it.summary         = domain;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = updat;
  it.link            = url;
  it.record_type     = "socrata-dataset";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"OPENDATA_SOCRATA\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int query_domain(const source_ctx *ctx, intel_sink *sink,
                        const char *enc, const char *domain) {
  char url[512];
  snprintf(url, sizeof url,
    "https://api.us.socrata.com/api/catalog/v1?q=%s&domains=%s&limit=5",
    enc, domain);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "opendata-socrata");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    cJSON *r;
    cJSON_ArrayForEach(r, results) emitted += emit_dataset(sink, r);
  }
  cJSON_Delete(root);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  /* Contract R3: no entity is a NO-OP, not a failure — see the same note in
   * opendata_ckan.c. -1 here would make scheduler.c quarantine the source on
   * its first run if it were ever given an interval. */
  if (!q || !*q) return 0;

  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  int emitted = 0;
  for (int i = 0; i < SOCRATA_N; i++)
    emitted += query_domain(ctx, sink, enc, SOCRATA_DOMAINS[i]);

  free(enc);
  fprintf(stderr, "[opendata-socrata] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def opendata_socrata_def = {
  .id = "OPENDATA_SOCRATA", .collector = "osint",
  .name = "Socrata Open Data Search", .name_ja = "Socrata オープンデータ検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "dataset",
  .url = "https://api.us.socrata.com/api/catalog/v1",
  .description = "Socrata Discovery API across ~20 US gov open-data portals (NYC/CDC/WA/Chicago…): dataset name, domain, link (keyless)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(opendata_socrata_def)
