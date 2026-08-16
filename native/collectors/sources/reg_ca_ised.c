/* collectors/sources/reg_ca_ised.c
 * OSINT service — CA_CORPORATIONS. On-demand entity pivot against Canada's
 * federal corporations open registry (ised-isde.canada.ca / Corporations Canada
 * Beta Business Registry search). Keyless and LIVE.
 *
 * For any entity we query the public search API:
 *   GET https://ised-isde.canada.ca/cbr/srch/api/v1/search
 *       ?fq=keyword:{<entity>}&lang=en&queryaction=fieldquery
 *       &sortfield=score&sortorder=desc
 * and emit one intel_item per matched corporation: name, corporation number,
 * status, jurisdiction.
 *
 * Only real, parsed API fields are emitted; fetch failure / no matches →
 * honest empty (return 0). */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* Pull a field that may live under one of several likely keys (the Solr-style
 * response uses different names across facets). Returns first non-empty. */
static const char *ca_first(const cJSON *o, const char *const *keys) {
  for (const char *const *k = keys; *k; k++) {
    const char *v = jo_sv(o, *k);
    if (v) return v;
  }
  return NULL;
}

/* one corporation → one intel_item. Returns 1 if emitted. */
static int emit_corp(intel_sink *sink, const cJSON *c) {
  if (!c) return 0;

  static const char *name_keys[] = {
    "corporationName", "corporation_name", "name", "legalName", "title", NULL };
  static const char *num_keys[] = {
    "corporationNumber", "corporation_number", "corpNumber", "businessNumber",
    "id", NULL };
  static const char *status_keys[] = {
    "status", "statusCode", "corporationStatus", "state", NULL };
  static const char *juris_keys[] = {
    "jurisdiction", "governingJurisdiction", "province", "region", NULL };

  const char *name   = ca_first(c, name_keys);
  const char *number = ca_first(c, num_keys);
  const char *status = ca_first(c, status_keys);
  const char *juris  = ca_first(c, juris_keys);

  if (!name && !number) return 0;

  cJSON *data = cJSON_CreateObject();
  if (name)   cJSON_AddStringToObject(data, "name", name);
  if (number) cJSON_AddStringToObject(data, "corporation_number", number);
  if (status) cJSON_AddStringToObject(data, "status", status);
  if (juris)  cJSON_AddStringToObject(data, "jurisdiction", juris);
  cJSON_AddStringToObject(data, "source", "Corporations Canada");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "CA_CORPORATIONS");
  cJSON_AddStringToObject(props, "source", "Corporations Canada");
  if (number) cJSON_AddStringToObject(props, "corporation_number", number);
  if (status) cJSON_AddStringToObject(props, "status", status);
  if (juris)  cJSON_AddStringToObject(props, "jurisdiction", juris);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = number ? number : name;
  it.title           = name ? name : number;
  it.summary         = status;
  it.body            = bj;
  it.lang            = "en";
  it.record_type     = "ca-corporation";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"CA_CORPORATIONS\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Walk the response looking for the docs array. The ISED search API returns a
 * Solr-style envelope; the result list is usually under docs / results /
 * response.docs. We tolerate the common shapes. */
static int emit_docs(intel_sink *sink, cJSON *root) {
  if (!root) return 0;
  cJSON *docs = cJSON_GetObjectItem(root, "docs");
  if (!cJSON_IsArray(docs)) docs = cJSON_GetObjectItem(root, "results");
  if (!cJSON_IsArray(docs)) {
    cJSON *resp = cJSON_GetObjectItem(root, "response");
    if (resp) docs = cJSON_GetObjectItem(resp, "docs");
  }
  int emitted = 0;
  if (cJSON_IsArray(docs)) {
    cJSON *d;
    cJSON_ArrayForEach(d, docs) emitted += emit_corp(sink, d);
  }
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  char *enc = jo_urlencode(q);
  if (!enc) return 0;

  char url[1024];
  snprintf(url, sizeof url,
    "https://ised-isde.canada.ca/cbr/srch/api/v1/search"
    "?fq=keyword:%%7B%s%%7D&lang=en&queryaction=fieldquery"
    "&sortfield=score&sortorder=desc", enc);
  free(enc);

  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "ca_corporations");
  if (!body) return 0;

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  int emitted = emit_docs(sink, root);
  cJSON_Delete(root);
  fprintf(stderr, "[ca_corporations] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def ca_corporations_def = {
  .id = "CA_CORPORATIONS", .collector = "osint",
  .name = "Corporations Canada Search", .name_ja = "カナダ法人登記検索",
  .update_interval_sec = 0, .run = run,
  .category = "government", .type = "api",
  .url = "https://ised-isde.canada.ca/cbr/srch/api/v1/search",
  .description = "Corporations Canada federal registry (keyless): corporation name, number, status, jurisdiction",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(ca_corporations_def)
