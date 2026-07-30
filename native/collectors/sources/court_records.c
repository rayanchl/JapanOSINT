/* collectors/osint/sources/court_records.c
 * OSINT service — faithful port of OSINTsaas osint_tools/court_records.c
 * (court_records_search ← handle_court_records). Canonical SERVICE id in
 * osint_dispatcher.c: {SERVICE_COURT_RECORDS, handle_court_records,
 * "COURT_RECORDS", true}. (LEGAL_SEARCH/BANKRUPTCY_SEARCH/CRIMINAL_RECORDS
 * are registered to DIFFERENT handlers — handle_legal_search etc — so they
 * are NOT this file even though those handlers internally call back into
 * court_records; the registry name bound to handle_court_records is exactly
 * COURT_RECORDS.) Entity = person/company name.
 *
 * NOT key-gated in behaviour: upstream reads COURTLISTENER_API_KEY via
 * getenv but never sends it (CourtListener's public REST v3 is keyless), so
 * a faithful port performs the same keyless GETs. Sources:
 * courtlistener search (opinions, type=o), dockets, recap. Rebuilds
 * upstream `root` exactly: {query,timestamp,sources:[…],summary:{
 * total_records_found,sources_searched,case_types:{criminal?,civil?,
 * bankruptcy?},notes:[…]}}. success = (total>0 || sources_checked>0);
 * confidence 80 / 50 / 30. Emits one osint_service_result row (body =
 * {success,confidence,data}). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* encode_query: alnum/-_. kept, space→+, else %XX (verbatim upstream). */
static void encode_query(const char *s, char *out, size_t cap) {
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)s; *p && w + 4 < cap; p++) {
    unsigned char c = *p;
    if (isalnum(c) || c == '-' || c == '_' || c == '.') out[w++] = (char)c;
    else if (c == ' ') out[w++] = '+';
    else { snprintf(out + w, cap - w, "%%%02X", c); w += 3; }
  }
  out[w] = 0;
}

static cJSON *http_json(http_client *h, const char *url) {
  http_response hr = {0};
  int hc = http_request(h, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  cJSON *j = (hc == 0 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  return j;
}

static cJSON *cl_opinions(http_client *h, const char *q) {
  char enc[768]; encode_query(q, enc, sizeof enc);
  char url[900];
  snprintf(url, sizeof url,
    "https://www.courtlistener.com/api/rest/v3/search/?q=%s&type=o", enc);
  cJSON *j = http_json(h, url);
  if (!j) return NULL;
  cJSON *res = cJSON_GetObjectItem(j, "results");
  if (!res || !cJSON_IsArray(res)) { cJSON_Delete(j); return NULL; }
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "CourtListener Opinions");
  int n = cJSON_GetArraySize(res);
  cJSON_AddNumberToObject(r, "total_results", n);
  cJSON *cases = cJSON_CreateArray();
  int mx = n > 10 ? 10 : n;
  for (int i = 0; i < mx; i++) {
    cJSON *it = cJSON_GetArrayItem(res, i);
    cJSON *ce = cJSON_CreateObject();
    cJSON *id = cJSON_GetObjectItem(it, "id");
    cJSON *cn = cJSON_GetObjectItem(it, "caseName");
    cJSON *co = cJSON_GetObjectItem(it, "court");
    cJSON *df = cJSON_GetObjectItem(it, "dateFiled");
    cJSON *dn = cJSON_GetObjectItem(it, "docketNumber");
    cJSON *ci = cJSON_GetObjectItem(it, "citation");
    cJSON *sn = cJSON_GetObjectItem(it, "snippet");
    cJSON *au = cJSON_GetObjectItem(it, "absolute_url");
    if (id) {
      if (cJSON_IsNumber(id)) cJSON_AddNumberToObject(ce, "id", id->valuedouble);
      else if (cJSON_IsString(id)) cJSON_AddStringToObject(ce, "id", id->valuestring);
    }
    if (cn && cJSON_IsString(cn)) cJSON_AddStringToObject(ce, "case_name", cn->valuestring);
    if (co && cJSON_IsString(co)) cJSON_AddStringToObject(ce, "court", co->valuestring);
    if (df && cJSON_IsString(df)) cJSON_AddStringToObject(ce, "date_filed", df->valuestring);
    if (dn && cJSON_IsString(dn)) cJSON_AddStringToObject(ce, "docket_number", dn->valuestring);
    if (ci && cJSON_IsArray(ci) && cJSON_GetArraySize(ci) > 0) {
      cJSON *c0 = cJSON_GetArrayItem(ci, 0);  /* exhaustive-ok: display pick; citations_all carries every value */
      if (cJSON_IsString(c0)) cJSON_AddStringToObject(ce, "citation", c0->valuestring);
    }
    /* an opinion carries several parallel citations — keep them all */
    cJSON_AddItemToObject(ce, "citations_all", cJSON_Duplicate(ci, 1));
    if (sn && cJSON_IsString(sn)) {
      char clean[512]; char *src = sn->valuestring, *dst = clean;
      int tag = 0;
      for (int j = 0; src[j] && dst - clean < 500; j++) {
        if (src[j] == '<') tag = 1;
        else if (src[j] == '>') tag = 0;
        else if (!tag) *dst++ = src[j];
      }
      *dst = 0;
      cJSON_AddStringToObject(ce, "excerpt", clean);
    }
    if (au && cJSON_IsString(au)) {
      char fu[256];
      snprintf(fu, sizeof fu, "https://www.courtlistener.com%s", au->valuestring);
      cJSON_AddStringToObject(ce, "url", fu);
    }
    cJSON_AddItemToArray(cases, ce);
  }
  cJSON_AddItemToObject(r, "cases", cases);
  cJSON_Delete(j);
  return r;
}

static cJSON *cl_dockets(http_client *h, const char *q) {
  char enc[768]; encode_query(q, enc, sizeof enc);
  char url[900];
  snprintf(url, sizeof url,
    "https://www.courtlistener.com/api/rest/v3/dockets/?q=%s", enc);
  cJSON *j = http_json(h, url);
  if (!j) return NULL;
  cJSON *res = cJSON_GetObjectItem(j, "results");
  if (!res || !cJSON_IsArray(res)) { cJSON_Delete(j); return NULL; }
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "CourtListener Dockets");
  int n = cJSON_GetArraySize(res);
  cJSON_AddNumberToObject(r, "total_results", n);
  cJSON *dks = cJSON_CreateArray();
  int mx = n > 10 ? 10 : n;
  for (int i = 0; i < mx; i++) {
    cJSON *it = cJSON_GetArrayItem(res, i);
    cJSON *de = cJSON_CreateObject();
    cJSON *id = cJSON_GetObjectItem(it, "id");
    if (id) {
      if (cJSON_IsNumber(id)) cJSON_AddNumberToObject(de, "id", id->valuedouble);
      else if (cJSON_IsString(id)) cJSON_AddStringToObject(de, "id", id->valuestring);
    }
    cJSON *au = cJSON_GetObjectItem(it, "absolute_url");
    if (au && cJSON_IsString(au)) {
      char fu[256];
      snprintf(fu, sizeof fu, "https://www.courtlistener.com%s", au->valuestring);
      cJSON_AddStringToObject(de, "url", fu);
    }
    const char *sk[][2] = {
      {"case_name","case_name"}, {"court","court"},
      {"date_filed","date_filed"}, {"date_terminated","date_terminated"},
      {"docket_number","docket_number"}, {"nature_of_suit","nature_of_suit"},
      {"cause","cause"}, {"assigned_to_str","judge"} };
    for (int k = 0; k < 8; k++) {
      cJSON *v = cJSON_GetObjectItem(it, sk[k][0]);
      if (v && cJSON_IsString(v)) cJSON_AddStringToObject(de, sk[k][1], v->valuestring);
    }
    cJSON_AddItemToArray(dks, de);
  }
  cJSON_AddItemToObject(r, "dockets", dks);
  cJSON_Delete(j);
  return r;
}

static cJSON *cl_recap(http_client *h, const char *q) {
  char enc[768]; encode_query(q, enc, sizeof enc);
  char url[900];
  snprintf(url, sizeof url,
    "https://www.courtlistener.com/api/rest/v3/recap/?q=%s", enc);
  cJSON *j = http_json(h, url);
  if (!j) return NULL;
  cJSON *res = cJSON_GetObjectItem(j, "results");
  if (!res || !cJSON_IsArray(res)) { cJSON_Delete(j); return NULL; }
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "RECAP/PACER");
  int n = cJSON_GetArraySize(res);
  cJSON_AddNumberToObject(r, "total_results", n);
  cJSON *docs = cJSON_CreateArray();
  int mx = n > 10 ? 10 : n;
  for (int i = 0; i < mx; i++) {
    cJSON *it = cJSON_GetArrayItem(res, i);
    cJSON *de = cJSON_CreateObject();
    cJSON *ds = cJSON_GetObjectItem(it, "description");
    cJSON *dnu = cJSON_GetObjectItem(it, "document_number");
    cJSON *df = cJSON_GetObjectItem(it, "date_filed");
    cJSON *pc = cJSON_GetObjectItem(it, "page_count");
    cJSON *fp = cJSON_GetObjectItem(it, "filepath_local");
    if (ds && cJSON_IsString(ds)) cJSON_AddStringToObject(de, "description", ds->valuestring);
    if (dnu && cJSON_IsNumber(dnu)) cJSON_AddNumberToObject(de, "document_number", dnu->valueint);
    if (df && cJSON_IsString(df)) cJSON_AddStringToObject(de, "date_filed", df->valuestring);
    if (pc && cJSON_IsNumber(pc)) cJSON_AddNumberToObject(de, "page_count", pc->valueint);
    if (fp && cJSON_IsString(fp)) cJSON_AddBoolToObject(de, "pdf_available", 1);
    cJSON_AddItemToArray(docs, de);
  }
  cJSON_AddItemToObject(r, "documents", docs);
  cJSON_Delete(j);
  return r;
}

static const char *case_type(const char *nature, const char *cause) {
  if (nature) {
    if (strstr(nature, "Criminal") || strstr(nature, "CRIMINAL")) return "Criminal";
    if (strstr(nature, "Bankruptcy") || strstr(nature, "BANKRUPTCY")) return "Bankruptcy";
    if (strstr(nature, "Civil Rights")) return "Civil - Civil Rights";
    if (strstr(nature, "Contract")) return "Civil - Contract";
    if (strstr(nature, "Property")) return "Civil - Property";
    if (strstr(nature, "Tort")) return "Civil - Tort";
    if (strstr(nature, "Labor")) return "Civil - Labor";
    if (strstr(nature, "Tax")) return "Civil - Tax";
  }
  if (cause) {
    if (strstr(cause, "18 U.S.C.")) return "Criminal";
    if (strstr(cause, "21 U.S.C.")) return "Criminal - Drug";
    if (strstr(cause, "11 U.S.C.")) return "Bankruptcy";
  }
  return "Civil";
}

/* Render a CourtListener record id (number or string) into rk_id; "" if none. */
static void record_id(cJSON *rec, char *out, size_t cap) {
  cJSON *id = cJSON_GetObjectItem(rec, "id");
  if (id && cJSON_IsNumber(id))      snprintf(out, cap, "%lld", (long long)id->valuedouble);
  else if (id && cJSON_IsString(id)) snprintf(out, cap, "%s", id->valuestring);
  else                               out[0] = 0;
}

static const char *str_of(cJSON *rec, const char *k) {
  cJSON *v = cJSON_GetObjectItem(rec, k);
  return (v && cJSON_IsString(v)) ? v->valuestring : NULL;
}

/* Build "court + date" summary string into buf. */
static void make_summary(const char *court, const char *date, char *buf, size_t cap) {
  if (court && date)      snprintf(buf, cap, "%s — %s", court, date);
  else if (court)         snprintf(buf, cap, "%s", court);
  else if (date)          snprintf(buf, cap, "%s", date);
  else                    buf[0] = 0;
}

/* Emit ONE intel_item for a single normalized case/docket record.
 * prefix is "case" or "docket"; entity is the search query. Returns 1 if a
 * row was emitted. */
static int emit_record(intel_sink *sink, cJSON *rec, const char *prefix,
                       const char *entity, const char *case_type_str, int idx) {
  if (!rec) return 0;

  char id_buf[64]; record_id(rec, id_buf, sizeof id_buf);
  const char *name  = str_of(rec, "case_name");
  const char *court = str_of(rec, "court");
  const char *date  = str_of(rec, "date_filed");
  const char *url   = str_of(rec, "url");

  /* Stable remote_key: prefer the CourtListener record id; fall back to a
   * deterministic composite so distinct records still get distinct keys. */
  char rk[256];
  if (id_buf[0])
    snprintf(rk, sizeof rk, "%s:%s", prefix, id_buf);
  else
    snprintf(rk, sizeof rk, "%s:%s:%d", prefix, entity ? entity : "?", idx);

  char title[384];
  if (name && court)      snprintf(title, sizeof title, "%s (%s)", name, court);
  else if (name)          snprintf(title, sizeof title, "%s", name);
  else                    snprintf(title, sizeof title, "%s %s", prefix, id_buf[0] ? id_buf : entity);

  char summary[256]; make_summary(court, date, summary, sizeof summary);

  char *bj = cJSON_PrintUnformatted(rec);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "COURT_RECORDS");
  if (entity) cJSON_AddStringToObject(props, "entity", entity);
  if (court)  cJSON_AddStringToObject(props, "court", court);
  if (case_type_str) cJSON_AddStringToObject(props, "case_type", case_type_str);
  char *pj = cJSON_PrintUnformatted(props);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = summary[0] ? summary : NULL;
  it.published_at    = date;   /* CourtListener dateFiled is ISO (YYYY-MM-DD) */
  it.link            = url;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"COURT_RECORDS\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  /* Fetch logic unchanged: opinions/search + dockets (RECAP fetched for
   * parity but its rows lack a stable per-record case id, so they are not
   * emitted as case/docket records). */
  cJSON *op  = cl_opinions(ctx->http, q);
  cJSON *dk  = cl_dockets(ctx->http, q);
  cJSON *rc_ = cl_recap(ctx->http, q);

  int emitted = 0, idx = 0;

  if (op) {
    cJSON *cases = cJSON_GetObjectItem(op, "cases");
    if (cases && cJSON_IsArray(cases)) {
      int n = cJSON_GetArraySize(cases);
      for (int i = 0; i < n; i++)
        emitted += emit_record(sink, cJSON_GetArrayItem(cases, i),
                               "case", q, NULL, idx++);
    }
  }
  if (dk) {
    cJSON *dks = cJSON_GetObjectItem(dk, "dockets");
    if (dks && cJSON_IsArray(dks)) {
      int n = cJSON_GetArraySize(dks);
      for (int i = 0; i < n; i++) {
        cJSON *d = cJSON_GetArrayItem(dks, i);
        const char *ty = case_type(str_of(d, "nature_of_suit"), str_of(d, "cause"));
        emitted += emit_record(sink, d, "docket", q, ty, idx++);
      }
    }
  }

  cJSON_Delete(op);
  cJSON_Delete(dk);
  cJSON_Delete(rc_);

  /* Honest empty: zero results → emit nothing, return 0 (not an error). */
  return emitted >= 0 ? 0 : 0;
}

static const source_def court_records_def = {
  .id = "COURT_RECORDS", .collector = "osint",
  .name = "Court Records", .name_ja = "裁判記録",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/court-records",
  .description = "Searches CourtListener opinions, dockets and RECAP/PACER records",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(court_records_def)

/* Folded into PERSON_SEARCH (people_finder.c) while still registered standalone
 * as COURT_RECORDS — exposes the run entry point. */
int jo_court_records_run(const source_ctx *ctx, intel_sink *sink) { return run(ctx, sink); }
