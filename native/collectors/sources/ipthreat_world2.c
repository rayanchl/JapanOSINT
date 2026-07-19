/* collectors/sources/ipthreat_world2.c
 * OSINT services — key-gated IP/indicator reputation, pivot on ctx->entity (an
 * IP or indicator). Honest-empty when the key is unset.
 *
 *   ABUSEIPDB_CHECK      api.abuseipdb.com/api/v2/check?ipAddress=  ABUSEIPDB_API_KEY
 *   IPQUALITYSCORE_IP    ipqualityscore.com/api/json/ip/<KEY>/<ip>  IPQUALITYSCORE_KEY
 *   PULSEDIVE_INDICATOR  pulsedive.com/api/info.php?indicator=      PULSEDIVE_API_KEY */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *IT_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

static int run_abuseipdb(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("ABUSEIPDB_API_KEY");
  if (!k) { fprintf(stderr, "[ABUSEIPDB_CHECK] no ABUSEIPDB_API_KEY\n"); return 0; }
  char url[512];
  snprintf(url, sizeof url,
    "https://api.abuseipdb.com/api/v2/check?ipAddress=%s&maxAgeInDays=90", enc);
  char keyh[160]; snprintf(keyh, sizeof keyh, "Key: %s", k);
  const char *hdrs[] = { "Accept: application/json", IT_UA, keyh, NULL };
  char *body = jo_get(ctx, url, hdrs, "ABUSEIPDB_CHECK");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *data = cJSON_GetObjectItem(root, "data");
  const char *ip = data ? jo_sv(data, "ipAddress") : NULL;
  if (!ip) { cJSON_Delete(root); return 0; }
  const cJSON *score = cJSON_GetObjectItem(data, "abuseConfidenceScore");
  const cJSON *reports = cJSON_GetObjectItem(data, "totalReports");
  const char *cc = jo_sv(data, "countryCode");
  const char *isp = jo_sv(data, "isp");
  const char *domain = jo_sv(data, "domain");
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "ip", ip);
  if (score && cJSON_IsNumber(score)) cJSON_AddNumberToObject(d, "abuse_confidence", score->valuedouble);
  if (reports && cJSON_IsNumber(reports)) cJSON_AddNumberToObject(d, "total_reports", reports->valuedouble);
  if (cc) cJSON_AddStringToObject(d, "country", cc);
  if (isp) cJSON_AddStringToObject(d, "isp", isp);
  if (domain) cJSON_AddStringToObject(d, "domain", domain);
  cJSON_AddStringToObject(d, "source", "AbuseIPDB");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "ABUSEIPDB_CHECK"); cJSON_AddStringToObject(p, "ip", ip);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char link[256], title[200];
  snprintf(link, sizeof link, "https://www.abuseipdb.com/check/%s", ip);
  snprintf(title, sizeof title, "%s — abuse score %.0f", ip, (score && cJSON_IsNumber(score)) ? score->valuedouble : 0);
  intel_item it = {0};
  it.remote_key = ip; it.title = title; it.summary = isp; it.body = bj; it.link = link; it.lang = "en";
  it.record_type = "ip-reputation";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"ABUSEIPDB_CHECK\"]";
  int rc = sink->emit(sink, &it); free(bj); free(pj); cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run_ipqs(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("IPQUALITYSCORE_KEY");
  if (!k) { fprintf(stderr, "[IPQUALITYSCORE_IP] no IPQUALITYSCORE_KEY\n"); return 0; }
  char url[512];
  snprintf(url, sizeof url, "https://ipqualityscore.com/api/json/ip/%s/%s", k, enc);
  const char *hdrs[] = { "Accept: application/json", IT_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "IPQUALITYSCORE_IP");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *success = cJSON_GetObjectItem(root, "success");
  if (!cJSON_IsTrue(success)) { cJSON_Delete(root); return 0; }
  const cJSON *fraud = cJSON_GetObjectItem(root, "fraud_score");
  const char *isp = jo_sv(root, "ISP");
  const char *cc = jo_sv(root, "country_code");
  const cJSON *proxy = cJSON_GetObjectItem(root, "proxy");
  const cJSON *vpn = cJSON_GetObjectItem(root, "vpn");
  const cJSON *tor = cJSON_GetObjectItem(root, "tor");
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "ip", ctx->entity);
  if (fraud && cJSON_IsNumber(fraud)) cJSON_AddNumberToObject(d, "fraud_score", fraud->valuedouble);
  if (isp) cJSON_AddStringToObject(d, "isp", isp);
  if (cc) cJSON_AddStringToObject(d, "country", cc);
  if (cJSON_IsBool(proxy)) cJSON_AddBoolToObject(d, "proxy", cJSON_IsTrue(proxy));
  if (cJSON_IsBool(vpn)) cJSON_AddBoolToObject(d, "vpn", cJSON_IsTrue(vpn));
  if (cJSON_IsBool(tor)) cJSON_AddBoolToObject(d, "tor", cJSON_IsTrue(tor));
  cJSON_AddStringToObject(d, "source", "IPQualityScore");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "IPQUALITYSCORE_IP"); cJSON_AddStringToObject(p, "ip", ctx->entity);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char title[200];
  snprintf(title, sizeof title, "%s — fraud %.0f", ctx->entity, (fraud && cJSON_IsNumber(fraud)) ? fraud->valuedouble : 0);
  intel_item it = {0};
  it.remote_key = ctx->entity; it.title = title; it.summary = isp; it.body = bj; it.lang = "en";
  it.record_type = "ip-reputation";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"IPQUALITYSCORE_IP\"]";
  int rc = sink->emit(sink, &it); free(bj); free(pj); cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run_pulsedive(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  const char *k = jo_env("PULSEDIVE_API_KEY");
  if (!k) { fprintf(stderr, "[PULSEDIVE_INDICATOR] no PULSEDIVE_API_KEY\n"); return 0; }
  char url[512];
  snprintf(url, sizeof url, "https://pulsedive.com/api/info.php?indicator=%s&pretty=1&key=%s", enc, k);
  const char *hdrs[] = { "Accept: application/json", IT_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "PULSEDIVE_INDICATOR");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *indicator = jo_sv(root, "indicator");
  if (!indicator) { cJSON_Delete(root); return 0; }
  const char *type = jo_sv(root, "type");
  const char *risk = jo_sv(root, "risk");
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "indicator", indicator);
  if (type) cJSON_AddStringToObject(d, "type", type);
  if (risk) cJSON_AddStringToObject(d, "risk", risk);
  cJSON_AddStringToObject(d, "source", "Pulsedive");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "PULSEDIVE_INDICATOR");
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char link[256], title[200];
  const char *iid = jo_sv(root, "iid");
  if (iid) snprintf(link, sizeof link, "https://pulsedive.com/indicator/?iid=%s", iid); else link[0] = 0;
  snprintf(title, sizeof title, "%s — risk %s", indicator, risk ? risk : "unknown");
  intel_item it = {0};
  it.remote_key = indicator; it.title = title; it.summary = risk; it.body = bj;
  it.link = link[0] ? link : NULL; it.lang = "en";
  it.record_type = "threat-indicator";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"PULSEDIVE_INDICATOR\"]";
  int rc = sink->emit(sink, &it); free(bj); free(pj); cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "ABUSEIPDB_CHECK"))     n = run_abuseipdb(ctx, sink, enc);
  else if (!strcmp(sid, "IPQUALITYSCORE_IP"))   n = run_ipqs(ctx, sink, enc);
  else if (!strcmp(sid, "PULSEDIVE_INDICATOR")) n = run_pulsedive(ctx, sink, enc);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define IT_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "cyber", \
    .type = "api", .url = "internal://osint/ipthreat", .description = DESC, \
    .layer = NULL, .free_tier = 0 }; \
  REGISTER_SOURCE(sym)

IT_DEF(it_abuseipdb_def, "ABUSEIPDB_CHECK", "AbuseIPDB Check",
  "AbuseIPDB (ABUSEIPDB_API_KEY): IP abuse confidence, reports, ISP, country.");
IT_DEF(it_ipqs_def, "IPQUALITYSCORE_IP", "IPQualityScore IP",
  "IPQualityScore (IPQUALITYSCORE_KEY): IP fraud score, proxy/vpn/tor flags.");
IT_DEF(it_pulsedive_def, "PULSEDIVE_INDICATOR", "Pulsedive Indicator",
  "Pulsedive (PULSEDIVE_API_KEY): risk/type for a domain/IP/URL indicator.");
