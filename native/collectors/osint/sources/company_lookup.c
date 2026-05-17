/* collectors/osint/sources/company_lookup.c
 * OSINT service — faithful port of OSINTsaas osint_tools/company_lookup.c
 * (handle_company_lookup, the comprehensive registry handler). Canonical
 * SERVICE id in osint_dispatcher.c: {SERVICE_COMPANY_LOOKUP,
 * handle_company_lookup, "COMPANY_LOOKUP", true} (alias OPENCORPORATES_SEARCH
 * shares the SAME handler; primary registered = COMPANY_LOOKUP). LEI_SEARCH /
 * VAT_VALIDATOR are bound to different handlers → not this file.
 * Entity = company name. No API key (OpenCorporates v0.4 public + JP/India
 * registry lookup-URL stubs).
 *
 * Upstream wraps results in result_builder (query=<name>,
 * query_type="company"): OpenCorporates as a found/HIGH item, then
 * search_zauba_corp / search_mca_india / search_touki_japan /
 * search_houjin_bangou / search_edinet_japan / search_nikkei_company /
 * search_baseconnect_japan as found/MEDIUM lookup-URL stub items (added via
 * add_company_source_item). Its finalized JSON is faithfully rebuilt:
 *   {query,query_type:"company",results:[{source,found,confidence,
 *    detection_method:"direct",url?,data,llama_analysis:null}…],
 *    summary:{total_sources,found_count,detection_breakdown:{direct,llama:0}},
 *    _meta:{execution_time_ms,llama_fallback_used:false,timestamp}}
 * success=true, confidence 85. Single-entity pivot form of upstream's
 * entity loop. Emits one osint_service_result row (body =
 * {success,confidence,data}), like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* url_encode-equivalent (upstream JP/India stubs use url_encode). */
static void uri_encode(const char *in, char *out, size_t cap) {
  static const char *keep = "-_.!~*'()";
  size_t w = 0;
  for (const unsigned char *p = (const unsigned char *)in; *p && w + 4 < cap; p++) {
    unsigned char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || strchr(keep, c)) out[w++] = (char)c;
    else { snprintf(out + w, cap - w, "%%%02X", c); w += 3; }
  }
  out[w] = 0;
}

/* result_builder_add_item shape. data adopted into result_obj.data. */
static void add_item(cJSON *results, const char *src, int found, int conf,
                     const char *url, cJSON *data /*owned, may be NULL*/) {
  cJSON *o = cJSON_CreateObject();
  cJSON_AddStringToObject(o, "source", src ? src : "unknown");
  cJSON_AddBoolToObject(o, "found", found);
  cJSON_AddNumberToObject(o, "confidence", conf);
  cJSON_AddStringToObject(o, "detection_method", "direct");
  if (url) cJSON_AddStringToObject(o, "url", url);
  if (data) cJSON_AddItemToObject(o, "data", data);
  else cJSON_AddNullToObject(o, "data");
  cJSON_AddNullToObject(o, "llama_analysis");
  cJSON_AddItemToArray(results, o);
}

/* OpenCorporates company search → upstream COMPANY_LOOKUP_OPENCORPORATES
 * `data` object {query,source,companies:[…≤20],total_results}, or NULL. */
static cJSON *search_opencorporates(http_client *h, const char *name) {
  char enc[512]; int j = 0;
  for (int i = 0; name[i] && j < 511; i++)
    enc[j++] = name[i] == ' ' ? '+' : name[i];
  enc[j] = 0;
  char url[1024];
  snprintf(url, sizeof url,
    "https://api.opencorporates.com/v0.4/companies/search?q=%s&per_page=10",
    enc);
  http_response hr = {0};
  int hc = http_request(h, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  cJSON *root = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!root) return NULL;
  cJSON *ro = cJSON_GetObjectItem(root, "results");
  cJSON *comps = ro ? cJSON_GetObjectItem(ro, "companies") : NULL;
  if (!comps || !cJSON_IsArray(comps)) { cJSON_Delete(root); return NULL; }
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "query", name);
  cJSON_AddStringToObject(d, "source", "OpenCorporates");
  cJSON *arr = cJSON_CreateArray();
  int n = cJSON_GetArraySize(comps);
  for (int i = 0; i < n && i < 20; i++) {
    cJSON *cw = cJSON_GetArrayItem(comps, i);
    cJSON *c = cJSON_GetObjectItem(cw, "company");
    if (!c) continue;
    cJSON *e = cJSON_CreateObject();
    cJSON *nm = cJSON_GetObjectItem(c, "name");
    cJSON *ju = cJSON_GetObjectItem(c, "jurisdiction_code");
    cJSON *cn = cJSON_GetObjectItem(c, "company_number");
    cJSON *st = cJSON_GetObjectItem(c, "current_status");
    cJSON *ct = cJSON_GetObjectItem(c, "company_type");
    cJSON *id = cJSON_GetObjectItem(c, "incorporation_date");
    cJSON *ad = cJSON_GetObjectItem(c, "registered_address_in_full");
    if (nm && cJSON_IsString(nm)) cJSON_AddStringToObject(e, "name", nm->valuestring);
    if (ju && cJSON_IsString(ju)) cJSON_AddStringToObject(e, "jurisdiction", ju->valuestring);
    if (cn && cJSON_IsString(cn)) cJSON_AddStringToObject(e, "company_number", cn->valuestring);
    if (st && cJSON_IsString(st)) cJSON_AddStringToObject(e, "status", st->valuestring);
    if (ct && cJSON_IsString(ct)) cJSON_AddStringToObject(e, "type", ct->valuestring);
    if (id && cJSON_IsString(id)) cJSON_AddStringToObject(e, "incorporated", id->valuestring);
    if (ad && cJSON_IsString(ad)) cJSON_AddStringToObject(e, "address", ad->valuestring);
    cJSON_AddItemToArray(arr, e);
  }
  cJSON_AddItemToObject(d, "companies", arr);
  cJSON_AddNumberToObject(d, "total_results", n);
  cJSON_Delete(root);
  return d;
}

/* JP/India registry stub objects (verbatim field-for-field). */
static cJSON *stub_zauba(const char *q) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "ZaubaCorp");
  char enc[512]; uri_encode(q, enc, sizeof enc);
  char url[600];
  snprintf(url, sizeof url, "https://www.zaubacorp.com/company-list/%s", enc);
  cJSON_AddStringToObject(r, "lookup_url", url);
  cJSON_AddStringToObject(r, "service_type", "Company/Director Search");
  cJSON_AddStringToObject(r, "country", "India");
  cJSON_AddStringToObject(r, "coverage",
    "Company directors, shareholders, company registration details");
  return r;
}
static cJSON *stub_mca(const char *q) {
  (void)q;
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "Ministry of Corporate Affairs India");
  cJSON_AddStringToObject(r, "lookup_url", "https://www.mca.gov.in/");
  cJSON_AddStringToObject(r, "mcain_url",
    "https://www.mca.gov.in/mcafoportal/companyLLPMasterData.do");
  cJSON_AddStringToObject(r, "service_type", "Company Registry");
  cJSON_AddStringToObject(r, "country", "India");
  cJSON_AddStringToObject(r, "coverage",
    "Company registration, LLP data, director identification");
  cJSON_AddStringToObject(r, "note", "Official government company registry");
  return r;
}
static cJSON *stub_touki(const char *q) {
  (void)q;
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "Touki Japan Business Registry");
  cJSON_AddStringToObject(r, "lookup_url", "https://www1.touki.or.jp/gateway.html");
  cJSON_AddStringToObject(r, "service_type", "Business Registry");
  cJSON_AddStringToObject(r, "country", "Japan");
  cJSON_AddStringToObject(r, "note",
    "Japanese language - company/property registration");
  return r;
}
static cJSON *stub_houjin(const char *q) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "Corporate Number Search (\346\263\225\344\272\272\347\225\252\345\217\267)");
  char enc[512]; uri_encode(q, enc, sizeof enc);
  char url[600];
  snprintf(url, sizeof url,
    "https://www.houjin-bangou.nta.go.jp/kensaku/index.html?name=%s", enc);
  cJSON_AddStringToObject(r, "lookup_url", url);
  cJSON_AddStringToObject(r, "service_type", "Corporate Number Registry");
  cJSON_AddStringToObject(r, "country", "Japan");
  cJSON_AddStringToObject(r, "coverage",
    "Corporate number, company name, address, registration date");
  cJSON_AddStringToObject(r, "note",
    "Official National Tax Agency corporate registry");
  return r;
}
static cJSON *stub_edinet(const char *q) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "EDINET Japan");
  char enc[512]; uri_encode(q, enc, sizeof enc);
  char url[600];
  snprintf(url, sizeof url,
    "https://disclosure2.edinet-fsa.go.jp/WEEK0010.aspx?name=%s", enc);
  cJSON_AddStringToObject(r, "lookup_url", url);
  cJSON_AddStringToObject(r, "service_type", "Financial Disclosure System");
  cJSON_AddStringToObject(r, "country", "Japan");
  cJSON_AddStringToObject(r, "coverage",
    "Annual reports, securities filings, financial statements");
  cJSON_AddStringToObject(r, "note",
    "Electronic Disclosure for Investors NETwork - FSA Japan");
  return r;
}
static cJSON *stub_nikkei(const char *q) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "NIKKEI Company Search");
  char enc[512]; uri_encode(q, enc, sizeof enc);
  char url[600];
  snprintf(url, sizeof url, "https://www.nikkei.com/nkd/company/?wd=%s", enc);
  cJSON_AddStringToObject(r, "lookup_url", url);
  cJSON_AddStringToObject(r, "service_type", "Financial/Company Database");
  cJSON_AddStringToObject(r, "country", "Japan");
  cJSON_AddStringToObject(r, "coverage",
    "Listed companies, stock info, financial data");
  return r;
}
static cJSON *stub_baseconnect(const char *q) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "Baseconnect");
  char enc[512]; uri_encode(q, enc, sizeof enc);
  char url[600];
  snprintf(url, sizeof url, "https://baseconnect.in/companies?q=%s", enc);
  cJSON_AddStringToObject(r, "lookup_url", url);
  cJSON_AddStringToObject(r, "service_type", "Company Database");
  cJSON_AddStringToObject(r, "country", "Japan");
  cJSON_AddStringToObject(r, "coverage",
    "Company profiles, employees, industry info");
  return r;
}

/* add_company_source_item: source name + lookup_url → MEDIUM found item. */
static void add_source_stub(cJSON *results, cJSON *sd /*owned*/) {
  cJSON *sn = cJSON_GetObjectItem(sd, "source");
  cJSON *lu = cJSON_GetObjectItem(sd, "lookup_url");
  const char *nm = (sn && cJSON_IsString(sn)) ? sn->valuestring : "Unknown";
  add_item(results, nm, 1, 70,
           (lu && cJSON_IsString(lu)) ? lu->valuestring : NULL, sd);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  time_t start = time(NULL);

  cJSON *results = cJSON_CreateArray();
  int total = 0, found = 0;

  cJSON *oc = search_opencorporates(ctx->http, q);
  if (oc) { add_item(results, "OpenCorporates", 1, 90,
                     "https://opencorporates.com/", oc);
            total++; found++; }

  cJSON *stubs[7];
  stubs[0] = stub_zauba(q);
  stubs[1] = stub_mca(q);
  stubs[2] = stub_touki(q);
  stubs[3] = stub_houjin(q);
  stubs[4] = stub_edinet(q);
  stubs[5] = stub_nikkei(q);
  stubs[6] = stub_baseconnect(q);
  for (int i = 0; i < 7; i++) { add_source_stub(results, stubs[i]); total++; found++; }

  cJSON *rb = cJSON_CreateObject();
  cJSON_AddStringToObject(rb, "query", q);
  cJSON_AddStringToObject(rb, "query_type", "company");
  cJSON_AddItemToObject(rb, "results", results);
  cJSON *summ = cJSON_CreateObject();
  cJSON_AddNumberToObject(summ, "total_sources", total);
  cJSON_AddNumberToObject(summ, "found_count", found);
  cJSON *bd = cJSON_CreateObject();
  cJSON_AddNumberToObject(bd, "direct", total);
  cJSON_AddNumberToObject(bd, "llama", 0);
  cJSON_AddItemToObject(summ, "detection_breakdown", bd);
  cJSON_AddItemToObject(rb, "summary", summ);
  cJSON *meta = cJSON_CreateObject();
  time_t end = time(NULL);
  cJSON_AddNumberToObject(meta, "execution_time_ms",
                          (int)((end - start) * 1000));
  cJSON_AddBoolToObject(meta, "llama_fallback_used", 0);
  cJSON_AddNumberToObject(meta, "timestamp", (double)end);
  cJSON_AddItemToObject(rb, "_meta", meta);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", 85);
  cJSON_AddItemToObject(env, "data", rb);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "COMPANY_LOOKUP");
  cJSON_AddStringToObject(props, "entity", q);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 85);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300], title[320];
  snprintf(rk, sizeof rk, "company:%s", q);
  snprintf(title, sizeof title, "COMPANY_LOOKUP — %s", q);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "company registries queried";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"COMPANY_LOOKUP\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def company_lookup_def = {
  .id = "COMPANY_LOOKUP", .collector = "osint",
  .name = "Company Lookup", .name_ja = "企業検索",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(company_lookup_def)
