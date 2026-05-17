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
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
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
    cJSON *cn = cJSON_GetObjectItem(it, "caseName");
    cJSON *co = cJSON_GetObjectItem(it, "court");
    cJSON *df = cJSON_GetObjectItem(it, "dateFiled");
    cJSON *dn = cJSON_GetObjectItem(it, "docketNumber");
    cJSON *ci = cJSON_GetObjectItem(it, "citation");
    cJSON *sn = cJSON_GetObjectItem(it, "snippet");
    cJSON *au = cJSON_GetObjectItem(it, "absolute_url");
    if (cn && cJSON_IsString(cn)) cJSON_AddStringToObject(ce, "case_name", cn->valuestring);
    if (co && cJSON_IsString(co)) cJSON_AddStringToObject(ce, "court", co->valuestring);
    if (df && cJSON_IsString(df)) cJSON_AddStringToObject(ce, "date_filed", df->valuestring);
    if (dn && cJSON_IsString(dn)) cJSON_AddStringToObject(ce, "docket_number", dn->valuestring);
    if (ci && cJSON_IsArray(ci) && cJSON_GetArraySize(ci) > 0) {
      cJSON *c0 = cJSON_GetArrayItem(ci, 0);
      if (cJSON_IsString(c0)) cJSON_AddStringToObject(ce, "citation", c0->valuestring);
    }
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

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", q);
  cJSON_AddNumberToObject(root, "timestamp", (double)time(NULL));

  cJSON *sources = cJSON_CreateArray();
  int total = 0, checked = 0;

  cJSON *op = cl_opinions(ctx->http, q);
  if (op) {
    cJSON_AddItemToArray(sources, op);
    checked++;
    cJSON *c = cJSON_GetObjectItem(op, "total_results");
    if (c && cJSON_IsNumber(c)) total += c->valueint;
  }
  cJSON *dk = cl_dockets(ctx->http, q);
  if (dk) {
    cJSON_AddItemToArray(sources, dk);
    checked++;
    cJSON *c = cJSON_GetObjectItem(dk, "total_results");
    if (c && cJSON_IsNumber(c)) total += c->valueint;
  }
  cJSON *rc_ = cl_recap(ctx->http, q);
  if (rc_) {
    cJSON_AddItemToArray(sources, rc_);
    checked++;
    cJSON *c = cJSON_GetObjectItem(rc_, "total_results");
    if (c && cJSON_IsNumber(c)) total += c->valueint;
  }
  cJSON_AddItemToObject(root, "sources", sources);

  cJSON *summary = cJSON_CreateObject();
  cJSON_AddNumberToObject(summary, "total_records_found", total);
  cJSON_AddNumberToObject(summary, "sources_searched", checked);

  cJSON *ctypes = cJSON_CreateObject();
  int crim = 0, civ = 0, bank = 0;
  if (dk) {
    cJSON *dl = cJSON_GetObjectItem(dk, "dockets");
    if (dl && cJSON_IsArray(dl)) {
      int n = cJSON_GetArraySize(dl);
      for (int i = 0; i < n; i++) {
        cJSON *d = cJSON_GetArrayItem(dl, i);
        cJSON *na = cJSON_GetObjectItem(d, "nature_of_suit");
        cJSON *ca = cJSON_GetObjectItem(d, "cause");
        const char *ty = case_type(na ? na->valuestring : NULL,
                                   ca ? ca->valuestring : NULL);
        if (strstr(ty, "Criminal")) crim++;
        else if (strstr(ty, "Bankruptcy")) bank++;
        else civ++;
      }
    }
  }
  if (crim > 0) cJSON_AddNumberToObject(ctypes, "criminal", crim);
  if (civ > 0) cJSON_AddNumberToObject(ctypes, "civil", civ);
  if (bank > 0) cJSON_AddNumberToObject(ctypes, "bankruptcy", bank);
  cJSON_AddItemToObject(summary, "case_types", ctypes);

  cJSON *notes = cJSON_CreateArray();
  cJSON_AddItemToArray(notes, cJSON_CreateString(
    "Data sourced from public court records via CourtListener/RECAP"));
  cJSON_AddItemToArray(notes, cJSON_CreateString(
    "Federal PACER documents may require account for full access"));
  cJSON_AddItemToArray(notes, cJSON_CreateString(
    "State court records may have different availability"));
  if (crim > 0)
    cJSON_AddItemToArray(notes, cJSON_CreateString(
      "Criminal records found - verify current status as charges may be dismissed or expunged"));
  cJSON_AddItemToObject(summary, "notes", notes);
  cJSON_AddItemToObject(root, "summary", summary);

  int success = (total > 0 || checked > 0);
  int conf = total > 0 ? 80 : (checked > 0 ? 50 : 30);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", conf);
  cJSON_AddItemToObject(env, "data", root);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "COURT_RECORDS");
  cJSON_AddStringToObject(props, "entity", q);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", conf);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300], title[320];
  snprintf(rk, sizeof rk, "court:%s", q);
  snprintf(title, sizeof title, "COURT_RECORDS — %s", q);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = total > 0 ? "court records found"
                                 : (checked > 0 ? "court sources checked"
                                                : "no court records");
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"COURT_RECORDS\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def court_records_def = {
  .id = "COURT_RECORDS", .collector = "osint",
  .name = "Court Records", .name_ja = "裁判記録",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(court_records_def)
