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
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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

/* search_ahmia: real onion-search scrape → array of discovered .onion URLs.
 * Returns the (possibly empty) array; caller owns it. */
static cJSON *search_ahmia(http_client *h, const char *q) {
  char enc[1024]; jo_uri_encode_buf(q, enc, sizeof enc);
  char url[1100];
  snprintf(url, sizeof url, "https://ahmia.fi/search/?q=%s", enc);
  http_response hr = {0};
  int hc = http_request(h, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    http_response_free(&hr);
    return cJSON_CreateArray();
  }
  cJSON *on = extract_onions(hr.body);
  http_response_free(&hr);
  return on;
}

/* search_pastebin: real Pastebin search → the DISTINCT paste URLs the result
 * page links to. Each hit in the fetched HTML is a `/raw/<key>` reference; we
 * surface every one of them (exhaustive use — the earlier version counted the
 * `/raw/` occurrences and discarded the paste identities behind them). Returns
 * a (possibly empty) array of "https://pastebin.com/<key>" strings; caller owns
 * it. */
static cJSON *search_pastebin(http_client *h, const char *q) {
  cJSON *a = cJSON_CreateArray();
  char enc[1024]; jo_uri_encode_buf(q, enc, sizeof enc);
  char url[1100];
  snprintf(url, sizeof url, "https://pastebin.com/search?q=%s", enc);
  http_response hr = {0};
  int hc = http_request(h, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  if (hc == 0 && hr.status == 200 && hr.body) {
    const char *p = hr.body;
    while ((p = strstr(p, "/raw/")) != NULL) {
      const char *k = p + 5;                 /* start of the paste key */
      size_t klen = 0;
      while (k[klen] && ((k[klen] >= 'a' && k[klen] <= 'z') ||
                         (k[klen] >= 'A' && k[klen] <= 'Z') ||
                         (k[klen] >= '0' && k[klen] <= '9')) && klen < 16)
        klen++;
      if (klen >= 6) {                        /* pastebin keys are ~8 chars */
        char purl[64];
        snprintf(purl, sizeof purl, "https://pastebin.com/%.*s", (int)klen, k);
        int dup = 0, n = cJSON_GetArraySize(a);
        for (int i = 0; i < n; i++) {
          cJSON *it = cJSON_GetArrayItem(a, i);
          if (it && cJSON_IsString(it) && strcmp(it->valuestring, purl) == 0) { dup = 1; break; }
        }
        if (!dup) cJSON_AddItemToArray(a, cJSON_CreateString(purl));
      }
      p = k + (klen ? klen : 1);
    }
  }
  http_response_free(&hr);
  return a;
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

/* Emit one item. data is owned by this fn (duplicated into envelope). */
static int emit_item(intel_sink *sink, const char *q, const char *rk,
                     const char *title, const char *summary, cJSON *data) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", 70);
  cJSON_AddItemToObject(env, "data", data);   /* takes ownership */
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "DARK_WEB_MONITOR");
  cJSON_AddStringToObject(props, "entity", q);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 70);
  char *pj = cJSON_PrintUnformatted(props);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = summary;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"DARK_WEB_MONITOR\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 1 : 0;
}

/* PER-RECORD EMIT: one item per discovered .onion URL (real Ahmia scrape),
 * one item per distinct Pastebin paste the search links to (real /raw/ keys),
 * and one item if IntelX (key-gated) returns a real search id. Nothing found →
 * emit nothing, return 0. */
static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;

  int emitted = 0;

  /* One item per discovered .onion URL. */
  cJSON *onions = search_ahmia(ctx->http, q);
  int on_n = cJSON_GetArraySize(onions);
  for (int i = 0; i < on_n; i++) {
    cJSON *o = cJSON_GetArrayItem(onions, i);
    if (!o || !cJSON_IsString(o)) continue;
    const char *ourl = o->valuestring;
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "onion_url", ourl);
    cJSON_AddStringToObject(data, "source", "Ahmia.fi");
    cJSON_AddStringToObject(data, "query", q);
    char rk[600]; snprintf(rk, sizeof rk, "onion:%s", ourl);
    char title[640]; snprintf(title, sizeof title, ".onion — %s", ourl);
    emitted += emit_item(sink, q, rk, title, "dark web onion result", data);
  }
  cJSON_Delete(onions);

  /* One item per distinct paste the Pastebin search links to — every paste
   * identity behind the /raw/ hits is surfaced, not just an aggregate count. */
  cJSON *pastes = search_pastebin(ctx->http, q);
  int paste_n = cJSON_GetArraySize(pastes);
  for (int i = 0; i < paste_n; i++) {
    cJSON *pu = cJSON_GetArrayItem(pastes, i);
    if (!pu || !cJSON_IsString(pu)) continue;
    const char *purl = pu->valuestring;
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "source", "Pastebin");
    cJSON_AddStringToObject(data, "query", q);
    cJSON_AddStringToObject(data, "paste_url", purl);
    char rk[320]; snprintf(rk, sizeof rk, "paste:%s", purl);
    char title[360]; snprintf(title, sizeof title, "Pastebin paste — %s", purl);
    emitted += emit_item(sink, q, rk, title, "pastebin paste", data);
  }
  cJSON_Delete(pastes);

  /* IntelX only when INTELX_API_KEY is set and returns a real search id. */
  cJSON *ix = search_intelx(ctx->http, q);
  if (ix) {
    cJSON *sid = cJSON_GetObjectItem(ix, "search_id");
    if (sid && cJSON_IsString(sid)) {
      cJSON *data = cJSON_Duplicate(ix, 1);
      cJSON_AddStringToObject(data, "query", q);
      char rk[320]; snprintf(rk, sizeof rk, "intelx:%s:%s", sid->valuestring, q);
      char title[360]; snprintf(title, sizeof title, "IntelX search — %s", q);
      emitted += emit_item(sink, q, rk, title, "intelx search", data);
    }
    cJSON_Delete(ix);
  }

  (void)emitted;
  return 0;   /* honest empty is not an error */
}

static const source_def dark_web_monitor_def = {
  .id = "DARK_WEB_MONITOR", .collector = "osint",
  .name = "Dark Web Monitor", .name_ja = "ダークウェブ監視",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/dark-web-monitor",
  .description = "Searches Ahmia, paste sites and IntelX for dark web mentions",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(dark_web_monitor_def)
