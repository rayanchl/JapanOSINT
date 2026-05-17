/* collectors/osint/sources/dark_web_monitor.c
 * OSINT service — faithful port of OSINTsaas osint_tools/dark_web_monitor.c
 * (dark_web_search → handle_dark_web_monitor). Canonical SERVICE id in
 * osint_dispatcher.c: {SERVICE_DARK_WEB_MONITOR, handle_dark_web_monitor,
 * "DARK_WEB_MONITOR", true}. (DEEP_WEB_SEARCH alias → handle_deep_web_search,
 * a different handler → not this file.) Entity = search term or .onion URL.
 * INTELX_API_KEY is optional (its absence just omits the intelx sub-object;
 * ahmia.fi + pastebin are keyless), so NOT a hard gate. Reproduces upstream
 * `root` exactly: onion-URL branch → {query,query_type:"onion_url",
 * valid_onion_format,tor2web_url,disclaimer}; term branch →
 * {query,search_timestamp,query_type:"search_term", <monitor fields merged:
 * target,monitor_type,tor_search,paste_search,intelx?,total_mentions,
 * risk_score,risk_level>, disclaimer}. success=true conf 70. Emits one
 * osint_service_result row (body = {success,confidence,data}). */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

static int is_valid_onion(const char *url) {
  if (!url) return 0;
  const char *on = strstr(url, ".onion");
  if (!on) return 0;
  const char *start = url, *pr = strstr(url, "://");
  if (pr) start = pr + 3;
  size_t len = on - start;
  if (len != 16 && len != 56) return 0;
  for (size_t i = 0; i < len; i++) {
    char c = tolower((unsigned char)start[i]);
    if (!((c >= 'a' && c <= 'z') || (c >= '2' && c <= '7'))) return 0;
  }
  return 1;
}

/* Extract unique <addr>.onion strings from text. */
static cJSON *extract_onions(const char *text) {
  cJSON *a = cJSON_CreateArray();
  if (!text) return a;
  const char *p = text;
  while ((p = strstr(p, ".onion")) != NULL) {
    const char *e = p;
    const char *s = p;
    while (s > text) {
      char c = tolower((unsigned char)s[-1]);
      if ((c >= 'a' && c <= 'z') || (c >= '2' && c <= '7')) s--;
      else break;
    }
    size_t alen = (size_t)(e - s);
    if (alen >= 16) {
      size_t tot = alen + 6;
      char *o = malloc(tot + 1);
      memcpy(o, s, tot); o[tot] = 0;
      int dup = 0, n = cJSON_GetArraySize(a);
      for (int i = 0; i < n; i++) {
        cJSON *it = cJSON_GetArrayItem(a, i);
        if (it && cJSON_IsString(it) && strcmp(it->valuestring, o) == 0) { dup = 1; break; }
      }
      if (!dup) cJSON_AddItemToArray(a, cJSON_CreateString(o));
      free(o);
    }
    p = e + 6;
  }
  return a;
}

static cJSON *search_ahmia(http_client *h, const char *q) {
  char enc[1024]; uri_encode(q, enc, sizeof enc);
  char url[1100];
  snprintf(url, sizeof url, "https://ahmia.fi/search/?q=%s", enc);
  http_response hr = {0};
  int hc = http_request(h, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    http_response_free(&hr);
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "source", "Ahmia.fi");
    cJSON_AddStringToObject(r, "status", "unavailable");
    cJSON_AddStringToObject(r, "note",
      "Ahmia.fi may be temporarily unavailable or blocking requests");
    return r;
  }
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "Ahmia.fi");
  cJSON *on = extract_onions(hr.body);
  if (cJSON_GetArraySize(on) > 0) {
    cJSON_AddItemToObject(r, "onion_urls_found", on);
    cJSON_AddNumberToObject(r, "result_count", cJSON_GetArraySize(on));
  } else {
    cJSON_Delete(on);
    cJSON_AddNumberToObject(r, "result_count", 0);
  }
  http_response_free(&hr);
  return r;
}

static cJSON *search_pastes(http_client *h, const char *q) {
  cJSON *r = cJSON_CreateObject();
  cJSON *sites = cJSON_CreateArray();
  char enc[1024]; uri_encode(q, enc, sizeof enc);
  char url[1100];
  snprintf(url, sizeof url, "https://pastebin.com/search?q=%s", enc);
  http_response hr = {0};
  int hc = http_request(h, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  if (hc == 0 && hr.status == 200 && hr.body) {
    cJSON *pb = cJSON_CreateObject();
    cJSON_AddStringToObject(pb, "site", "Pastebin");
    cJSON_AddStringToObject(pb, "status", "queried");
    int pc = 0; const char *p = hr.body;
    while ((p = strstr(p, "/raw/")) != NULL) { pc++; p++; }
    cJSON_AddNumberToObject(pb, "estimated_results", pc);
    cJSON_AddStringToObject(pb, "search_url", url);
    cJSON_AddItemToArray(sites, pb);
  }
  http_response_free(&hr);

  snprintf(url, sizeof url, "https://ghostbin.com/search?q=%s", enc);
  memset(&hr, 0, sizeof hr);
  hc = http_request(h, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  if (hc == 0 && hr.status == 200) {
    cJSON *gb = cJSON_CreateObject();
    cJSON_AddStringToObject(gb, "site", "Ghostbin");
    cJSON_AddStringToObject(gb, "status", "queried");
    cJSON_AddItemToArray(sites, gb);
  }
  http_response_free(&hr);

  cJSON_AddItemToObject(r, "paste_sites", sites);
  return r;
}

static cJSON *search_intelx(http_client *h, const char *q) {
  const char *key = getenv("INTELX_API_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[DARK_WEB_MONITOR] intelx skipped (no INTELX_API_KEY)\n");
    return NULL;   /* upstream returns NULL → key omitted from output */
  }
  cJSON *req = cJSON_CreateObject();
  cJSON_AddStringToObject(req, "term", q);
  cJSON_AddNumberToObject(req, "maxresults", 100);
  cJSON_AddNumberToObject(req, "media", 0);
  cJSON_AddNumberToObject(req, "timeout", 20);
  char *body = cJSON_PrintUnformatted(req);
  cJSON_Delete(req);
  char url[256];
  snprintf(url, sizeof url, "https://2.intelx.io/intelligent/search?k=%s", key);
  const char *hdr[2] = { "Content-Type: application/json", NULL };
  http_response hr = {0};
  int hc = http_request(h, "POST", url, hdr, body, strlen(body), 20000, 1, &hr);
  free(body);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  cJSON *j = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!j) return NULL;
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "IntelX");
  cJSON *id = cJSON_GetObjectItem(j, "id");
  if (id && cJSON_IsString(id)) {
    cJSON_AddStringToObject(r, "search_id", id->valuestring);
    cJSON_AddStringToObject(r, "status", "pending");
    cJSON_AddStringToObject(r, "note",
      "IntelX search initiated. Use search_id to retrieve results.");
  }
  cJSON_Delete(j);
  return r;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", q);

  if (is_valid_onion(q)) {
    cJSON_AddStringToObject(root, "query_type", "onion_url");
    cJSON_AddBoolToObject(root, "valid_onion_format", 1);
    char *oc = strdup(q);
    char *d = strstr(oc, ".onion");
    if (d) {
      strcpy(d, ".onion.ws");
      char t2[600];
      if (strncmp(oc, "http", 4) != 0)
        snprintf(t2, sizeof t2, "https://%s", oc);
      else
        snprintf(t2, sizeof t2, "%s", oc);
      cJSON_AddStringToObject(root, "tor2web_url", t2);
    }
    free(oc);
  } else {
    cJSON_AddNumberToObject(root, "search_timestamp", (double)time(NULL));
    cJSON_AddStringToObject(root, "query_type", "search_term");
    cJSON_AddStringToObject(root, "target", q);
    cJSON_AddStringToObject(root, "monitor_type", "dark_web_mention");

    cJSON *ah = search_ahmia(ctx->http, q);
    cJSON_AddItemToObject(root, "tor_search", ah);
    cJSON *pa = search_pastes(ctx->http, q);
    cJSON_AddItemToObject(root, "paste_search", pa);
    cJSON *ix = search_intelx(ctx->http, q);
    if (ix) cJSON_AddItemToObject(root, "intelx", ix);

    int risk = 0, total = 0;
    cJSON *cnt = cJSON_GetObjectItem(ah, "result_count");
    if (cnt && cJSON_IsNumber(cnt)) {
      total += cnt->valueint;
      if (cnt->valueint > 0) risk += 30;
    }
    cJSON *sites = cJSON_GetObjectItem(pa, "paste_sites");
    if (sites && cJSON_IsArray(sites)) {
      int sn = cJSON_GetArraySize(sites);
      for (int i = 0; i < sn; i++) {
        cJSON *s = cJSON_GetArrayItem(sites, i);
        cJSON *e = cJSON_GetObjectItem(s, "estimated_results");
        if (e && cJSON_IsNumber(e)) {
          total += e->valueint;
          if (e->valueint > 0) risk += 20;
        }
      }
    }
    cJSON_AddNumberToObject(root, "total_mentions", total);
    cJSON_AddNumberToObject(root, "risk_score", risk > 100 ? 100 : risk);
    cJSON_AddStringToObject(root, "risk_level",
      risk >= 60 ? "HIGH" : risk >= 30 ? "MEDIUM" : "LOW");
  }

  cJSON_AddStringToObject(root, "disclaimer",
    "Dark web monitoring is for security research purposes only. "
    "Results may include false positives. Always verify findings through proper channels.");

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", 70);
  cJSON_AddItemToObject(env, "data", root);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "DARK_WEB_MONITOR");
  cJSON_AddStringToObject(props, "entity", q);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 70);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300], title[320];
  snprintf(rk, sizeof rk, "darkweb:%s", q);
  snprintf(title, sizeof title, "DARK_WEB_MONITOR — %s", q);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "dark web monitoring complete";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"DARK_WEB_MONITOR\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def dark_web_monitor_def = {
  .id = "DARK_WEB_MONITOR", .collector = "osint",
  .name = "Dark Web Monitor", .name_ja = "ダークウェブ監視",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(dark_web_monitor_def)
