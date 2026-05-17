/* collectors/osint/sources/threat_intel.c
 * OSINT service — port of OSINTsaas osint_tools/threat_intel.c
 * (threat_intel_check → handle_threat_intel). Canonical SERVICE = THREAT_INTEL
 * (dispatcher row {SERVICE_THREAT_INTEL, handle_threat_intel, "THREAT_INTEL",
 * true}; THREAT_FEED_LOOKUP maps to a different handler/source). On-demand
 * (interval 0); ctx->entity = an IP or domain (handle_threat_intel: IP →
 * threat_intel_ip, domain → threat_intel_domain, else try IP then domain).
 * Upstream: AbuseIPDB (api.abuseipdb.com, requires ABUSEIPDB_API_KEY — the C
 * code degrades gracefully without it, so we do too: skip that source, keep
 * OTX) and AlienVault OTX (otx.alienvault.com, no key). Faithfully reproduces
 * analyze_ip_threat / analyze_domain_threat: query_abuseipdb (Key + Accept
 * headers), query_alienvault_otx (pulse_info.count → is_malicious, pulse tags
 * → threat_types) and the summary/verdict item. result_builder items become an
 * "items" array in the envelope data; confidence 90/85/60/50 as the original.
 * Emits ONE osint_service_result row like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static int looks_ipv4(const char *s) {
  int parts = 0;
  while (*s) {
    if (!isdigit((unsigned char)*s)) return 0;
    int v = 0, d = 0;
    while (isdigit((unsigned char)*s)) { v = v * 10 + (*s - '0'); s++; d++; }
    if (d == 0 || d > 3 || v > 255) return 0;
    parts++;
    if (*s == '.') s++;
    else if (*s == 0) break;
    else return 0;
  }
  return parts == 4;
}

static int otx_query(http_client *http, const char *target, const char *itype,
                     int *is_mal, int *total_reports, char *ttypes, size_t tcap) {
  char url[512];
  snprintf(url, sizeof url,
    "https://otx.alienvault.com/api/v1/indicators/%s/%s/general", itype, target);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  if (!j) return 0;
  const cJSON *pi = cJSON_GetObjectItem(j, "pulse_info");
  if (pi) {
    const cJSON *cnt = cJSON_GetObjectItem(pi, "count");
    if (cnt && cJSON_IsNumber(cnt)) {
      int pc = (int)cnt->valuedouble;
      *total_reports += pc;
      if (pc > 0) *is_mal = 1;
    }
    const cJSON *pulses = cJSON_GetObjectItem(pi, "pulses");
    if (pulses && cJSON_IsArray(pulses)) {
      int pn = cJSON_GetArraySize(pulses), off = 0;
      ttypes[0] = 0;
      for (int i = 0; i < pn && i < 10; i++) {
        const cJSON *p = cJSON_GetArrayItem(pulses, i);
        const cJSON *tags = p ? cJSON_GetObjectItem(p, "tags") : NULL;
        if (tags && cJSON_IsArray(tags)) {
          int tn = cJSON_GetArraySize(tags);
          for (int k = 0; k < tn && k < 5; k++) {
            const cJSON *tg = cJSON_GetArrayItem(tags, k);
            if (tg && cJSON_IsString(tg)) {
              if (off > 0) off += snprintf(ttypes + off, tcap - off, ", ");
              off += snprintf(ttypes + off, tcap - off, "%s", tg->valuestring);
            }
          }
        }
      }
    }
  }
  cJSON_Delete(j);
  return 1;
}

static int abuseipdb_query(http_client *http, const char *ip, int *score,
                           int *total, int *is_tor, char *country, char *isp,
                           char *last) {
  const char *key = getenv("ABUSEIPDB_API_KEY");
  if (!key || !*key) return 0;   /* C code degrades gracefully w/o key */
  char url[512];
  snprintf(url, sizeof url,
    "https://api.abuseipdb.com/api/v2/check?ipAddress=%s&maxAgeInDays=90&verbose", ip);
  const char *hdrs[5];
  int h = 0;
  hdrs[h++] = "Key"; hdrs[h++] = key;
  hdrs[h++] = "Accept"; hdrs[h++] = "application/json";
  hdrs[h] = NULL;
  http_response hr = {0};
  int hc = http_request(http, "GET", url, hdrs, NULL, 0, 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  if (!j) return 0;
  const cJSON *d = cJSON_GetObjectItem(j, "data");
  if (d) {
    const cJSON *sc = cJSON_GetObjectItem(d, "abuseConfidenceScore");
    const cJSON *cc = cJSON_GetObjectItem(d, "countryCode");
    const cJSON *is = cJSON_GetObjectItem(d, "isp");
    const cJSON *tr = cJSON_GetObjectItem(d, "totalReports");
    const cJSON *it = cJSON_GetObjectItem(d, "isTor");
    const cJSON *lr = cJSON_GetObjectItem(d, "lastReportedAt");
    if (sc && cJSON_IsNumber(sc)) *score = (int)sc->valuedouble;
    if (cc && cJSON_IsString(cc)) { strncpy(country, cc->valuestring, 63); country[63] = 0; }
    if (is && cJSON_IsString(is)) { strncpy(isp, is->valuestring, 127); isp[127] = 0; }
    if (tr && cJSON_IsNumber(tr)) *total = (int)tr->valuedouble;
    if (it && cJSON_IsBool(it)) *is_tor = cJSON_IsTrue(it);
    if (lr && cJSON_IsString(lr)) { strncpy(last, lr->valuestring, 63); last[63] = 0; }
  }
  cJSON_Delete(j);
  return 1;
}

static int emit_result(intel_sink *sink, const char *entity, int success,
                        int confidence, cJSON *data /*owned*/) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  cJSON_AddItemToObject(env, "data", data);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "THREAT_INTEL");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300]; snprintf(rk, sizeof rk, "threatintel:%s", entity);
  char title[320]; snprintf(title, sizeof title, "THREAT_INTEL — %s", entity);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = success ? "threat intel" : "threat intel (unsupported)";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"THREAT_INTEL\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int analyze_ip(const source_ctx *ctx, intel_sink *sink, const char *ip) {
  int score = 0, total = 0, is_tor = 0, is_mal = 0;
  char country[64] = {0}, isp[128] = {0}, last[64] = {0}, ttypes[1024] = {0};
  int abuse_ok = abuseipdb_query(ctx->http, ip, &score, &total, &is_tor,
                                 country, isp, last);
  if (abuse_ok && score > 50) is_mal = 1;
  int otx_ok = otx_query(ctx->http, ip, "IPv4", &is_mal, &total, ttypes, sizeof ttypes);

  cJSON *items = cJSON_CreateArray();
  if (abuse_ok) {
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "ip", ip);
    cJSON_AddNumberToObject(d, "abuse_confidence_score", score);
    cJSON_AddNumberToObject(d, "total_reports", total);
    cJSON_AddBoolToObject(d, "is_tor_exit", is_tor);
    if (*country) cJSON_AddStringToObject(d, "country", country);
    if (*isp) cJSON_AddStringToObject(d, "isp", isp);
    if (*last) cJSON_AddStringToObject(d, "last_reported", last);
    cJSON *it = cJSON_CreateObject();
    cJSON_AddStringToObject(it, "source", "AbuseIPDB");
    cJSON_AddBoolToObject(it, "found", 1);
    cJSON_AddStringToObject(it, "url", "https://www.abuseipdb.com/");
    cJSON_AddItemToObject(it, "data", d);
    cJSON_AddItemToArray(items, it);
  }
  if (otx_ok) {
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "ip", ip);
    cJSON_AddNumberToObject(d, "pulse_count", total);
    cJSON_AddBoolToObject(d, "is_malicious", is_mal);
    if (*ttypes) cJSON_AddStringToObject(d, "threat_types", ttypes);
    cJSON *it = cJSON_CreateObject();
    cJSON_AddStringToObject(it, "source", "AlienVault OTX");
    cJSON_AddBoolToObject(it, "found", 1);
    cJSON_AddStringToObject(it, "url", "https://otx.alienvault.com/");
    cJSON_AddItemToObject(it, "data", d);
    cJSON_AddItemToArray(items, it);
  }
  cJSON *sum = cJSON_CreateObject();
  cJSON_AddStringToObject(sum, "target", ip);
  cJSON_AddStringToObject(sum, "type", "ip");
  cJSON_AddBoolToObject(sum, "is_malicious", is_mal);
  cJSON_AddBoolToObject(sum, "is_tor_exit", is_tor);
  cJSON_AddBoolToObject(sum, "is_vpn", 0);
  cJSON_AddBoolToObject(sum, "is_proxy", 0);
  cJSON_AddStringToObject(sum, "verdict",
    is_mal ? "MALICIOUS" : (score > 50 ? "SUSPICIOUS" : "CLEAN"));
  cJSON *si = cJSON_CreateObject();
  cJSON_AddStringToObject(si, "source", "Threat Summary");
  cJSON_AddBoolToObject(si, "found", 1);
  cJSON_AddItemToObject(si, "data", sum);
  cJSON_AddItemToArray(items, si);

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "target", ip);
  cJSON_AddStringToObject(data, "type", "threat_intel");
  cJSON_AddItemToObject(data, "items", items);
  return emit_result(sink, ip, 1, (abuse_ok || otx_ok) ? 90 : 60, data);
}

static int analyze_domain(const source_ctx *ctx, intel_sink *sink, const char *dom) {
  int total = 0, is_mal = 0;
  char ttypes[1024] = {0};
  int otx_ok = otx_query(ctx->http, dom, "domain", &is_mal, &total, ttypes, sizeof ttypes);

  cJSON *items = cJSON_CreateArray();
  if (otx_ok) {
    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "domain", dom);
    cJSON_AddNumberToObject(d, "pulse_count", total);
    cJSON_AddBoolToObject(d, "is_malicious", is_mal);
    if (*ttypes) cJSON_AddStringToObject(d, "threat_types", ttypes);
    cJSON *it = cJSON_CreateObject();
    cJSON_AddStringToObject(it, "source", "AlienVault OTX");
    cJSON_AddBoolToObject(it, "found", 1);
    cJSON_AddStringToObject(it, "url", "https://otx.alienvault.com/");
    cJSON_AddItemToObject(it, "data", d);
    cJSON_AddItemToArray(items, it);
  }
  cJSON *sum = cJSON_CreateObject();
  cJSON_AddStringToObject(sum, "target", dom);
  cJSON_AddStringToObject(sum, "type", "domain");
  cJSON_AddBoolToObject(sum, "is_malicious", is_mal);
  cJSON_AddNumberToObject(sum, "total_reports", total);
  cJSON_AddStringToObject(sum, "verdict",
    is_mal ? "MALICIOUS" : (total > 0 ? "SUSPICIOUS" : "CLEAN"));
  cJSON_AddStringToObject(sum, "note",
    "Additional threat intel requires VirusTotal or similar APIs");
  cJSON *si = cJSON_CreateObject();
  cJSON_AddStringToObject(si, "source", "Threat Summary");
  cJSON_AddBoolToObject(si, "found", 1);
  cJSON_AddItemToObject(si, "data", sum);
  cJSON_AddItemToArray(items, si);

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "target", dom);
  cJSON_AddStringToObject(data, "type", "threat_intel");
  cJSON_AddItemToObject(data, "items", items);
  return emit_result(sink, dom, 1, otx_ok ? 85 : 50, data);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *t = ctx->entity;
  if (!t || !*t) return -1;
  if (looks_ipv4(t)) return analyze_ip(ctx, sink, t);
  /* domain or fallback: handle_threat_intel tries IP first then domain;
   * for a non-IP entity that path always lands on the domain analyzer. */
  return analyze_domain(ctx, sink, t);
}

static const source_def threat_intel_def = {
  .id = "THREAT_INTEL", .collector = "osint",
  .name = "Threat Intel", .name_ja = "脅威インテリジェンス",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(threat_intel_def)
