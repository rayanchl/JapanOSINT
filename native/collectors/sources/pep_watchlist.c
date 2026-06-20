/* collectors/osint/sources/pep_watchlist.c — PEP / sanctions-watchlist OSINT,
 * ported from OSINTsaas sanctions_check.c (handle_pep_check / handle_watchlist_check)
 * onto the JapanOSINT source ABI.
 *
 *   PEP_CHECK           — politically-exposed-person screening
 *   WATCHLIST_CHECK_NEW — sanctions / watchlist screening
 *
 * Backed by OpenSanctions (api.opensanctions.org), which requires an API key —
 * gates on OPENSANCTIONS_API_KEY and is honest-empty without it. Results are
 * the real matched entities (caption / schema / datasets / topics). */
#include "../../source.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void urlenc(const char *s, char *out, size_t cap) {
  size_t o = 0;
  for (const char *p = s; *p && o + 4 < cap; p++) {
    unsigned char c = (unsigned char)*p;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out[o++] = (char)c;
    else o += (size_t)snprintf(out + o, cap - o, "%%%02X", c);
  }
  out[o] = 0;
}

static int opensanctions(const source_ctx *ctx, intel_sink *sink, const char *entity,
                         const char *service, const char *tags, int pep_only) {
  const char *key = getenv("OPENSANCTIONS_API_KEY");
  if (!key || !*key) return 0;                  /* honest empty without key */
  char enc[512]; urlenc(entity, enc, sizeof enc);
  char url[700];
  if (pep_only)
    snprintf(url, sizeof url,
      "https://api.opensanctions.org/search/default?q=%s&topics=role.pep&limit=15", enc);
  else
    snprintf(url, sizeof url,
      "https://api.opensanctions.org/search/default?q=%s&limit=15", enc);
  char auth[160]; snprintf(auth, sizeof auth, "Authorization: ApiKey %s", key);
  const char *headers[] = { auth, NULL };

  http_response hr = {0};
  if (http_request(ctx->http, "GET", url, headers, NULL, 0, 15000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  if (!j) return 0;
  cJSON *res = cJSON_GetObjectItem(j, "results");
  int emitted = 0, n = (res && cJSON_IsArray(res)) ? cJSON_GetArraySize(res) : 0;
  for (int i = 0; i < n; i++) {
    cJSON *r = cJSON_GetArrayItem(res, i);
    cJSON *cap = cJSON_GetObjectItem(r, "caption");
    if (!cap || !cJSON_IsString(cap)) continue;
    cJSON *out = cJSON_CreateObject();
    cJSON_AddStringToObject(out, "source", "api.opensanctions.org");
    cJSON_AddStringToObject(out, "caption", cap->valuestring);
    cJSON *sch = cJSON_GetObjectItem(r, "schema");
    if (sch && cJSON_IsString(sch)) cJSON_AddStringToObject(out, "schema", sch->valuestring);
    cJSON *ds = cJSON_GetObjectItem(r, "datasets");
    if (ds) cJSON_AddItemToObject(out, "datasets", cJSON_Duplicate(ds, 1));
    cJSON *tp = cJSON_GetObjectItem(r, "topics");
    if (tp) cJSON_AddItemToObject(out, "topics", cJSON_Duplicate(tp, 1));
    char *bj = cJSON_PrintUnformatted(out);
    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", service);
    cJSON_AddStringToObject(props, "entity", entity);
    char *pj = cJSON_PrintUnformatted(props);
    char rk[400], title[400];
    snprintf(rk, sizeof rk, "sanc:%s:%s", service, cap->valuestring);
    snprintf(title, sizeof title, "%s — %s", service, cap->valuestring);
    intel_item it = {0};
    it.remote_key = rk; it.title = title; it.body = bj; it.summary = cap->valuestring;
    it.record_type = "osint_service_result"; it.properties_json = pj; it.tags_json = tags;
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(bj); free(pj); cJSON_Delete(out); cJSON_Delete(props);
  }
  cJSON_Delete(j);
  return emitted;
}

static int run_pep(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return 0;
  return opensanctions(ctx, sink, ctx->entity, "PEP_CHECK",
                       "[\"osint-search\",\"PEP_CHECK\"]", 1);
}
static int run_watchlist(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return 0;
  return opensanctions(ctx, sink, ctx->entity, "WATCHLIST_CHECK_NEW",
                       "[\"osint-search\",\"WATCHLIST_CHECK_NEW\"]", 0);
}

static const source_def pep_def = {
  .id = "PEP_CHECK", .collector = "osint", .name = "PEP Check", .run = run_pep,
  .category = "investigation", .type = "api", .url = "https://api.opensanctions.org/",
  .description = "Politically-exposed-person screening via OpenSanctions (requires OPENSANCTIONS_API_KEY).",
  .free_tier = 0,
};
REGISTER_SOURCE(pep_def)

static const source_def watchlist_def = {
  .id = "WATCHLIST_CHECK_NEW", .collector = "osint", .name = "Watchlist Check", .run = run_watchlist,
  .category = "investigation", .type = "api", .url = "https://api.opensanctions.org/",
  .description = "Sanctions / watchlist screening via OpenSanctions (requires OPENSANCTIONS_API_KEY).",
  .free_tier = 0,
};
REGISTER_SOURCE(watchlist_def)

/* Folded into PERSON_SEARCH (people_finder.c) while still registered standalone
 * as PEP_CHECK / WATCHLIST_CHECK_NEW — these expose the run entry points. */
int jo_pep_run(const source_ctx *ctx, intel_sink *sink) { return run_pep(ctx, sink); }
int jo_watchlist_run(const source_ctx *ctx, intel_sink *sink) { return run_watchlist(ctx, sink); }
