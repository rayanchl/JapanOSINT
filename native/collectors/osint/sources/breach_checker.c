/* collectors/osint/sources/breach_checker.c
 * OSINT service — faithful port of OSINTsaas osint_tools/breach_checker.c
 * (breach_check_email → query_haveibeenpwned). Canonical SERVICE id in
 * osint_dispatcher.c: {SERVICE_BREACH_CHECKER, handle_breach_checker,
 * "BREACH_CHECKER", true}. Entity = email. Upstream: HIBP API v3
 * breachedaccount endpoint. Key HIBP_API_KEY is used for the authenticated
 * attempt; upstream then *also* falls back to an unauthenticated GET, so this
 * is NOT a hard no-key gate (faithful: still attempts the request without a
 * key — HIBP simply 401/429s and we surface that, mirroring upstream's
 * http_code-branch result objects exactly).
 *
 * Faithful result mapping (query_haveibeenpwned):
 *  200 + array → {email,breached:true,breach_count,breaches:[…≤50],summary},
 *                 success=true conf=95
 *  404         → {email,breached:false,message:"No breaches found"},
 *                 success=true conf=90
 *  429         → {email,error:"Rate limited",note:…}, success=false conf=0
 *  other       → {email,error:"HTTP <code>",note:…}, success=false conf=0
 *  net fail / parse-fail → upstream returns NULL (no service row). We emit a
 *                 success=false envelope with error so the pipeline still has
 *                 a deterministic row (the unified sink requires one emit).
 * Emits one osint_service_result row (body = envelope), like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* encodeURIComponent-equivalent (upstream uses url_encode on the email). */
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

static int emit_result(intel_sink *sink, const char *email, int success,
                        int confidence, cJSON *data /*owned*/,
                        const char *err) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  if (data) cJSON_AddItemToObject(env, "data", data);
  else cJSON_AddNullToObject(env, "data");
  if (err) cJSON_AddStringToObject(env, "error", err);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "BREACH_CHECKER");
  cJSON_AddStringToObject(props, "entity", email);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300], title[320];
  snprintf(rk, sizeof rk, "breach:%s", email);
  snprintf(title, sizeof title, "BREACH_CHECKER — %s", email);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = success ? "breach lookup complete"
                               : (err ? err : "breach lookup failed");
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"BREACH_CHECKER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *email = ctx->entity;
  if (!email || !*email) return -1;

  char enc[256];
  uri_encode(email, enc, sizeof enc);
  char url[512];
  snprintf(url, sizeof url,
    "https://haveibeenpwned.com/api/v3/breachedaccount/%s?truncateResponse=false",
    enc);

  /* Authenticated attempt if HIBP_API_KEY present, else unauth (upstream does
   * exactly this fallback). hibp-api-key header only when key is set. */
  const char *key = getenv("HIBP_API_KEY");
  const char *hdr_auth[3];
  const char *hdr_none[2] = { "User-Agent: OSINT-SaaS-Platform/1.0", NULL };
  const char *const *headers = hdr_none;
  char keybuf[256];
  if (key && *key) {
    snprintf(keybuf, sizeof keybuf, "hibp-api-key: %s", key);
    hdr_auth[0] = keybuf;
    hdr_auth[1] = "User-Agent: OSINT-SaaS-Platform/1.0";
    hdr_auth[2] = NULL;
    headers = hdr_auth;
  }

  http_response hr = {0};
  int hc = http_request(ctx->http, "GET", url, headers, NULL, 0, 15000, 1, &hr);
  long code = hr.status;

  if (hc != 0) { http_response_free(&hr); /* net fail → upstream NULL */
    return emit_result(sink, email, 0, 0, NULL, "network error"); }

  if (code == 200 && hr.body) {
    cJSON *j = cJSON_Parse(hr.body);
    if (j && cJSON_IsArray(j)) {
      int n = cJSON_GetArraySize(j);
      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "email", email);
      cJSON_AddBoolToObject(data, "breached", 1);
      cJSON_AddNumberToObject(data, "breach_count", n);
      cJSON *arr = cJSON_CreateArray();
      for (int i = 0; i < n && i < 50; i++) {
        cJSON *b = cJSON_GetArrayItem(j, i);
        cJSON *nm = cJSON_GetObjectItem(b, "Name");
        if (!(nm && cJSON_IsString(nm))) continue;
        cJSON *e = cJSON_CreateObject();
        cJSON_AddStringToObject(e, "name", nm->valuestring);
        cJSON *dom = cJSON_GetObjectItem(b, "Domain");
        if (dom && cJSON_IsString(dom))
          cJSON_AddStringToObject(e, "domain", dom->valuestring);
        cJSON *bd = cJSON_GetObjectItem(b, "BreachDate");
        if (bd && cJSON_IsString(bd))
          cJSON_AddStringToObject(e, "date", bd->valuestring);
        cJSON *pc = cJSON_GetObjectItem(b, "PwnCount");
        if (pc && cJSON_IsNumber(pc))
          cJSON_AddNumberToObject(e, "accounts_affected", (int)pc->valuedouble);
        cJSON_AddItemToArray(arr, e);
      }
      cJSON_AddItemToObject(data, "breaches", arr);
      char summ[96];
      snprintf(summ, sizeof summ, "Email found in %d data breaches", n);
      cJSON_AddStringToObject(data, "summary", summ);
      cJSON_Delete(j);
      http_response_free(&hr);
      return emit_result(sink, email, 1, 95, data, NULL);
    }
    if (j) cJSON_Delete(j);
    http_response_free(&hr);
    return emit_result(sink, email, 0, 0, NULL,
                       "Failed to parse HIBP response");
  }

  if (code == 404) {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "email", email);
    cJSON_AddBoolToObject(data, "breached", 0);
    cJSON_AddStringToObject(data, "message", "No breaches found");
    http_response_free(&hr);
    return emit_result(sink, email, 1, 90, data, NULL);
  }

  if (code == 429) {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "email", email);
    cJSON_AddStringToObject(data, "error", "Rate limited");
    cJSON_AddStringToObject(data, "note",
      "HIBP requires API key for automated queries");
    http_response_free(&hr);
    return emit_result(sink, email, 0, 0, data, NULL);
  }

  {
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "email", email);
    char eb[48];
    snprintf(eb, sizeof eb, "HTTP %ld", code);
    cJSON_AddStringToObject(data, "error", eb);
    cJSON_AddStringToObject(data, "note",
      "Configure HIBP API key for automated queries");
    http_response_free(&hr);
    return emit_result(sink, email, 0, 0, data, NULL);
  }
}

static const source_def breach_checker_def = {
  .id = "BREACH_CHECKER", .collector = "osint",
  .name = "Breach Checker", .name_ja = "漏洩チェック",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(breach_checker_def)
