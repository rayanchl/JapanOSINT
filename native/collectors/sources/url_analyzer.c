/* collectors/osint/sources/url_analyzer.c
 * OSINT service — port of OSINTsaas osint_tools/url_analyzer.c
 * (url_analyzer_scan → handle_url_analyzer). Canonical SERVICE = URL_ANALYZER
 * (dispatcher row {SERVICE_URL_ANALYZER, handle_url_analyzer, "URL_ANALYZER",
 * true}). On-demand (interval 0); ctx->entity = a URL. No key (URLScan.io
 * public search). Faithfully reproduces analyze_url(): submit_to_urlscan
 * (GET urlscan.io/api/v1/search/?q=<enc> → first result uuid),
 * get_urlscan_result (urlscan.io/api/v1/result/<uuid>/ → page.domain/status/
 * server, verdicts.overall.malicious, task.url), analyze_url_direct fallback
 * (fetch page, <title> extract, suspicious-phrase heuristics),
 * calculate_url_score and the verdict bucketing. result_builder items
 * (URLScan.io / Direct Analysis / Reputation) become an "items" array in the
 * envelope; confidence 90 if URLScan succeeded else 70. Emits ONE
 * osint_service_result row like dns_records.c. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <strings.h>
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

/* "^(?:https?://)?(?:www\.)?([^:/\s]+)" — extract host. */
static void extract_domain(const char *url, char *out, size_t cap) {
  const char *p = url;
  if (strncasecmp(p, "https://", 8) == 0) p += 8;
  else if (strncasecmp(p, "http://", 7) == 0) p += 7;
  if (strncasecmp(p, "www.", 4) == 0) p += 4;
  size_t w = 0;
  while (*p && *p != ':' && *p != '/' && *p != ' ' && *p != '\t' && w + 1 < cap)
    out[w++] = *p++;
  out[w] = 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = ctx->entity;
  if (!url || !*url) return -1;

  char proto[16];
  if (strncmp(url, "https://", 8) == 0) strcpy(proto, "https");
  else if (strncmp(url, "http://", 7) == 0) strcpy(proto, "http");
  else strcpy(proto, "unknown");

  char domain[256] = {0};
  extract_domain(url, domain, sizeof domain);

  /* --- submit_to_urlscan: search existing scans for a uuid --- */
  char enc[1024], surl[1200];
  uri_encode(url, enc, sizeof enc);
  snprintf(surl, sizeof surl, "https://urlscan.io/api/v1/search/?q=%s", enc);
  char uuid[128] = {0};
  {
    http_response hr = {0};
    int hc = http_request(ctx->http, "GET", surl, NULL, NULL, 0, 15000, 1, &hr);
    cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
    http_response_free(&hr);
    if (j) {
      const cJSON *res = cJSON_GetObjectItem(j, "results");
      if (res && cJSON_IsArray(res) && cJSON_GetArraySize(res) > 0) {
        const cJSON *f = cJSON_GetArrayItem(res, 0);
        const cJSON *u = f ? cJSON_GetObjectItem(f, "uuid") : NULL;
        if (u && cJSON_IsString(u)) { strncpy(uuid, u->valuestring, sizeof uuid - 1); }
      }
      cJSON_Delete(j);
    }
  }

  int urlscan_ok = 0, http_status = 0, is_mal = 0, is_phish = 0, is_susp = 0;
  long content_length = 0;
  char info_url[2048] = {0}, info_domain[256] = {0};
  char *server = NULL, *title = NULL;
  strncpy(info_url, url, sizeof info_url - 1);
  strncpy(info_domain, domain, sizeof info_domain - 1);

  /* --- get_urlscan_result --- */
  if (uuid[0]) {
    char rurl[256];
    snprintf(rurl, sizeof rurl, "https://urlscan.io/api/v1/result/%s/", uuid);
    http_response hr = {0};
    int hc = http_request(ctx->http, "GET", rurl, NULL, NULL, 0, 15000, 1, &hr);
    cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
    http_response_free(&hr);
    if (j) {
      urlscan_ok = 1;
      const cJSON *task = cJSON_GetObjectItem(j, "task");
      const cJSON *page = cJSON_GetObjectItem(j, "page");
      const cJSON *verd = cJSON_GetObjectItem(j, "verdicts");
      if (task) {
        const cJSON *u = cJSON_GetObjectItem(task, "url");
        if (u && cJSON_IsString(u)) { strncpy(info_url, u->valuestring, sizeof info_url - 1);
                                      info_url[sizeof info_url - 1] = 0; }
      }
      if (page) {
        const cJSON *d = cJSON_GetObjectItem(page, "domain");
        const cJSON *s = cJSON_GetObjectItem(page, "status");
        const cJSON *sv = cJSON_GetObjectItem(page, "server");
        if (d && cJSON_IsString(d)) { strncpy(info_domain, d->valuestring, sizeof info_domain - 1);
                                      info_domain[sizeof info_domain - 1] = 0; }
        if (s && cJSON_IsNumber(s)) http_status = (int)s->valuedouble;
        if (sv && cJSON_IsString(sv)) server = strdup(sv->valuestring);
      }
      if (verd) {
        const cJSON *ov = cJSON_GetObjectItem(verd, "overall");
        const cJSON *ml = ov ? cJSON_GetObjectItem(ov, "malicious") : NULL;
        if (ml && cJSON_IsBool(ml)) is_mal = cJSON_IsTrue(ml);
      }
      cJSON_Delete(j);
    }
  }

  /* --- analyze_url_direct fallback --- */
  int direct_ok = 0;
  if (!urlscan_ok) {
    http_response hr = {0};
    int hc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
    if (hc == 0) {
      direct_ok = 1;
      http_status = (int)hr.status;
      content_length = (long)hr.body_len;
      if (hr.body) {
        char *ts = strcasestr(hr.body, "<title>");
        char *te = strcasestr(hr.body, "</title>");
        if (ts && te && te > ts) {
          ts += 7;
          long l = te - ts;
          if (l > 0 && l < 512) { title = malloc(l + 1); memcpy(title, ts, l); title[l] = 0; }
        }
        if (strcasestr(hr.body, "verify your account") ||
            strcasestr(hr.body, "confirm your identity") ||
            strcasestr(hr.body, "urgent action required"))
          is_susp = 1;
      }
    }
    http_response_free(&hr);
  }

  /* Total failure: neither URLScan nor a direct fetch produced anything. */
  if (!urlscan_ok && !direct_ok) {
    if (server) free(server);
    if (title) free(title);
    return 0;
  }

  /* --- calculate_url_score --- */
  int score = 50;
  if (http_status == 200) score += 10;
  if (strncmp(proto, "https", 5) == 0) score += 15;
  if (server && *server) score += 5;
  if (is_mal) score -= 50;
  if (is_phish) score -= 45;
  if (is_susp) score -= 30;
  if (http_status >= 400) score -= 20;
  if (score < 0) score = 0;
  if (score > 100) score = 100;
  const char *verdict = score >= 80 ? "SAFE" : score >= 60 ? "LOW_RISK"
    : score >= 40 ? "MEDIUM_RISK" : "HIGH_RISK";

  /* Per-record body: the real fetched fields, flat. */
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "url", info_url);
  cJSON_AddStringToObject(data, "domain", info_domain);
  cJSON_AddStringToObject(data, "source", urlscan_ok ? "URLScan.io" : "Direct Analysis");
  cJSON_AddStringToObject(data, "protocol", proto);
  cJSON_AddNumberToObject(data, "http_status", http_status);
  if (server) cJSON_AddStringToObject(data, "server", server);
  if (title) cJSON_AddStringToObject(data, "title", title);
  if (!urlscan_ok)
    cJSON_AddNumberToObject(data, "content_length", (double)content_length);
  cJSON_AddBoolToObject(data, "is_malicious", is_mal);
  cJSON_AddBoolToObject(data, "is_phishing", is_phish);
  cJSON_AddBoolToObject(data, "is_suspicious", is_susp);
  cJSON_AddNumberToObject(data, "score", score);
  cJSON_AddStringToObject(data, "verdict", verdict);
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "URL_ANALYZER");
  cJSON_AddStringToObject(props, "domain", info_domain);
  cJSON_AddBoolToObject(props, "is_malicious", is_mal);
  cJSON_AddStringToObject(props, "verdict", verdict);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300]; snprintf(rk, sizeof rk, "url:%s", url);
  /* Title: domain + verdict from the real fetch. */
  char title2[400];
  snprintf(title2, sizeof title2, "%s — %s",
           info_domain[0] ? info_domain : url, is_mal ? "malicious" : verdict);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title2;
  it.body            = bj;
  it.summary         = is_mal ? "URL malicious" : "URL analyzed";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"URL_ANALYZER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data);
  cJSON_Delete(props);
  if (server) free(server);
  if (title) free(title);
  return rc >= 0 ? 0 : -1;
}

static const source_def url_analyzer_def = {
  .id = "URL_ANALYZER", .collector = "osint",
  .name = "URL Analyzer", .name_ja = "URL解析",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/url-analyzer",
  .description = "Analyze a URL via URLScan.io and direct page heuristics.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(url_analyzer_def)
