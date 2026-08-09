/* collectors/sources/cyi_bgp_prefix_search.c
 * OSINT service — BGP_PREFIX_SEARCH. IP pivot: the covering prefix actually in
 * the global routing table for an address, its origins, and the less/more
 * specific relations around it. Separate host and dataset from the stat.ripe.net
 * endpoints the fleet already uses.
 * Endpoint: https://rest.bgp-api.net/api/v1/prefix/<prefix>/search    (keyless)
 * parse_notes: "Accepts a bare IP or a CIDR; type field tells you whether the
 * answer was an exact or longest match. result.relations enumerates less/more
 * specifics — that is where a more-specific hijack shows up." The entity is
 * passed through as given (bare IP or CIDR) and both are carried.
 * Emits ONE row: query type, resolved prefix, meta[] sourceType + originASNs,
 * and the relation counts/entries.
 * No coordinates -> has_geo 0 (R2).
 * Licence: RIPE NCC public API, keyless.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* dotted quad, optionally /len; or an IPv6-looking literal, optionally /len */
static int looks_like_ip_or_cidr(const char *s) {
  if (!s || !*s) return 0;
  size_t n = strlen(s);
  if (n < 3 || n > 49) return 0;
  if (strchr(s, ':')) {                       /* IPv6 */
    for (const char *q = s; *q; q++)
      if (!(isxdigit((unsigned char)*q) || *q == ':' || *q == '/')) return 0;
    return 1;
  }
  int parts = 0;
  const char *p = s;
  while (*p && *p != '/') {
    int v = 0, d = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; d++; if (v > 255) return 0; }
    if (!d || d > 3) return 0;
    parts++;
    if (*p == '.') { p++; if (!*p) return 0; }
    else if (*p && *p != '/') return 0;
  }
  if (parts != 4) return 0;
  if (*p == '/') {
    p++;
    int len = 0, d = 0;
    while (*p >= '0' && *p <= '9') { len = len * 10 + (*p - '0'); p++; d++; }
    if (!d || *p || len > 32) return 0;
  }
  return 1;
}

static char *http_get(const source_ctx *ctx, const char *url, long *status) {
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  *status = hr.status;
  if (rc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  char *b = hr.body; hr.body = NULL; http_response_free(&hr);
  return b;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_ip_or_cidr(q)) return 0;         /* wrong shape -> no-op */

  /* The prefix sits in a PATH segment, so the '/' of a CIDR must stay literal
   * (%2F is normalised away or rejected). looks_like_ip_or_cidr() has already
   * restricted the entity to [0-9a-f.:/], so this is safe to inject as-is. */
  char url[320];
  snprintf(url, sizeof url,
           "https://rest.bgp-api.net/api/v1/prefix/%s/search", q);

  long status = 0;
  char *body = http_get(ctx, url, &status);
  if (!body) {
    fprintf(stderr, "[BGP_PREFIX_SEARCH] http status=%ld\n", status);
    if (status >= 400 && status < 500) return 0;   /* not routed / bad query */
    return -1;
  }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[BGP_PREFIX_SEARCH] unparseable body\n"); return -1; }

  const char *mtype = jo_sv(root, "type");
  const char *asked = jo_sv(root, "prefix");
  const cJSON *result = cJSON_GetObjectItem(root, "result");
  const char *prefix = cJSON_IsObject(result) ? jo_sv(result, "prefix") : NULL;
  if (!prefix && !asked) {                        /* nothing resolved */
    cJSON_Delete(root);
    fprintf(stderr, "[BGP_PREFIX_SEARCH] no covering prefix for %s\n", q);
    return 0;
  }

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "BGP_PREFIX_SEARCH");
  cJSON_AddStringToObject(p, "query", q);
  if (mtype)  cJSON_AddStringToObject(p, "match_type", mtype);
  if (asked)  cJSON_AddStringToObject(p, "queried_prefix", asked);
  if (prefix) cJSON_AddStringToObject(p, "prefix", prefix);

  /* meta[]: sourceType + originASNs, verbatim from the reply */
  cJSON *origins = cJSON_CreateArray();
  cJSON *sources = cJSON_CreateArray();
  if (cJSON_IsObject(result)) {
    const cJSON *m;
    cJSON_ArrayForEach(m, cJSON_GetObjectItem(result, "meta")) {
      const char *st = jo_sv(m, "sourceType");
      if (st) cJSON_AddItemToArray(sources, cJSON_CreateString(st));
      const cJSON *oa;
      cJSON_ArrayForEach(oa, cJSON_GetObjectItem(m, "originASNs")) {
        if (cJSON_IsString(oa) && oa->valuestring)
          cJSON_AddItemToArray(origins, cJSON_CreateString(oa->valuestring));
        else if (cJSON_IsNumber(oa)) {
          char b[24]; snprintf(b, sizeof b, "%.0f", oa->valuedouble);
          cJSON_AddItemToArray(origins, cJSON_CreateString(b));
        }
      }
    }
    const cJSON *rel = cJSON_GetObjectItem(result, "relations");
    if (cJSON_IsArray(rel))
      cJSON_AddNumberToObject(p, "relation_count", cJSON_GetArraySize(rel));
  }
  if (cJSON_GetArraySize(origins) > 0) cJSON_AddItemToObject(p, "origin_asns", origins);
  else cJSON_Delete(origins);
  if (cJSON_GetArraySize(sources) > 0) cJSON_AddItemToObject(p, "source_types", sources);
  else cJSON_Delete(sources);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char title[160];
  snprintf(title, sizeof title, "%s covers %s", prefix ? prefix : asked, q);
  char summary[192];
  snprintf(summary, sizeof summary, "RIPE BGP API%s%s", mtype ? " · " : "",
           mtype ? mtype : "");

  intel_item it = {0};
  it.remote_key      = prefix ? prefix : q;
  it.title           = title;
  it.summary         = summary;
  it.link            = url;
  it.lang            = "en";
  it.record_type     = "bgp-prefix";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"cyber\",\"bgp\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(root);
  fprintf(stderr, "[BGP_PREFIX_SEARCH] emitted 1 (%s)\n", q);
  return 0;
}

static const source_def cyi_bgp_prefix_search_def = {
  .id = "BGP_PREFIX_SEARCH", .collector = "osint",
  .name = "RIPE BGP API — longest-match prefix search",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://rest.bgp-api.net/api/v1/prefix/",
  .description = "The covering prefix actually in the global table for an address, its origins, and the less/more-specific relations where a hijack shows up.",
  .license = "RIPE NCC public API, keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cyi_bgp_prefix_search_def)
