/* collectors/osint/sources/breach_checker.c
 * OSINT service — BREACH_CHECKER. On-demand (interval 0); ctx->entity = email.
 * Upstream: HIBP API v3 breachedaccount endpoint, key-gated on HIBP_API_KEY.
 *
 * PER-RECORD EMIT:
 *   - With HIBP_API_KEY and a 200 + non-empty array: emit ONE item per breach
 *     (remote_key="breach:<name>:<account>", body=real name/domain/date/
 *     PwnCount).
 *   - 404 (no breaches), 401/429 (key absent or rate-limited), or any other
 *     status: emit NOTHING (no note row). Without the key set, do not even
 *     issue the request — emit nothing. Never fabricate. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "social_fuse.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Emit one intel row for a single HIBP breach. Returns 1 if emitted. */
static int emit_breach(intel_sink *sink, const char *account, cJSON *b) {
  cJSON *nm = cJSON_GetObjectItem(b, "Name");
  if (!(nm && cJSON_IsString(nm))) return 0;

  cJSON *body = cJSON_CreateObject();
  cJSON_AddStringToObject(body, "account", account);
  cJSON_AddStringToObject(body, "name", nm->valuestring);
  cJSON *dom = cJSON_GetObjectItem(b, "Domain");
  if (dom && cJSON_IsString(dom)) cJSON_AddStringToObject(body, "domain", dom->valuestring);
  cJSON *bd = cJSON_GetObjectItem(b, "BreachDate");
  if (bd && cJSON_IsString(bd)) cJSON_AddStringToObject(body, "date", bd->valuestring);
  cJSON *pc = cJSON_GetObjectItem(b, "PwnCount");
  if (pc && cJSON_IsNumber(pc))
    cJSON_AddNumberToObject(body, "accounts_affected", (int)pc->valuedouble);
  cJSON *title_j = cJSON_GetObjectItem(b, "Title");
  if (title_j && cJSON_IsString(title_j))
    cJSON_AddStringToObject(body, "title", title_j->valuestring);
  char *bj = cJSON_PrintUnformatted(body);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "BREACH_CHECKER");
  cJSON_AddStringToObject(props, "account", account);
  cJSON_AddStringToObject(props, "breach", nm->valuestring);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[400], title[400];
  snprintf(rk, sizeof rk, "breach:%s:%s", nm->valuestring, account);
  snprintf(title, sizeof title, "%s — %s",
           (title_j && cJSON_IsString(title_j)) ? title_j->valuestring : nm->valuestring,
           account);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = (dom && cJSON_IsString(dom)) ? dom->valuestring : nm->valuestring;
  it.published_at    = (bd && cJSON_IsString(bd)) ? bd->valuestring : NULL;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"BREACH_CHECKER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(body); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

int jo_breach_checker_run(const source_ctx *ctx, intel_sink *sink) {
  const char *email = ctx->entity;
  if (!email || !*email) return 0;

  /* Key-gated: without HIBP_API_KEY, emit nothing (HIBP 401s anyway). */
  const char *key = getenv("HIBP_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[BREACH_CHECKER] gated (no HIBP_API_KEY)\n");
    return 0;
  }

  char enc[256];
  jo_uri_encode_buf(email, enc, sizeof enc);
  char url[512];
  snprintf(url, sizeof url,
    "https://haveibeenpwned.com/api/v3/breachedaccount/%s?truncateResponse=false",
    enc);

  char keybuf[256];
  snprintf(keybuf, sizeof keybuf, "hibp-api-key: %s", key);
  const char *headers[3] = {
    keybuf, "User-Agent: OSINT-SaaS-Platform/1.0", NULL
  };

  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", url, headers, NULL, 0, 15000, 1, &hr);
  long code = hr.status;

  /* Only a 200 + non-empty array carries real breach data. 404 = no breaches,
   * 401/429/other = no usable data → emit nothing. */
  if (hc != 0 || code != 200 || !hr.body) { http_response_free(&hr); return 0; }

  cJSON *j = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!j || !cJSON_IsArray(j)) { if (j) cJSON_Delete(j); return 0; }

  int n = cJSON_GetArraySize(j);
  for (int i = 0; i < n; i++) emit_breach(sink, email, cJSON_GetArrayItem(j, i));
  cJSON_Delete(j);
  return 0;
}

/* Fused into SOCIAL_EMAIL — exposed via social_fuse.h as jo_breach_checker_run. */
