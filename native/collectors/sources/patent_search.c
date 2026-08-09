/* collectors/osint/sources/patent_search.c — PATENT_SEARCH, ported onto the
 * JapanOSINT source ABI. OSINTsaas routed PATENT_SEARCH through trademark DBs;
 * here it queries PatentsView (the de-facto free patent API) for real granted
 * patents by title. PatentsView now requires an API key, so this gates on
 * PATENTSVIEW_API_KEY and is honest-empty without one. (Japan-specific patents
 * are already covered by the scheduled JPO collectors.) */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../core/httpclient.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int patent_run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *key = getenv("PATENTSVIEW_API_KEY");
  if (!key || !*key) {                          /* honest empty without key */
    fprintf(stderr, "[patent-search] gated (no PATENTSVIEW_API_KEY)\n");
    return 0;
  }

  /* The entity is analyst-supplied and used to be pasted straight into a JSON
   * literal, so a company name containing a `"` (or a Windows path with a `\`)
   * produced a body PatentsView rejects. cJSON escapes it correctly. */
  cJSON *qo = cJSON_CreateObject(), *qany = cJSON_CreateObject();
  cJSON_AddStringToObject(qany, "patent_title", q);
  cJSON_AddItemToObject(qo, "_text_any", qany);
  char *qjson = cJSON_PrintUnformatted(qo);
  cJSON_Delete(qo);
  if (!qjson) return 0;
  char qenc[800]; jo_urlencode_buf(qjson, qenc, sizeof qenc);
  free(qjson);
  const char *fenc = "%5B%22patent_id%22%2C%22patent_title%22%2C%22patent_date%22%5D";
  char url[1100];
  snprintf(url, sizeof url,
    "https://search.patentsview.org/api/v1/patent/?q=%s&f=%s&o=%%7B%%22size%%22%%3A15%%7D",
    qenc, fenc);
  char hk[160]; snprintf(hk, sizeof hk, "X-Api-Key: %s", key);
  const char *headers[] = { hk, NULL };

  http_response hr = {0};
  if (http_request(ctx->http, "GET", url, headers, NULL, 0, 15000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  if (!j) return 0;
  cJSON *pats = cJSON_GetObjectItem(j, "patents");
  int emitted = 0, n = (pats && cJSON_IsArray(pats)) ? cJSON_GetArraySize(pats) : 0;
  for (int i = 0; i < n; i++) {
    cJSON *p = cJSON_GetArrayItem(pats, i);
    const char *pid = jo_str(p, "patent_id");
    const char *ptitle = jo_str(p, "patent_title");
    const char *pdate = jo_str(p, "patent_date");
    if (!pid) continue;
    cJSON *out = cJSON_CreateObject();
    cJSON_AddStringToObject(out, "source", "patentsview.org");
    cJSON_AddStringToObject(out, "patent_id", pid);
    if (ptitle) cJSON_AddStringToObject(out, "title", ptitle);
    if (pdate) cJSON_AddStringToObject(out, "date", pdate);
    char link[256]; snprintf(link, sizeof link, "https://patents.google.com/patent/US%s", pid);
    cJSON_AddStringToObject(out, "url", link);
    char *bj = cJSON_PrintUnformatted(out);
    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "PATENT_SEARCH");
    cJSON_AddStringToObject(props, "entity", q);
    char *pj = cJSON_PrintUnformatted(props);
    char rk[128], title[400];
    snprintf(rk, sizeof rk, "patent:%s", pid);
    snprintf(title, sizeof title, "Patent %s — %s", pid, ptitle ? ptitle : q);
    intel_item it = {0};
    it.remote_key = rk; it.title = title; it.body = bj;
    it.summary = ptitle ? ptitle : "patent"; it.link = link;
    it.record_type = "osint_service_result"; it.properties_json = pj;
    it.tags_json = "[\"osint-search\",\"PATENT_SEARCH\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(bj); free(pj); cJSON_Delete(out); cJSON_Delete(props);
  }
  cJSON_Delete(j);
  fprintf(stderr, "[PATENT_SEARCH] emitted %d\n", emitted);
  /* STATUS code, not a row count: the fetch and parse both succeeded above, so
   * a query with no granted patents is an honest empty, not an errored run. */
  return 0;
}

static const source_def patent_def = {
  .id = "PATENT_SEARCH", .collector = "osint", .name = "Patent Search", .run = patent_run,
  .category = "investigation", .type = "api", .url = "https://search.patentsview.org/",
  .description = "Granted-patent search by title via PatentsView (requires PATENTSVIEW_API_KEY).",
  .free_tier = 0,
};
REGISTER_SOURCE(patent_def)
