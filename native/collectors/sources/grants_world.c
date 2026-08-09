/* collectors/sources/grants_world.c
 * OSINT services — WORLD RESEARCH GRANTS. On-demand entity pivot against three
 * keyless, LIVE public research-funding catalogs. One run() switches on
 * ctx->source_id (three related sources share this file):
 *
 *   NIH_REPORTER — US NIH RePORTER v2 (POST api.reporter.nih.gov/v2/projects/
 *                  search, JSON body {criteria:{text_search:{...}}}). One item
 *                  per funded project.
 *   NSF_AWARDS   — US NSF awards (GET api.nsf.gov/services/v1/awards.json?
 *                  keyword=<entity>). One item per award.
 *   UKRI_GRANTS  — UK Research & Innovation Gateway to Research (GET
 *                  gtr.ukri.org/gtr/api/projects?q=<entity>, Accept
 *                  application/vnd.rcuk.gtr.json-v7). One item per project.
 *
 * All keyless. Only real, parsed API fields are emitted; fetch failure / no
 * matches → honest empty (return 0). Nothing is fabricated. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- NIH RePORTER --------------------------------------------------------- */
/* one project → one intel_item. Returns 1 if emitted. */
static int nih_emit(intel_sink *sink, const cJSON *p) {
  if (!p) return 0;
  const char *title = jo_sv(p, "project_title");
  const char *num   = jo_sv(p, "project_num");
  if (!title && !num) return 0;

  const cJSON *org = cJSON_GetObjectItem(p, "organization");
  const char *orgn = org ? jo_sv(org, "org_name") : NULL;
  const cJSON *agc = cJSON_GetObjectItem(p, "agency_ic_admin");
  const char *agcn = agc ? jo_sv(agc, "name") : NULL;
  const char *start = jo_sv(p, "project_start_date");
  double fy = 0, cost = 0; int have_fy = jo_num(p, "fiscal_year", &fy);
  int have_cost = jo_num(p, "award_amount", &cost);
  const char *url = jo_sv(p, "project_detail_url");

  /* PI list */
  char *pis = NULL;
  const cJSON *pi = cJSON_GetObjectItem(p, "principal_investigators");
  if (cJSON_IsArray(pi)) {
    size_t cap = 128, len = 0; pis = malloc(cap); if (pis) pis[0] = 0;
    int n = 0; const cJSON *x;
    cJSON_ArrayForEach(x, pi) {
      const char *nm = jo_sv(x, "full_name");
      if (!nm || !pis) continue;
      size_t need = len + strlen(nm) + 3;
      if (need > cap) { cap = need * 2; char *t = realloc(pis, cap); if (!t) break; pis = t; }
      if (n) { strcpy(pis + len, ", "); len += 2; }
      strcpy(pis + len, nm); len += strlen(nm);
      if (++n >= 20) break;
    }
    if (!n && pis) { free(pis); pis = NULL; }
  }

  cJSON *data = cJSON_CreateObject();
  if (title) cJSON_AddStringToObject(data, "project_title", title);
  if (num)   cJSON_AddStringToObject(data, "project_num", num);
  if (orgn)  cJSON_AddStringToObject(data, "organization", orgn);
  if (agcn)  cJSON_AddStringToObject(data, "agency", agcn);
  if (pis)   cJSON_AddStringToObject(data, "principal_investigators", pis);
  if (have_fy)   cJSON_AddNumberToObject(data, "fiscal_year", fy);
  if (have_cost) cJSON_AddNumberToObject(data, "award_amount", cost);
  cJSON_AddStringToObject(data, "source", "NIH RePORTER");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "NIH_REPORTER");
  cJSON_AddStringToObject(props, "source", "NIH RePORTER");
  if (num)  cJSON_AddStringToObject(props, "project_num", num);
  if (orgn) cJSON_AddStringToObject(props, "organization", orgn);
  if (have_cost) cJSON_AddNumberToObject(props, "award_amount", cost);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = num ? num : title;
  it.title           = title ? title : num;
  it.summary         = orgn;
  it.body            = bj;
  it.author          = pis;
  it.lang            = "en";
  it.published_at    = start;
  it.link            = url;
  it.record_type     = "nih-reporter-project";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"NIH_REPORTER\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); free(pis);
  return rc >= 0 ? 1 : 0;
}

static int nih_run(const source_ctx *ctx, intel_sink *sink, const char *q) {
  /* JSON-escape the entity for embedding in the request body. */
  char esc[512]; size_t j = 0;
  for (size_t i = 0; q[i] && j < sizeof esc - 2; i++) {
    unsigned char c = (unsigned char)q[i];
    if (c == '"' || c == '\\') { esc[j++] = '\\'; esc[j++] = (char)c; }
    else if (c == '\n' || c == '\r' || c == '\t') esc[j++] = ' ';
    else esc[j++] = (char)c;
  }
  esc[j] = 0;

  char body[900];
  snprintf(body, sizeof body,
    "{\"criteria\":{\"text_search\":{\"operator\":\"and\","
    "\"search_field\":\"projecttitle\",\"search_text\":\"%s\"}},"
    "\"limit\":20,\"offset\":0}", esc);

  const char *hdrs[] = { "Content-Type: application/json",
                         "Accept: application/json", NULL };
  http_response hr = {0};
  int hc = http_request(ctx->http, "POST",
    "https://api.reporter.nih.gov/v2/projects/search",
    hdrs, body, strlen(body), 20000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    fprintf(stderr, "[nih_reporter] http status=%ld\n", hr.status);
    http_response_free(&hr); return 0;
  }
  cJSON *root = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!root) return 0;
  cJSON *results = cJSON_GetObjectItem(root, "results");
  int emitted = 0;
  if (cJSON_IsArray(results)) {
    cJSON *r; cJSON_ArrayForEach(r, results) emitted += nih_emit(sink, r);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[nih_reporter] emitted %d\n", emitted);
  return 0;
}

/* ---- NSF awards ----------------------------------------------------------- */
static int nsf_emit(intel_sink *sink, const cJSON *a) {
  if (!a) return 0;
  const char *title = jo_sv(a, "title");
  const char *id    = jo_sv(a, "id");            /* award number */
  if (!title && !id) return 0;

  const char *agency  = jo_sv(a, "agency");
  const char *inst    = jo_sv(a, "awardeeName");
  const char *pi      = jo_sv(a, "piFirstName");
  const char *pilast  = jo_sv(a, "piLastName");
  const char *amt     = jo_sv(a, "fundsObligatedAmt");
  const char *date    = jo_sv(a, "date");
  const char *program = jo_sv(a, "fundProgramName");

  char piname[256] = {0};
  if (pi && pilast) snprintf(piname, sizeof piname, "%s %s", pi, pilast);
  else if (pilast)  snprintf(piname, sizeof piname, "%s", pilast);

  char link[256] = {0};
  if (id) snprintf(link, sizeof link,
    "https://www.nsf.gov/awardsearch/showAward?AWD_ID=%s", id);

  cJSON *data = cJSON_CreateObject();
  if (title)   cJSON_AddStringToObject(data, "title", title);
  if (id)      cJSON_AddStringToObject(data, "award_id", id);
  if (agency)  cJSON_AddStringToObject(data, "agency", agency);
  if (inst)    cJSON_AddStringToObject(data, "awardee", inst);
  if (piname[0]) cJSON_AddStringToObject(data, "principal_investigator", piname);
  if (amt)     cJSON_AddStringToObject(data, "funds_obligated", amt);
  if (program) cJSON_AddStringToObject(data, "program", program);
  cJSON_AddStringToObject(data, "source", "NSF");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "NSF_AWARDS");
  cJSON_AddStringToObject(props, "source", "NSF");
  if (id)   cJSON_AddStringToObject(props, "award_id", id);
  if (inst) cJSON_AddStringToObject(props, "awardee", inst);
  if (amt)  cJSON_AddStringToObject(props, "funds_obligated", amt);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = id ? id : title;
  it.title           = title ? title : id;
  it.summary         = inst;
  it.body            = bj;
  it.author          = piname[0] ? piname : NULL;
  it.lang            = "en";
  it.published_at    = date;
  it.link            = link[0] ? link : NULL;
  it.record_type     = "nsf-award";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"NSF_AWARDS\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int nsf_run(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.nsf.gov/services/v1/awards.json?keyword=%s"
    "&printFields=id,title,agency,awardeeName,piFirstName,piLastName,"
    "fundsObligatedAmt,date,fundProgramName&rpp=25", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "nsf_awards");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  /* shape: { "response": { "award": [ ... ] } } */
  cJSON *resp = cJSON_GetObjectItem(root, "response");
  cJSON *award = resp ? cJSON_GetObjectItem(resp, "award") : NULL;
  int emitted = 0;
  if (cJSON_IsArray(award)) {
    cJSON *a; cJSON_ArrayForEach(a, award) emitted += nsf_emit(sink, a);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[nsf_awards] emitted %d\n", emitted);
  return 0;
}

/* ---- UKRI Gateway to Research -------------------------------------------- */
static int ukri_emit(intel_sink *sink, const cJSON *p) {
  if (!p) return 0;
  const char *title = jo_sv(p, "title");
  const char *id    = jo_sv(p, "id");
  if (!title && !id) return 0;

  const char *abstr  = jo_sv(p, "abstractText");
  const char *status = jo_sv(p, "status");
  const char *gref   = jo_sv(p, "grantReference");
  const char *start  = jo_sv(p, "start");

  char link[512] = {0};
  if (id) snprintf(link, sizeof link,
    "https://gtr.ukri.org/projects?ref=%s", gref ? gref : id);

  cJSON *data = cJSON_CreateObject();
  if (title)  cJSON_AddStringToObject(data, "title", title);
  if (gref)   cJSON_AddStringToObject(data, "grant_reference", gref);
  if (status) cJSON_AddStringToObject(data, "status", status);
  if (abstr)  cJSON_AddStringToObject(data, "abstract", abstr);
  cJSON_AddStringToObject(data, "source", "UKRI Gateway to Research");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "UKRI_GRANTS");
  cJSON_AddStringToObject(props, "source", "UKRI Gateway to Research");
  if (gref)   cJSON_AddStringToObject(props, "grant_reference", gref);
  if (status) cJSON_AddStringToObject(props, "status", status);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = gref ? gref : (id ? id : title);
  it.title           = title ? title : gref;
  it.summary         = status;
  it.body            = bj;
  it.lang            = "en";
  it.published_at    = start;
  it.link            = link[0] ? link : NULL;
  it.record_type     = "ukri-gtr-project";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"UKRI_GRANTS\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int ukri_run(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://gtr.ukri.org/gtr/api/projects?q=%s&s=25", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/vnd.rcuk.gtr.json-v7", NULL };
  char *body = jo_get(ctx, url, hdrs, "ukri_grants");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  /* shape: { "project": [ ... ] } */
  cJSON *proj = cJSON_GetObjectItem(root, "project");
  int emitted = 0;
  if (cJSON_IsArray(proj)) {
    cJSON *p; cJSON_ArrayForEach(p, proj) emitted += ukri_emit(sink, p);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[ukri_grants] emitted %d\n", emitted);
  return 0;
}

/* ---- shared dispatch ------------------------------------------------------ */
static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  if (strcmp(sid, "NIH_REPORTER") == 0) return nih_run(ctx, sink, q);
  if (strcmp(sid, "NSF_AWARDS")   == 0) return nsf_run(ctx, sink, q);
  if (strcmp(sid, "UKRI_GRANTS")  == 0) return ukri_run(ctx, sink, q);
  return 0;
}

static const source_def nih_reporter_def = {
  .id = "NIH_REPORTER", .collector = "osint",
  .name = "NIH RePORTER Grants", .name_ja = "NIH RePORTER 研究助成",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://api.reporter.nih.gov",
  .description = "US NIH RePORTER — funded biomedical research projects (keyless): title, PI, organization, award amount",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(nih_reporter_def)

static const source_def nsf_awards_def = {
  .id = "NSF_AWARDS", .collector = "osint",
  .name = "NSF Awards", .name_ja = "NSF 研究助成",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://api.nsf.gov",
  .description = "US National Science Foundation awards (keyless): title, awardee, PI, funds obligated",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(nsf_awards_def)

static const source_def ukri_grants_def = {
  .id = "UKRI_GRANTS", .collector = "osint",
  .name = "UKRI Gateway to Research", .name_ja = "UKRI 研究助成",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://gtr.ukri.org",
  .description = "UK Research & Innovation Gateway to Research (keyless): funded projects, grant reference, status",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(ukri_grants_def)
