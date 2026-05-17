/* collectors/osint/sources/trademark_search.c
 * OSINT service — port of OSINTsaas osint_tools/trademark_search.c
 * (trademark_search → handle_trademark_search). Canonical SERVICE =
 * TRADEMARK_SEARCH (dispatcher row {SERVICE_TRADEMARK_SEARCH,
 * handle_trademark_search, "TRADEMARK_SEARCH", true}). On-demand (interval 0);
 * ctx->entity = a brand/mark. No key. Faithfully reproduces the three registry
 * probes: search_uspto (static guidance — USPTO has no public API),
 * search_wipo (POST branddb.wipo.int/branddb/api/search JSON {brandName,start,
 * rows}; non-200 → api_unavailable + manual URL), search_euipo (GET
 * euipo.europa.eu/eSearchCLW/api/basic-search?text=...&page=0&size=20; non-200
 * → api_unavailable), analyze_availability and the Nice-classification
 * reference block. confidence 75. PATENT_SEARCH is a separate
 * handler/source-id (handle_patent_search) and is NOT emitted here. Emits ONE
 * osint_service_result row like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

static cJSON *search_uspto(const char *mark) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "USPTO TESS");
  cJSON_AddStringToObject(r, "coverage", "United States");
  char enc[512], url[1100];
  uri_encode(mark, enc, sizeof enc);
  snprintf(url, sizeof url,
    "https://tmsearch.uspto.gov/bin/showfield?f=toc&state=4801%%3A0001.0001&p_search=searchss&p_L=100&BackReference=&p_plural=yes&p_s_PARA1=&p_taession=&p_s_PARA2=%s",
    enc);
  cJSON_AddStringToObject(r, "status", "manual_search_required");
  cJSON_AddStringToObject(r, "search_url", url);
  cJSON_AddStringToObject(r, "direct_search_url", "https://tmsearch.uspto.gov/");
  cJSON *tips = cJSON_CreateArray();
  cJSON_AddItemToArray(tips, cJSON_CreateString("Use Basic Word Mark Search for exact matches"));
  cJSON_AddItemToArray(tips, cJSON_CreateString("Use Free Form Search with wildcards (* or $) for broader results"));
  cJSON_AddItemToArray(tips, cJSON_CreateString("Check both LIVE and DEAD status marks"));
  cJSON_AddItemToArray(tips, cJSON_CreateString("Review Nice Classification codes for your industry"));
  cJSON_AddItemToObject(r, "search_tips", tips);
  return r;
}

static cJSON *search_wipo(http_client *http, const char *mark) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "WIPO Global Brand Database");
  cJSON_AddStringToObject(r, "coverage", "International (Madrid System)");

  cJSON *q = cJSON_CreateObject();
  cJSON_AddStringToObject(q, "brandName", mark);
  cJSON_AddNumberToObject(q, "start", 0);
  cJSON_AddNumberToObject(q, "rows", 20);
  char *post = cJSON_PrintUnformatted(q);
  cJSON_Delete(q);

  static const char *hdrs[] = { "Content-Type", "application/json", NULL };
  http_response hr = {0};
  int hc = http_request(http, "POST", "https://branddb.wipo.int/branddb/api/search",
                         hdrs, post, post ? strlen(post) : 0, 20000, 1, &hr);
  long status = hr.status;
  cJSON *json = (hc == 0 && status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  free(post);

  if (hc != 0 || status != 200) {
    cJSON_AddStringToObject(r, "status", "api_unavailable");
    cJSON_AddStringToObject(r, "manual_search_url", "https://www.wipo.int/branddb/");
    cJSON_AddStringToObject(r, "note", "WIPO API requires specific access. Use manual search URL.");
    if (json) cJSON_Delete(json);
    return r;
  }
  if (!json) { cJSON_AddStringToObject(r, "status", "parse_error"); return r; }

  const cJSON *total = cJSON_GetObjectItem(json, "total");
  if (total && cJSON_IsNumber(total)) {
    cJSON_AddNumberToObject(r, "total_results", total->valueint);
    if (total->valueint > 0) {
      const cJSON *docs = cJSON_GetObjectItem(json, "docs");
      if (docs && cJSON_IsArray(docs)) {
        cJSON *marks = cJSON_CreateArray();
        int n = cJSON_GetArraySize(docs);
        for (int i = 0; i < n && i < 10; i++) {
          const cJSON *d = cJSON_GetArrayItem(docs, i);
          if (!d) continue;
          cJSON *e = cJSON_CreateObject();
          const cJSON *nm = cJSON_GetObjectItem(d, "brandName");
          const cJSON *ho = cJSON_GetObjectItem(d, "holderName");
          const cJSON *st = cJSON_GetObjectItem(d, "status");
          if (nm && cJSON_IsString(nm)) cJSON_AddStringToObject(e, "mark", nm->valuestring);
          if (ho && cJSON_IsString(ho)) cJSON_AddStringToObject(e, "holder", ho->valuestring);
          if (st && cJSON_IsString(st)) cJSON_AddStringToObject(e, "status", st->valuestring);
          cJSON_AddItemToArray(marks, e);
        }
        cJSON_AddItemToObject(r, "trademarks", marks);
      }
    }
  }
  cJSON_Delete(json);
  return r;
}

static cJSON *search_euipo(http_client *http, const char *mark) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "EUIPO eSearch+");
  cJSON_AddStringToObject(r, "coverage", "European Union");
  char enc[512], url[1100];
  uri_encode(mark, enc, sizeof enc);
  snprintf(url, sizeof url,
    "https://euipo.europa.eu/eSearchCLW/api/basic-search?text=%s&page=0&size=20", enc);

  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  long status = hr.status;
  cJSON *json = (hc == 0 && status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);

  if (hc != 0 || status != 200) {
    cJSON_AddStringToObject(r, "status", "api_unavailable");
    cJSON_AddStringToObject(r, "manual_search_url", "https://euipo.europa.eu/eSearch/");
    if (json) cJSON_Delete(json);
    return r;
  }
  if (!json) { cJSON_AddStringToObject(r, "status", "parse_error"); return r; }

  const cJSON *content = cJSON_GetObjectItem(json, "content");
  if (content && cJSON_IsArray(content)) {
    int n = cJSON_GetArraySize(content);
    cJSON_AddNumberToObject(r, "results_count", n);
    if (n > 0) {
      cJSON *marks = cJSON_CreateArray();
      for (int i = 0; i < n && i < 10; i++) {
        const cJSON *it = cJSON_GetArrayItem(content, i);
        if (!it) continue;
        cJSON *e = cJSON_CreateObject();
        const cJSON *nm = cJSON_GetObjectItem(it, "markName");
        const cJSON *no = cJSON_GetObjectItem(it, "applicationNumber");
        const cJSON *st = cJSON_GetObjectItem(it, "status");
        const cJSON *ap = cJSON_GetObjectItem(it, "applicantName");
        if (nm && cJSON_IsString(nm)) cJSON_AddStringToObject(e, "mark", nm->valuestring);
        if (no && cJSON_IsString(no)) cJSON_AddStringToObject(e, "application_number", no->valuestring);
        if (st && cJSON_IsString(st)) cJSON_AddStringToObject(e, "status", st->valuestring);
        if (ap && cJSON_IsString(ap)) cJSON_AddStringToObject(e, "applicant", ap->valuestring);
        cJSON_AddItemToArray(marks, e);
      }
      cJSON_AddItemToObject(r, "trademarks", marks);
    }
  }
  cJSON_Delete(json);
  return r;
}

static cJSON *analyze_availability(const cJSON *uspto, const cJSON *wipo, const cJSON *euipo) {
  cJSON *a = cJSON_CreateObject();
  int conflicts = 0;
  cJSON *clear = cJSON_CreateArray();
  cJSON *conf = cJSON_CreateArray();
  if (uspto) {
    const cJSON *st = cJSON_GetObjectItem(uspto, "status");
    if (st && cJSON_IsString(st) && strcmp(st->valuestring, "manual_search_required") == 0) {
      cJSON *e = cJSON_CreateObject();
      cJSON_AddStringToObject(e, "region", "United States");
      cJSON_AddStringToObject(e, "status", "needs_manual_check");
      cJSON_AddItemToArray(conf, e);
    }
  }
  if (wipo) {
    const cJSON *t = cJSON_GetObjectItem(wipo, "total_results");
    if (t && cJSON_IsNumber(t)) {
      cJSON *e = cJSON_CreateObject();
      cJSON_AddStringToObject(e, "region", "International");
      if (t->valueint > 0) {
        cJSON_AddStringToObject(e, "status", "potential_conflict");
        cJSON_AddNumberToObject(e, "matches", t->valueint);
        conflicts += t->valueint;
        cJSON_AddItemToArray(conf, e);
      } else { cJSON_AddStringToObject(e, "status", "clear"); cJSON_AddItemToArray(clear, e); }
    }
  }
  if (euipo) {
    const cJSON *c = cJSON_GetObjectItem(euipo, "results_count");
    if (c && cJSON_IsNumber(c)) {
      cJSON *e = cJSON_CreateObject();
      cJSON_AddStringToObject(e, "region", "European Union");
      if (c->valueint > 0) {
        cJSON_AddStringToObject(e, "status", "potential_conflict");
        cJSON_AddNumberToObject(e, "matches", c->valueint);
        conflicts += c->valueint;
        cJSON_AddItemToArray(conf, e);
      } else { cJSON_AddStringToObject(e, "status", "clear"); cJSON_AddItemToArray(clear, e); }
    }
  }
  cJSON_AddItemToObject(a, "clear_regions", clear);
  cJSON_AddItemToObject(a, "conflict_regions", conf);
  cJSON_AddNumberToObject(a, "total_potential_conflicts", conflicts);
  const char *as = conflicts == 0 ? "LIKELY_AVAILABLE"
                 : conflicts < 3 ? "NEEDS_REVIEW" : "LIKELY_CONFLICTS";
  cJSON_AddStringToObject(a, "preliminary_assessment", as);
  cJSON_AddStringToObject(a, "disclaimer",
    "This is a preliminary search only. A professional trademark "
    "clearance search by a qualified attorney is strongly recommended "
    "before filing any trademark application.");
  return a;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *mark = ctx->entity;
  if (!mark || !*mark) return -1;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "mark", mark);

  cJSON *uspto = search_uspto(mark);
  cJSON *wipo = search_wipo(ctx->http, mark);
  cJSON *euipo = search_euipo(ctx->http, mark);

  cJSON *reg = cJSON_CreateObject();
  cJSON_AddItemToObject(reg, "uspto", cJSON_Duplicate(uspto, 1));
  cJSON_AddItemToObject(reg, "wipo", cJSON_Duplicate(wipo, 1));
  cJSON_AddItemToObject(reg, "euipo", cJSON_Duplicate(euipo, 1));
  cJSON_AddItemToObject(root, "registries", reg);

  cJSON_AddItemToObject(root, "availability_analysis",
                        analyze_availability(uspto, wipo, euipo));
  cJSON_Delete(uspto); cJSON_Delete(wipo); cJSON_Delete(euipo);

  cJSON *nice = cJSON_CreateObject();
  cJSON_AddStringToObject(nice, "system", "Nice Classification");
  cJSON_AddNumberToObject(nice, "total_classes", 45);
  cJSON_AddStringToObject(nice, "goods_classes", "1-34");
  cJSON_AddStringToObject(nice, "services_classes", "35-45");
  cJSON_AddStringToObject(nice, "reference_url", "https://www.wipo.int/classifications/nice/");
  cJSON_AddItemToObject(root, "nice_classification", nice);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", 75);
  cJSON_AddItemToObject(env, "data", root);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "TRADEMARK_SEARCH");
  cJSON_AddStringToObject(props, "entity", mark);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 75);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300]; snprintf(rk, sizeof rk, "trademark:%s", mark);
  char title[320]; snprintf(title, sizeof title, "TRADEMARK_SEARCH — %s", mark);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "trademark search";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"TRADEMARK_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def trademark_search_def = {
  .id = "TRADEMARK_SEARCH", .collector = "osint",
  .name = "Trademark Search", .name_ja = "商標検索",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(trademark_search_def)
