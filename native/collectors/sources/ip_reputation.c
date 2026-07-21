/* collectors/osint/sources/ip_reputation.c
 * OSINT service — IP_REPUTATION. On-demand (interval 0); ctx->entity = an IP.
 * Sources: AbuseIPDB (key-gated ABUSEIPDB_API_KEY) + IPQualityScore (key-gated
 * IPQS_API_KEY). Each query function returns NULL without its key.
 *
 * PER-RECORD EMIT: emit ONE item per source verdict that returned real data
 * (remote_key="iprep:<source>:<ip>", body=real score/country/reports/etc.).
 * If NO key is set, no source returns data → emit NOTHING (a verdict computed
 * over zero signals would be fabricated). Private/invalid IP → emit NOTHING.
 * Never fabricate. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>

static const char *HIGH_RISK_COUNTRIES[] = {
  "CN","RU","KP","IR","SY","VE","BY","MM", NULL
};

static int is_high_risk_country(const char *cc) {
  if (!cc) return 0;
  for (int i = 0; HIGH_RISK_COUNTRIES[i]; i++)
    if (strcasecmp(cc, HIGH_RISK_COUNTRIES[i]) == 0) return 1;
  return 0;
}

static int is_private_ip(const char *ip) {
  struct in_addr a;
  if (inet_pton(AF_INET, ip, &a) != 1) return 0;
  uint32_t n = ntohl(a.s_addr);
  if ((n & 0xFF000000) == 0x0A000000) return 1;
  if ((n & 0xFFF00000) == 0xAC100000) return 1;
  if ((n & 0xFFFF0000) == 0xC0A80000) return 1;
  if ((n & 0xFF000000) == 0x7F000000) return 1;
  return 0;
}

/* query_abuseipdb: key-gated; returns NULL without ABUSEIPDB_API_KEY or on any
 * transport/parse failure (no real data → caller emits nothing). */
static cJSON *query_abuseipdb(http_client *http, const char *ip) {
  const char *api_key = getenv("ABUSEIPDB_API_KEY");
  if (!api_key || !*api_key) return NULL;

  char url[512];
  snprintf(url, sizeof url,
    "https://api.abuseipdb.com/api/v2/check?ipAddress=%s&maxAgeInDays=90&verbose",
    ip);
  char keyhdr[256];
  snprintf(keyhdr, sizeof keyhdr, "Key: %s", api_key);
  const char *hdr[3] = { keyhdr, "Accept: application/json", NULL };
  http_response hr = {0};
  int hc = http_request(http, "GET", url, hdr, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json) return NULL;
  cJSON *data = cJSON_GetObjectItem(json, "data");
  if (!data) { cJSON_Delete(json); return NULL; }

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "AbuseIPDB");
  cJSON_AddStringToObject(result, "ip", ip);
  cJSON *v;
  cJSON *sc = cJSON_GetObjectItem(data, "abuseConfidenceScore");
  if (sc && cJSON_IsNumber(sc)) {
    cJSON_AddNumberToObject(result, "abuse_confidence", sc->valuedouble);
    const char *risk;
    if (sc->valuedouble >= 75) risk = "HIGH";
    else if (sc->valuedouble >= 50) risk = "MEDIUM";
    else if (sc->valuedouble >= 25) risk = "LOW";
    else risk = "MINIMAL";
    cJSON_AddStringToObject(result, "risk_level", risk);
  }
  if ((v = cJSON_GetObjectItem(data, "totalReports")) && cJSON_IsNumber(v))
    cJSON_AddNumberToObject(result, "total_reports", v->valuedouble);
  if ((v = cJSON_GetObjectItem(data, "numDistinctUsers")) && cJSON_IsNumber(v))
    cJSON_AddNumberToObject(result, "distinct_reporters", v->valuedouble);
  cJSON *cc = cJSON_GetObjectItem(data, "countryCode");
  if (cc && cJSON_IsString(cc)) {
    cJSON_AddStringToObject(result, "country", cc->valuestring);
    cJSON_AddBoolToObject(result, "high_risk_country",
      is_high_risk_country(cc->valuestring));
  }
  if ((v = cJSON_GetObjectItem(data, "isp")) && cJSON_IsString(v))
    cJSON_AddStringToObject(result, "isp", v->valuestring);
  if ((v = cJSON_GetObjectItem(data, "domain")) && cJSON_IsString(v))
    cJSON_AddStringToObject(result, "domain", v->valuestring);
  if ((v = cJSON_GetObjectItem(data, "usageType")) && cJSON_IsString(v))
    cJSON_AddStringToObject(result, "usage_type", v->valuestring);
  if ((v = cJSON_GetObjectItem(data, "isTor")) && cJSON_IsBool(v))
    cJSON_AddBoolToObject(result, "is_tor", cJSON_IsTrue(v));
  if ((v = cJSON_GetObjectItem(data, "isWhitelisted")) && cJSON_IsBool(v))
    cJSON_AddBoolToObject(result, "is_whitelisted", cJSON_IsTrue(v));
  if ((v = cJSON_GetObjectItem(data, "lastReportedAt")) && cJSON_IsString(v))
    cJSON_AddStringToObject(result, "last_reported", v->valuestring);
  cJSON *ra = cJSON_GetObjectItem(data, "reports");
  if (ra && cJSON_IsArray(ra)) {
    cJSON *cats = cJSON_CreateObject();
    int rc2 = cJSON_GetArraySize(ra);
    for (int i = 0; i < rc2 && i < 100; i++) {
      cJSON *rep = cJSON_GetArrayItem(ra, i);
      cJSON *cs = cJSON_GetObjectItem(rep, "categories");
      if (cs && cJSON_IsArray(cs)) {
        int cn = cJSON_GetArraySize(cs);
        for (int j = 0; j < cn; j++) {
          cJSON *cat = cJSON_GetArrayItem(cs, j);
          if (cat && cJSON_IsNumber(cat)) {
            char k[16];
            snprintf(k, sizeof k, "cat_%d", (int)cat->valuedouble);
            cJSON *ex = cJSON_GetObjectItem(cats, k);
            if (ex) cJSON_SetNumberValue(ex, ex->valuedouble + 1);
            else cJSON_AddNumberToObject(cats, k, 1);
          }
        }
      }
    }
    cJSON_AddItemToObject(result, "abuse_categories", cats);
  }
  cJSON_Delete(json);
  return result;
}

/* query_ipqs: key-gated on IPQS_API_KEY; NULL without key or on failure. */
static cJSON *query_ipqs(http_client *http, const char *ip) {
  const char *api_key = getenv("IPQS_API_KEY");
  if (!api_key || !*api_key) return NULL;

  char url[512];
  snprintf(url, sizeof url,
    "https://ipqualityscore.com/api/json/ip/%s/%s", api_key, ip);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json) return NULL;
  cJSON *ok = cJSON_GetObjectItem(json, "success");
  if (!ok || !cJSON_IsTrue(ok)) { cJSON_Delete(json); return NULL; }

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "IPQualityScore");
  cJSON_AddStringToObject(result, "ip", ip);
  cJSON *v;
  if ((v = cJSON_GetObjectItem(json, "fraud_score")) && cJSON_IsNumber(v))
    cJSON_AddNumberToObject(result, "fraud_score", v->valuedouble);
  if ((v = cJSON_GetObjectItem(json, "proxy")) && cJSON_IsBool(v))
    cJSON_AddBoolToObject(result, "is_proxy", cJSON_IsTrue(v));
  if ((v = cJSON_GetObjectItem(json, "vpn")) && cJSON_IsBool(v))
    cJSON_AddBoolToObject(result, "is_vpn", cJSON_IsTrue(v));
  if ((v = cJSON_GetObjectItem(json, "tor")) && cJSON_IsBool(v))
    cJSON_AddBoolToObject(result, "is_tor", cJSON_IsTrue(v));
  if ((v = cJSON_GetObjectItem(json, "bot_status")) && cJSON_IsBool(v))
    cJSON_AddBoolToObject(result, "is_bot", cJSON_IsTrue(v));
  if ((v = cJSON_GetObjectItem(json, "recent_abuse")) && cJSON_IsBool(v))
    cJSON_AddBoolToObject(result, "recent_abuse", cJSON_IsTrue(v));
  if ((v = cJSON_GetObjectItem(json, "ISP")) && cJSON_IsString(v))
    cJSON_AddStringToObject(result, "isp", v->valuestring);
  if ((v = cJSON_GetObjectItem(json, "organization")) && cJSON_IsString(v))
    cJSON_AddStringToObject(result, "organization", v->valuestring);
  if ((v = cJSON_GetObjectItem(json, "ASN")) && cJSON_IsNumber(v))
    cJSON_AddNumberToObject(result, "asn", v->valuedouble);
  if ((v = cJSON_GetObjectItem(json, "country_code")) && cJSON_IsString(v))
    cJSON_AddStringToObject(result, "country", v->valuestring);
  if ((v = cJSON_GetObjectItem(json, "city")) && cJSON_IsString(v))
    cJSON_AddStringToObject(result, "city", v->valuestring);
  if ((v = cJSON_GetObjectItem(json, "mobile")) && cJSON_IsBool(v))
    cJSON_AddBoolToObject(result, "is_mobile", cJSON_IsTrue(v));
  if ((v = cJSON_GetObjectItem(json, "host")) && cJSON_IsString(v))
    cJSON_AddStringToObject(result, "hostname", v->valuestring);
  cJSON_Delete(json);
  return result;
}

/* Emit one per-source verdict item. Returns 1 if emitted. Frees `data`. */
static int emit_source(intel_sink *sink, const char *ip, const char *source,
                       cJSON *data /*owned*/) {
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "IP_REPUTATION");
  cJSON_AddStringToObject(props, "ip", ip);
  cJSON_AddStringToObject(props, "rep_source", source);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300], title[320];
  snprintf(rk, sizeof rk, "iprep:%s:%s", source, ip);
  snprintf(title, sizeof title, "%s — %s", source, ip);

  /* summary = the most data-bearing verdict field available. */
  const char *summary = "IP reputation";
  cJSON *rl = cJSON_GetObjectItem(data, "risk_level");
  cJSON *fs = cJSON_GetObjectItem(data, "fraud_score");
  static char sbuf[64];
  if (rl && cJSON_IsString(rl)) summary = rl->valuestring;
  else if (fs && cJSON_IsNumber(fs)) {
    snprintf(sbuf, sizeof sbuf, "fraud score %d", (int)fs->valuedouble);
    summary = sbuf;
  }

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = summary;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"IP_REPUTATION\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(props);
  cJSON_Delete(data);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *ip = ctx->entity;
  if (!ip || !*ip) return 0;

  struct in_addr a4;
  struct in6_addr a6;
  int is4 = (inet_pton(AF_INET, ip, &a4) == 1);
  int is6 = (inet_pton(AF_INET6, ip, &a6) == 1);

  if (!is4 && !is6) return 0;              /* invalid IP → emit nothing */
  if (is4 && is_private_ip(ip)) return 0;  /* private IP → emit nothing */

  cJSON *ab = query_abuseipdb(ctx->http, ip);
  if (ab) emit_source(sink, ip, "AbuseIPDB", ab);
  cJSON *iq = query_ipqs(ctx->http, ip);
  if (iq) emit_source(sink, ip, "IPQualityScore", iq);

  return 0;   /* no key / no signals → nothing emitted (honest empty) */
}

static const source_def ip_reputation_def = {
  .id = "IP_REPUTATION", .collector = "osint",
  .name = "IP Reputation", .name_ja = "IP評価",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/ip-reputation",
  .description = "Score IP reputation via AbuseIPDB and IPQualityScore.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(ip_reputation_def)
