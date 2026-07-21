/* collectors/osint/sources/trademark_search.c
 * OSINT service — TRADEMARK_SEARCH. Canonical SERVICE = TRADEMARK_SEARCH
 * (dispatcher row {SERVICE_TRADEMARK_SEARCH, handle_trademark_search,
 * "TRADEMARK_SEARCH", true}). On-demand (interval 0); ctx->entity = a
 * brand/mark. No key. PATENT_SEARCH is a separate handler/source-id
 * (handle_patent_search) and is NOT emitted here.
 *
 * PER-RECORD EMIT: probes WIPO Global Brand Database (POST
 * branddb.wipo.int/branddb/api/search) and EUIPO eSearch+ (GET
 * euipo.europa.eu/eSearchCLW/api/basic-search), and emits ONE intel_item per
 * real trademark hit, keyed by office + mark id. Each row's body carries the
 * real fetched fields (mark/holder/status/applicant/application_number). APIs
 * 403/unavailable or no hits → emits nothing (honest empty — no USPTO
 * manual-search block, no Nice-classification reference block). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
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

/* Emit one intel row for a single trademark hit. Returns 1 if emitted.
 * id_field/holder_field names differ per office. */
static int emit_mark(intel_sink *sink, const char *office, const cJSON *d,
                     const char *mark_field, const char *id_field,
                     const char *holder_field) {
  if (!d) return 0;
  const cJSON *nm = cJSON_GetObjectItem(d, mark_field);
  const cJSON *id = id_field ? cJSON_GetObjectItem(d, id_field) : NULL;
  const cJSON *ho = holder_field ? cJSON_GetObjectItem(d, holder_field) : NULL;
  const cJSON *st = cJSON_GetObjectItem(d, "status");

  const char *name = (nm && cJSON_IsString(nm)) ? nm->valuestring : NULL;
  const char *mid  = (id && cJSON_IsString(id)) ? id->valuestring : NULL;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "office", office);
  if (name) cJSON_AddStringToObject(data, "mark", name);
  if (mid)  cJSON_AddStringToObject(data, "id", mid);
  if (ho && cJSON_IsString(ho)) cJSON_AddStringToObject(data, "holder", ho->valuestring);
  if (st && cJSON_IsString(st)) cJSON_AddStringToObject(data, "status", st->valuestring);
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "TRADEMARK_SEARCH");
  cJSON_AddStringToObject(props, "office", office);
  if (st && cJSON_IsString(st)) cJSON_AddStringToObject(props, "status", st->valuestring);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[400];
  snprintf(rk, sizeof rk, "mark:%s:%s", office,
           mid ? mid : (name ? name : "?"));
  char title[400];
  snprintf(title, sizeof title, "%s — %s", office, name ? name : "trademark");

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = name ? name : "trademark";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"TRADEMARK_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* WIPO Global Brand Database — POST JSON. Returns number of marks emitted. */
static int search_wipo(http_client *http, const char *mark, intel_sink *sink) {
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
  if (!json) return 0;

  int emitted = 0;
  const cJSON *docs = cJSON_GetObjectItem(json, "docs");
  if (docs && cJSON_IsArray(docs)) {
    int n = cJSON_GetArraySize(docs);
    for (int i = 0; i < n; i++)
      emitted += emit_mark(sink, "WIPO", cJSON_GetArrayItem(docs, i),
                           "brandName", "applicationNumber", "holderName");
  }
  cJSON_Delete(json);
  return emitted;
}

/* EUIPO eSearch+ — GET. Returns number of marks emitted. */
static int search_euipo(http_client *http, const char *mark, intel_sink *sink) {
  char enc[512], url[1100];
  uri_encode(mark, enc, sizeof enc);
  snprintf(url, sizeof url,
    "https://euipo.europa.eu/eSearchCLW/api/basic-search?text=%s&page=0&size=20", enc);

  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  long status = hr.status;
  cJSON *json = (hc == 0 && status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  if (!json) return 0;

  int emitted = 0;
  const cJSON *content = cJSON_GetObjectItem(json, "content");
  if (content && cJSON_IsArray(content)) {
    int n = cJSON_GetArraySize(content);
    for (int i = 0; i < n; i++)
      emitted += emit_mark(sink, "EUIPO", cJSON_GetArrayItem(content, i),
                           "markName", "applicationNumber", "applicantName");
  }
  cJSON_Delete(json);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *mark = ctx->entity;
  if (!mark || !*mark) return -1;

  int emitted = 0;
  emitted += search_wipo(ctx->http, mark, sink);
  emitted += search_euipo(ctx->http, mark, sink);
  (void)emitted;
  return 0;   /* honest empty is not an error */
}

static const source_def trademark_search_def = {
  .id = "TRADEMARK_SEARCH", .collector = "osint",
  .name = "Trademark Search", .name_ja = "商標検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "internal://osint/trademark-search",
  .description = "Search trademark registries (WIPO, EUIPO) for a mark or owner.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(trademark_search_def)
