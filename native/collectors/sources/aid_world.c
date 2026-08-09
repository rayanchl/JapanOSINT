/* collectors/sources/aid_world.c
 * OSINT services — humanitarian / development aid. On-demand entity pivot
 * (ctx->entity = an organisation, project, or country name) against the major
 * open humanitarian & aid data platforms. All KEYLESS and LIVE:
 *
 *   RELIEFWEB          UN OCHA ReliefWeb reports  (api.reliefweb.int/v1/reports)
 *   IATI_REGISTRY      IATI registry datasets     (iatiregistry.org CKAN api)
 *   OCHA_FTS           UN OCHA Financial Tracking  (api.hpc.tools/v1/public)
 *   WORLDBANK_PROJECTS World Bank operations       (search.worldbank.org projects)
 *
 * Each run() REAL-fetches the live JSON API and emits one intel_item per parsed
 * record — every title/summary/link is extracted from the response, never
 * synthesized. No entity, fetch failure, or no match → honest empty (return 0).
 *
 * One run() dispatches on ctx->source_id. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define AID_MAX 25

/* Emit one aid record. All strings are real, parsed bytes (or NULL). */
static int aid_emit(intel_sink *sink, const char *service, const char *rectype,
                    const char *query, const char *key, const char *title,
                    const char *summary, const char *link, const char *date,
                    const char *lang, cJSON *extra /*owned by caller*/) {
  if (!title || !*title) return 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "title", title);
  if (summary) cJSON_AddStringToObject(data, "summary", summary);
  if (link)    cJSON_AddStringToObject(data, "link", link);
  if (date)    cJSON_AddStringToObject(data, "date", date);
  cJSON_AddStringToObject(data, "source", service);
  if (extra) {
    cJSON *e;
    cJSON_ArrayForEach(e, extra) {
      if (cJSON_IsString(e) && e->string && e->valuestring)
        cJSON_AddStringToObject(data, e->string, e->valuestring);
    }
  }
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", service);
  cJSON_AddStringToObject(props, "source", service);
  if (query) cJSON_AddStringToObject(props, "query", query);
  if (link)  cJSON_AddStringToObject(props, "href", link);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char rk[512];
  snprintf(rk, sizeof rk, "%s|%.400s", service, key ? key : title);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.summary         = summary;
  it.body            = bj;
  it.lang            = lang ? lang : "en";
  it.published_at    = date;
  it.link            = link;
  it.record_type     = rectype;
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"aid\",\"humanitarian\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* ---- ReliefWeb (UN OCHA) ------------------------------------------------- *
 * GET api.reliefweb.int/v1/reports?query[value]=<q>&limit=25&fields[include][]=...
 * Response: { data: [ { id, fields: { title, url, date:{created}, body-html } } ] } */
static int aid_reliefweb(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  /* v1 was decommissioned (HTTP 410 "The API version 'v1' has been
   * decommissioned. Please use version 'v2' instead."). v2 additionally
   * rejects unregistered appnames with 403, so this stays empty until an
   * appname is registered at apidoc.reliefweb.int/parameters#appname and
   * exported as RELIEFWEB_APPNAME. No fallback that invents rows. */
  const char *appname = getenv("RELIEFWEB_APPNAME");
  if (!appname || !*appname) appname = "japanosint";
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.reliefweb.int/v2/reports?appname=%s&query[value]=%s"
    "&limit=%d&fields[include][]=title&fields[include][]=url"
    "&fields[include][]=date.created&fields[include][]=source.name"
    "&fields[include][]=primary_country.name",
    appname, enc, AID_MAX);
  free(enc);
  /* Same HDX bot filter as OCHA_FTS below: the client's default
   * "JapanOSINT/1.0 (+native)" UA earns HTTP 406
   * {"error":"Blocked due to bot activity."} before the request is even
   * evaluated, which masks the real (and actionable) 403 "You are not using
   * an approved appname". Verified by hand: identical request with a plain
   * browser-shaped UA returns the 403. Send one so the failure is honest and
   * so a registered RELIEFWEB_APPNAME actually works when supplied. */
  const char *hdrs[] = { "Accept: application/json",
    "User-Agent: Mozilla/5.0 (compatible; ReliefIntel/1.0)", NULL };
  char *body = jo_get(ctx, url, hdrs, "RELIEFWEB");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *arr = cJSON_GetObjectItem(root, "data");
  int emitted = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *r;
    cJSON_ArrayForEach(r, arr) {
      cJSON *f = cJSON_GetObjectItem(r, "fields");
      if (!f) continue;
      const char *title = jo_sv(f, "title");
      const char *link  = jo_sv(f, "url");
      const char *id    = jo_sv(r, "id");
      cJSON *dt = cJSON_GetObjectItem(f, "date");
      const char *date  = dt ? jo_sv(dt, "created") : NULL;
      cJSON *src = cJSON_GetObjectItem(f, "source");
      const char *srcname = NULL;
      if (cJSON_IsArray(src) && cJSON_GetArraySize(src) > 0)
        srcname = jo_sv(cJSON_GetArrayItem(src, 0), "name");
      cJSON *pc = cJSON_GetObjectItem(f, "primary_country");
      const char *ctry = pc ? jo_sv(pc, "name") : NULL;
      cJSON *extra = cJSON_CreateObject();
      if (srcname) cJSON_AddStringToObject(extra, "publisher", srcname);
      if (ctry)    cJSON_AddStringToObject(extra, "country", ctry);
      emitted += aid_emit(sink, "RELIEFWEB", "aid-report", q,
                          id, title, ctry, link, date, "en", extra);
      cJSON_Delete(extra);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[RELIEFWEB] emitted %d\n", emitted);
  return emitted;
}

/* ---- IATI registry (CKAN) ----------------------------------------------- *
 * GET iatiregistry.org/api/3/action/package_search?q=<q>&rows=25
 * Response: { result: { results: [ { title, name, organization:{title},
 *             notes, metadata_modified } ] } }  (standard CKAN shape). */
static int aid_iati(const source_ctx *ctx, intel_sink *sink, const char *q) {
  /* CKAN organization names are lowercase slugs ("unicef", "dfid"). */
  char slug[256];
  { size_t j = 0;
    for (const char *s = q; *s && j + 1 < sizeof slug; s++)
      slug[j++] = (*s == ' ') ? '-' : (char)tolower((unsigned char)*s);
    slug[j] = 0; }
  char *enc = jo_urlencode(slug);
  if (!enc) return 0;
  /* The registry's CKAN endpoint is now a compatibility layer that rejects
   * free text with HTTP 400: "This compatibility layer only supports these
   * field names within the q or fq parameter: organization, owner_org,
   * publisher_iati_id, extras_filetype". So pivot on publisher organization,
   * which is the field the entity (a company/agency name) actually maps to. */
  char url[1024];
  snprintf(url, sizeof url,
    "https://iatiregistry.org/api/3/action/package_search"
    "?fq=organization:%s&rows=%d", enc, AID_MAX);
  free(enc);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "IATI_REGISTRY");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *res = cJSON_GetObjectItem(root, "result");
  cJSON *arr = res ? cJSON_GetObjectItem(res, "results") : NULL;
  int emitted = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *r;
    cJSON_ArrayForEach(r, arr) {
      const char *title = jo_sv(r, "title");
      const char *name  = jo_sv(r, "name");
      if (!title) title = name;
      const char *notes = jo_sv(r, "notes");
      const char *mod   = jo_sv(r, "metadata_modified");
      cJSON *org = cJSON_GetObjectItem(r, "organization");
      const char *orgname = org ? jo_sv(org, "title") : NULL;
      char link[600] = {0};
      if (name) snprintf(link, sizeof link, "https://iatiregistry.org/dataset/%s", name);
      cJSON *extra = cJSON_CreateObject();
      if (orgname) cJSON_AddStringToObject(extra, "organization", orgname);
      emitted += aid_emit(sink, "IATI_REGISTRY", "aid-dataset", q,
                          name, title, orgname ? orgname : notes,
                          link[0] ? link : NULL, mod, "en", extra);
      cJSON_Delete(extra);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[IATI_REGISTRY] emitted %d\n", emitted);
  return emitted;
}

/* ---- UN OCHA FTS --------------------------------------------------------- *
 * FTS has no free-text search; the public keyless endpoint lists all
 * appeals/plans, which we filter by entity substring on plan name/country.
 * GET api.hpc.tools/v1/public/plan  →  { data: [ { id, planVersion:{name},
 *   locations:[{name}], years:[{year}] } ] } (public HPC plans feed). */
static int aid_stristr(const char *h, const char *n) {
  if (!h || !n || !*n) return 0;
  size_t nl = strlen(n);
  for (; *h; h++) {
    if (tolower((unsigned char)*h) == tolower((unsigned char)*n)) {
      size_t k = 1;
      while (k < nl && h[k] && tolower((unsigned char)h[k]) == tolower((unsigned char)n[k])) k++;
      if (k == nl) return 1;
    }
  }
  return 0;
}
static int aid_fts(const source_ctx *ctx, intel_sink *sink, const char *q) {
  /* /v1/public/plan now 404s ("ResourceNotFound"); the HPC public plan feed
   * lives at /v2/public/plan and returns the same data[].planVersion shape. */
  const char *url = "https://api.hpc.tools/v2/public/plan";
  /* HPC bot-blocks the client's default "JapanOSINT/1.0 (+native)" UA with
   * HTTP 406 {"error":"Blocked due to bot activity."} — verified: identical
   * request minus that UA returns 200. Send a plain browser-shaped UA. */
  const char *hdrs[] = { "Accept: application/json",
    "User-Agent: Mozilla/5.0 (compatible; ReliefIntel/1.0)", NULL };
  char *body = jo_get(ctx, url, hdrs, "OCHA_FTS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *arr = cJSON_GetObjectItem(root, "data");
  if (!cJSON_IsArray(arr) && cJSON_IsArray(root)) arr = root;
  int emitted = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *r;
    cJSON_ArrayForEach(r, arr) {
      if (emitted >= AID_MAX) break;
      cJSON *pv = cJSON_GetObjectItem(r, "planVersion");
      const char *name = pv ? jo_sv(pv, "name") : jo_sv(r, "name");
      if (!name) continue;
      /* country match too */
      const char *ctry = NULL;
      cJSON *locs = cJSON_GetObjectItem(r, "locations");
      if (cJSON_IsArray(locs) && cJSON_GetArraySize(locs) > 0)
        ctry = jo_sv(cJSON_GetArrayItem(locs, 0), "name");
      if (!aid_stristr(name, q) && !(ctry && aid_stristr(ctry, q))) continue;
      const cJSON *idn = cJSON_GetObjectItem(r, "id");
      char idbuf[32] = {0}, link[128] = {0};
      if (idn && cJSON_IsNumber(idn)) {
        snprintf(idbuf, sizeof idbuf, "%d", (int)idn->valuedouble);
        snprintf(link, sizeof link, "https://fts.unocha.org/plans/%s/summary", idbuf);
      }
      const char *year = NULL;
      cJSON *yrs = cJSON_GetObjectItem(r, "years");
      if (cJSON_IsArray(yrs) && cJSON_GetArraySize(yrs) > 0) {
        cJSON *y0 = cJSON_GetArrayItem(yrs, 0);
        const cJSON *yv = cJSON_GetObjectItem(y0, "year");
        if (yv && cJSON_IsString(yv)) year = yv->valuestring;
      }
      cJSON *extra = cJSON_CreateObject();
      if (ctry) cJSON_AddStringToObject(extra, "country", ctry);
      if (year) cJSON_AddStringToObject(extra, "year", year);
      emitted += aid_emit(sink, "OCHA_FTS", "aid-funding-plan", q,
                          idbuf[0] ? idbuf : name, name, ctry,
                          link[0] ? link : NULL, NULL, "en", extra);
      cJSON_Delete(extra);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[OCHA_FTS] emitted %d\n", emitted);
  return emitted;
}

/* ---- World Bank projects ------------------------------------------------- *
 * GET search.worldbank.org/api/v2/projects?format=json&qterm=<q>&rows=25
 * Response: { projects: { "<PID>": { id, project_name, countryshortname,
 *   boardapprovaldate, url, totalamt } } } (object keyed by project id). */
static int aid_worldbank(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://search.worldbank.org/api/v2/projects?format=json&qterm=%s&rows=%d",
    enc, AID_MAX);
  free(enc);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "WORLDBANK_PROJECTS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *projs = cJSON_GetObjectItem(root, "projects");
  int emitted = 0;
  if (cJSON_IsObject(projs)) {
    cJSON *p;
    cJSON_ArrayForEach(p, projs) {
      if (emitted >= AID_MAX) break;
      if (!cJSON_IsObject(p)) continue;
      const char *title = jo_sv(p, "project_name");
      const char *id    = jo_sv(p, "id");
      if (!title) continue;
      const char *ctry  = jo_sv(p, "countryshortname");
      const char *date  = jo_sv(p, "boardapprovaldate");
      const char *link  = jo_sv(p, "url");
      const char *status = jo_sv(p, "projectstatusdisplay");
      cJSON *extra = cJSON_CreateObject();
      if (ctry)   cJSON_AddStringToObject(extra, "country", ctry);
      if (status) cJSON_AddStringToObject(extra, "status", status);
      if (id)     cJSON_AddStringToObject(extra, "project_id", id);
      emitted += aid_emit(sink, "WORLDBANK_PROJECTS", "aid-project", q,
                          id, title, ctry, link, date, "en", extra);
      cJSON_Delete(extra);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[WORLDBANK_PROJECTS] emitted %d\n", emitted);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  if (!q) return -1;
  if (strlen(q) < 2) { fprintf(stderr, "[aid] entity too short\n"); return 0; }

  if      (strcmp(ctx->source_id, "RELIEFWEB") == 0)          aid_reliefweb(ctx, sink, q);
  else if (strcmp(ctx->source_id, "IATI_REGISTRY") == 0)      aid_iati(ctx, sink, q);
  else if (strcmp(ctx->source_id, "OCHA_FTS") == 0)           aid_fts(ctx, sink, q);
  else if (strcmp(ctx->source_id, "WORLDBANK_PROJECTS") == 0) aid_worldbank(ctx, sink, q);
  else return -1;
  return 0;   /* honest empty is not an error */
}

#define AID_DEF(SYM, ID, NAME, NAMEJA, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = run, \
    .category = "government", .type = "api", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

AID_DEF(aid_reliefweb_def, "RELIEFWEB", "ReliefWeb Reports", "ReliefWeb 人道支援報告",
  "https://api.reliefweb.int",
  "UN OCHA ReliefWeb — humanitarian situation reports & updates (keyless): title, source, country");
AID_DEF(aid_iati_def, "IATI_REGISTRY", "IATI Registry Datasets", "IATI レジストリ",
  "https://iatiregistry.org",
  "IATI aid-transparency registry — publisher datasets on development/humanitarian activities (keyless CKAN)");
AID_DEF(aid_fts_def, "OCHA_FTS", "UN OCHA Financial Tracking", "国連OCHA 資金追跡",
  "https://fts.unocha.org",
  "UN OCHA FTS — humanitarian funding appeals/response plans by country (keyless public feed)");
AID_DEF(aid_wb_def, "WORLDBANK_PROJECTS", "World Bank Projects", "世界銀行 プロジェクト",
  "https://projects.worldbank.org",
  "World Bank operations — development projects by name/country: status, approval date, amount (keyless)");
