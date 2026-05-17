/* collectors/osint/sources/ip_reputation.c
 * OSINT service — faithful port of OSINTsaas osint_tools/ip_reputation.c
 * (handle_ip_reputation → ip_reputation_check). Canonical service IP_REPUTATION
 * (osint_dispatcher.c service_registry[]). On-demand (interval 0); ctx->entity
 * = an IP. Sources: AbuseIPDB (key-gated ABUSEIPDB_API_KEY) + IPQualityScore
 * (key-gated IPQS_API_KEY). OSINTsaas query_abuseipdb/query_ipqs return NULL
 * without the key; reproduced faithfully (incl. the original behaviour of
 * issuing the AbuseIPDB GET WITHOUT the Key: header — the upstream builds
 * auth_header but never sends it). Reproduces is_private_ip, calculate_
 * reputation scoring/verdict/factors, recommendations, and the result_builder
 * output as the documented result_builder JSON shape inline (no
 * result_builder.c in this build). Invalid IP → success=false conf 0; private
 * IPv4 → success=true conf 100; else success=true conf=reputation score.
 * Emits ONE osint_service_result row; body = {success,confidence,data,error?}
 * envelope, like ip_geolocation.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
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

/* query_abuseipdb: key-gated; OSINTsaas issues a plain GET (no Key header). */
static cJSON *query_abuseipdb(http_client *http, const char *ip) {
  const char *api_key = getenv("ABUSEIPDB_API_KEY");
  if (!api_key || !*api_key) return NULL;

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "AbuseIPDB");
  char url[512];
  snprintf(url, sizeof url,
    "https://api.abuseipdb.com/api/v2/check?ipAddress=%s&maxAgeInDays=90&verbose",
    ip);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    http_response_free(&hr);
    cJSON_AddStringToObject(result, "error", "API request failed");
    return result;
  }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json) { cJSON_AddStringToObject(result, "error", "Invalid JSON response"); return result; }
  cJSON *data = cJSON_GetObjectItem(json, "data");
  if (data) {
    cJSON *v;
    if ((v = cJSON_GetObjectItem(data, "ipAddress")) && cJSON_IsString(v))
      cJSON_AddStringToObject(result, "ip", v->valuestring);
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
  }
  cJSON_Delete(json);
  return result;
}

/* query_ipqs: key-gated on IPQS_API_KEY. */
static cJSON *query_ipqs(http_client *http, const char *ip) {
  const char *api_key = getenv("IPQS_API_KEY");
  if (!api_key || !*api_key) return NULL;

  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "IPQualityScore");
  char url[512];
  snprintf(url, sizeof url,
    "https://ipqualityscore.com/api/json/ip/%s/%s", api_key, ip);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) {
    http_response_free(&hr);
    cJSON_AddStringToObject(result, "error", "API request failed");
    return result;
  }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!json) { cJSON_AddStringToObject(result, "error", "Invalid JSON response"); return result; }
  cJSON *ok = cJSON_GetObjectItem(json, "success");
  if (!ok || !cJSON_IsTrue(ok)) {
    cJSON_AddStringToObject(result, "error", "API returned failure");
    cJSON_Delete(json);
    return result;
  }
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

/* calculate_reputation: verbatim scoring/verdict/factors. */
static cJSON *calculate_reputation(cJSON *ab, cJSON *iq, const char *ip) {
  cJSON *rep = cJSON_CreateObject();
  int total = 100;
  int risk_factors = 0;
  cJSON *factors = cJSON_CreateArray();

  if (is_private_ip(ip)) {
    cJSON_AddStringToObject(rep, "ip_type", "private");
    cJSON_AddNumberToObject(rep, "score", 100);
    cJSON_AddStringToObject(rep, "verdict", "PRIVATE_IP");
    cJSON_Delete(factors);
    return rep;
  }
  cJSON_AddStringToObject(rep, "ip_type", "public");

  if (ab) {
    cJSON *ac = cJSON_GetObjectItem(ab, "abuse_confidence");
    if (ac && cJSON_IsNumber(ac)) {
      int conf = (int)ac->valuedouble;
      if (conf > 0) {
        total -= conf / 2;
        char f[128];
        snprintf(f, sizeof f, "AbuseIPDB confidence: %d%%", conf);
        cJSON_AddItemToArray(factors, cJSON_CreateString(f));
        risk_factors++;
      }
    }
    cJSON *t = cJSON_GetObjectItem(ab, "is_tor");
    if (t && cJSON_IsTrue(t)) {
      total -= 20;
      cJSON_AddItemToArray(factors, cJSON_CreateString("Tor exit node"));
      risk_factors++;
    }
    cJSON *hr = cJSON_GetObjectItem(ab, "high_risk_country");
    if (hr && cJSON_IsTrue(hr)) {
      total -= 10;
      cJSON_AddItemToArray(factors, cJSON_CreateString("High-risk country"));
      risk_factors++;
    }
    cJSON *rp = cJSON_GetObjectItem(ab, "total_reports");
    if (rp && cJSON_IsNumber(rp) && rp->valuedouble > 10) {
      total -= 15;
      cJSON_AddItemToArray(factors, cJSON_CreateString("Multiple abuse reports"));
      risk_factors++;
    }
  }
  if (iq) {
    cJSON *fs = cJSON_GetObjectItem(iq, "fraud_score");
    if (fs && cJSON_IsNumber(fs)) {
      int fr = (int)fs->valuedouble;
      if (fr > 50) {
        total -= fr / 3;
        char f[128];
        snprintf(f, sizeof f, "Fraud score: %d", fr);
        cJSON_AddItemToArray(factors, cJSON_CreateString(f));
        risk_factors++;
      }
    }
    cJSON *vp = cJSON_GetObjectItem(iq, "is_vpn");
    if (vp && cJSON_IsTrue(vp)) {
      total -= 10;
      cJSON_AddItemToArray(factors, cJSON_CreateString("VPN detected"));
      risk_factors++;
    }
    cJSON *px = cJSON_GetObjectItem(iq, "is_proxy");
    if (px && cJSON_IsTrue(px)) {
      total -= 15;
      cJSON_AddItemToArray(factors, cJSON_CreateString("Proxy detected"));
      risk_factors++;
    }
    cJSON *bt = cJSON_GetObjectItem(iq, "is_bot");
    if (bt && cJSON_IsTrue(bt)) {
      total -= 25;
      cJSON_AddItemToArray(factors, cJSON_CreateString("Bot activity detected"));
      risk_factors++;
    }
    cJSON *rab = cJSON_GetObjectItem(iq, "recent_abuse");
    if (rab && cJSON_IsTrue(rab)) {
      total -= 20;
      cJSON_AddItemToArray(factors, cJSON_CreateString("Recent abuse detected"));
      risk_factors++;
    }
  }
  if (total < 0) total = 0;
  if (total > 100) total = 100;
  cJSON_AddNumberToObject(rep, "score", total);
  cJSON_AddNumberToObject(rep, "risk_factors", risk_factors);
  cJSON_AddItemToObject(rep, "factors", factors);
  const char *verdict;
  if (total >= 80) verdict = "CLEAN";
  else if (total >= 60) verdict = "LOW_RISK";
  else if (total >= 40) verdict = "MEDIUM_RISK";
  else if (total >= 20) verdict = "HIGH_RISK";
  else verdict = "MALICIOUS";
  cJSON_AddStringToObject(rep, "verdict", verdict);
  return rep;
}

/* result_builder item: {source,found,data,confidence,detection_method,url?}. */
static void rb_add(cJSON *results, const char *source, int confidence,
                   cJSON *data /*owned*/, const char *url,
                   int *total, int *fc) {
  cJSON *it = cJSON_CreateObject();
  cJSON_AddStringToObject(it, "source", source);
  cJSON_AddBoolToObject(it, "found", 1);     /* OSINTsaas adds items as found */
  if (data) cJSON_AddItemToObject(it, "data", data);
  else cJSON_AddNullToObject(it, "data");
  cJSON_AddNumberToObject(it, "confidence", confidence);
  cJSON_AddStringToObject(it, "detection_method", "direct");
  if (url) cJSON_AddStringToObject(it, "url", url);
  cJSON_AddItemToArray(results, it);
  (*total)++; (*fc)++;
}

static cJSON *finalize(cJSON *results, int total, int fc) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query_type", "ip_reputation");
  cJSON_AddItemToObject(root, "results", results);
  cJSON *summary = cJSON_CreateObject();
  cJSON_AddNumberToObject(summary, "total_sources", total);
  cJSON_AddNumberToObject(summary, "found_count", fc);
  cJSON *bd = cJSON_CreateObject();
  cJSON_AddNumberToObject(bd, "direct", total);
  cJSON_AddNumberToObject(bd, "llama", 0);
  cJSON_AddItemToObject(summary, "detection_breakdown", bd);
  cJSON_AddItemToObject(root, "summary", summary);
  return root;
}

static int emit_one(intel_sink *sink, const char *entity, int success,
                    int confidence, cJSON *data /*owned*/) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  cJSON_AddItemToObject(env, "data", data);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "IP_REPUTATION");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "iprep:%s", entity);
  char title[320];
  snprintf(title, sizeof title, "IP_REPUTATION — %s", entity);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = success ? "IP reputation analyzed" : "invalid IP";
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"IP_REPUTATION\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *ip = ctx->entity;
  if (!ip || !*ip) return -1;

  struct in_addr a4;
  struct in6_addr a6;
  int is4 = (inet_pton(AF_INET, ip, &a4) == 1);
  int is6 = (inet_pton(AF_INET6, ip, &a6) == 1);

  cJSON *results = cJSON_CreateArray();
  int total = 0, fc = 0;

  if (!is4 && !is6) {
    cJSON *ed = cJSON_CreateObject();
    cJSON_AddStringToObject(ed, "error", "Invalid IP address format");
    rb_add(results, "Validation", 0, ed, NULL, &total, &fc);
    /* OSINTsaas marks Validation item found=false; correct the flag. */
    cJSON *it0 = cJSON_GetArrayItem(results, 0);
    cJSON_ReplaceItemInObject(it0, "found", cJSON_CreateFalse());
    fc--;
    return emit_one(sink, ip, 0, 0, finalize(results, total, fc));
  }

  if (is4 && is_private_ip(ip)) {
    cJSON *pd = cJSON_CreateObject();
    cJSON_AddStringToObject(pd, "ip_type", "private");
    cJSON_AddStringToObject(pd, "ip_version", "IPv4");
    cJSON_AddStringToObject(pd, "note", "Private IP addresses are not rated");
    rb_add(results, "Private IP", 90, pd, NULL, &total, &fc);
    return emit_one(sink, ip, 1, 100, finalize(results, total, fc));
  }

  cJSON *ab = query_abuseipdb(ctx->http, ip);
  if (ab) {
    cJSON *acj = cJSON_GetObjectItem(ab, "abuse_confidence");
    int conf = (acj && cJSON_IsNumber(acj)) ? (int)acj->valuedouble : 0;
    rb_add(results, "AbuseIPDB", conf > 50 ? 90 : 70, ab,
           "https://www.abuseipdb.com/", &total, &fc);
  }
  cJSON *iq = query_ipqs(ctx->http, ip);
  if (iq) {
    cJSON *fsj = cJSON_GetObjectItem(iq, "fraud_score");
    int sc = (fsj && cJSON_IsNumber(fsj)) ? (int)fsj->valuedouble : 0;
    rb_add(results, "IP Quality Score", sc > 50 ? 90 : 70, iq,
           "https://www.ipqualityscore.com/", &total, &fc);
  }

  /* calculate_reputation needs the source objects; rebuild lightweight
   * copies from what we placed in results (data items) — but we already
   * transferred ownership. Re-derive from the result data nodes. */
  cJSON *ab_d = NULL, *iq_d = NULL;
  int rn = cJSON_GetArraySize(results);
  for (int i = 0; i < rn; i++) {
    cJSON *itm = cJSON_GetArrayItem(results, i);
    cJSON *src = cJSON_GetObjectItem(itm, "source");
    cJSON *d = cJSON_GetObjectItem(itm, "data");
    if (src && cJSON_IsString(src)) {
      if (strcmp(src->valuestring, "AbuseIPDB") == 0) ab_d = d;
      else if (strcmp(src->valuestring, "IP Quality Score") == 0) iq_d = d;
    }
  }
  cJSON *rep = calculate_reputation(ab_d, iq_d, ip);
  int rep_score = 50;
  cJSON *sj = cJSON_GetObjectItem(rep, "score");
  if (sj && cJSON_IsNumber(sj)) rep_score = (int)sj->valuedouble;
  cJSON *verdict = cJSON_GetObjectItem(rep, "verdict");
  char vbuf[32] = {0};
  if (verdict && cJSON_IsString(verdict))
    strncpy(vbuf, verdict->valuestring, sizeof vbuf - 1);
  rb_add(results, "Reputation Analysis", 90, rep, NULL, &total, &fc);

  if (vbuf[0]) {
    cJSON *recs = cJSON_CreateArray();
    if (strcmp(vbuf, "MALICIOUS") == 0 || strcmp(vbuf, "HIGH_RISK") == 0) {
      cJSON_AddItemToArray(recs, cJSON_CreateString("Block this IP at firewall level"));
      cJSON_AddItemToArray(recs, cJSON_CreateString("Review any traffic from this IP"));
      cJSON_AddItemToArray(recs, cJSON_CreateString("Report abuse to ISP if warranted"));
    } else if (strcmp(vbuf, "MEDIUM_RISK") == 0) {
      cJSON_AddItemToArray(recs, cJSON_CreateString("Monitor traffic from this IP"));
      cJSON_AddItemToArray(recs, cJSON_CreateString("Consider rate limiting"));
    }
    if (cJSON_GetArraySize(recs) > 0) {
      cJSON *rd = cJSON_CreateObject();
      cJSON_AddItemToObject(rd, "recommendations", recs);
      rb_add(results, "Security Recommendations", 90, rd, NULL, &total, &fc);
    } else {
      cJSON_Delete(recs);
    }
  }

  return emit_one(sink, ip, 1, rep_score, finalize(results, total, fc));
}

static const source_def ip_reputation_def = {
  .id = "IP_REPUTATION", .collector = "osint",
  .name = "IP Reputation", .name_ja = "IP評価",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(ip_reputation_def)
