/* collectors/osint/sources/threat_intel.c
 * OSINT service — port of OSINTsaas osint_tools/threat_intel.c
 * (threat_intel_check → handle_threat_intel). Canonical SERVICE = THREAT_INTEL
 * (dispatcher row {SERVICE_THREAT_INTEL, handle_threat_intel, "THREAT_INTEL",
 * true}; THREAT_FEED_LOOKUP maps to a different handler/source). On-demand
 * (interval 0); ctx->entity = an IP or domain (handle_threat_intel: IP →
 * threat_intel_ip, domain → threat_intel_domain, else try IP then domain).
 * Upstream: AbuseIPDB (api.abuseipdb.com, requires ABUSEIPDB_API_KEY — skipped
 * with no fabricated row when absent) and AlienVault OTX (otx.alienvault.com,
 * no key).
 *
 * PER-RECORD EMIT: emits ONE osint_service_result row per OTX pulse associated
 * with the indicator (remote_key "otx:<pulse_id>") and, if the AbuseIPDB key is
 * present, ONE row for the AbuseIPDB report (remote_key "abuseipdb:<ip>"). body
 * = that item's real fetched data. AbuseIPDB key absent → skip it. Nothing
 * found → emit nothing, return 0 (honest empty). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
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

/* Emit one intel row. Returns 1 if emitted. */
static int emit_item(intel_sink *sink, const char *entity, const char *feed_source,
                     const char *remote_key, const char *title,
                     const char *summary, const char *published_at,
                     cJSON *data /*owned by caller*/) {
  char *bj = cJSON_PrintUnformatted(data);
  if (!bj) return 0;

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "THREAT_INTEL");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddStringToObject(props, "feed_source", feed_source);
  char *pj = cJSON_PrintUnformatted(props);

  intel_item it = {0};
  it.remote_key      = remote_key;
  it.title           = title;
  it.body            = bj;
  it.summary         = summary;
  it.published_at    = published_at;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"THREAT_INTEL\"]";
  int rc = sink->emit(sink, &it);

  free(bj);
  free(pj);
  cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* OTX general indicator: one row per associated pulse. */
static int otx_query(http_client *http, const char *target, const char *itype,
                     intel_sink *sink) {
  char url[512];
  snprintf(url, sizeof url,
    "https://otx.alienvault.com/api/v1/indicators/%s/%s/general", itype, target);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.status == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  if (!j) return 0;

  int emitted = 0;
  const cJSON *pi = cJSON_GetObjectItem(j, "pulse_info");
  const cJSON *pulses = pi ? cJSON_GetObjectItem(pi, "pulses") : NULL;
  if (pulses && cJSON_IsArray(pulses)) {
    int pn = cJSON_GetArraySize(pulses), mx = pn > 10 ? 10 : pn;
    for (int i = 0; i < mx; i++) {
      const cJSON *p = cJSON_GetArrayItem(pulses, i);
      if (!p) continue;
      const cJSON *id = cJSON_GetObjectItem(p, "id");
      const cJSON *nm = cJSON_GetObjectItem(p, "name");
      const cJSON *de = cJSON_GetObjectItem(p, "description");
      const cJSON *tg = cJSON_GetObjectItem(p, "tags");
      const cJSON *cr = cJSON_GetObjectItem(p, "created");
      const cJSON *ad = cJSON_GetObjectItem(p, "adversary");
      const cJSON *mf = cJSON_GetObjectItem(p, "malware_families");

      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "source", "AlienVault OTX");
      cJSON_AddStringToObject(d, "indicator", target);
      cJSON_AddStringToObject(d, "indicator_type", itype);
      if (nm && cJSON_IsString(nm)) cJSON_AddStringToObject(d, "name", nm->valuestring);
      if (de && cJSON_IsString(de)) {
        char b[256]; strncpy(b, de->valuestring, 255); b[255] = 0;
        cJSON_AddStringToObject(d, "description", b);
      }
      if (cr && cJSON_IsString(cr)) cJSON_AddStringToObject(d, "date", cr->valuestring);
      if (ad && cJSON_IsString(ad) && ad->valuestring[0])
        cJSON_AddStringToObject(d, "threat_actor", ad->valuestring);
      if (tg && cJSON_IsArray(tg) && cJSON_GetArraySize(tg) > 0)
        cJSON_AddItemToObject(d, "tags", cJSON_Duplicate(tg, 1));
      if (mf && cJSON_IsArray(mf) && cJSON_GetArraySize(mf) > 0)
        cJSON_AddItemToObject(d, "malware_families", cJSON_Duplicate(mf, 1));
      cJSON_AddBoolToObject(d, "is_malicious", 1);

      char rk[300];
      if (id && cJSON_IsString(id)) snprintf(rk, sizeof rk, "otx:%s", id->valuestring);
      else snprintf(rk, sizeof rk, "otx:%s:%d", target, i);
      char title[320];
      snprintf(title, sizeof title, "OTX pulse — %s",
               (nm && cJSON_IsString(nm)) ? nm->valuestring : target);
      const char *pub = (cr && cJSON_IsString(cr)) ? cr->valuestring : NULL;

      emitted += emit_item(sink, target, "AlienVault OTX", rk, title,
                           "OTX threat pulse", pub, d);
      cJSON_Delete(d);
    }
  }
  cJSON_Delete(j);
  return emitted;
}

/* AbuseIPDB: one row for the IP report. Key-gated — no key → skip (return 0). */
static int abuseipdb_query(http_client *http, const char *ip, intel_sink *sink) {
  const char *key = getenv("ABUSEIPDB_API_KEY");
  if (!key || !*key) return 0;   /* degrade gracefully w/o key, no fabricated row */
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
  const cJSON *dd = cJSON_GetObjectItem(j, "data");
  if (!dd) { cJSON_Delete(j); return 0; }

  const cJSON *sc = cJSON_GetObjectItem(dd, "abuseConfidenceScore");
  const cJSON *cc = cJSON_GetObjectItem(dd, "countryCode");
  const cJSON *is = cJSON_GetObjectItem(dd, "isp");
  const cJSON *tr = cJSON_GetObjectItem(dd, "totalReports");
  const cJSON *it = cJSON_GetObjectItem(dd, "isTor");
  const cJSON *lr = cJSON_GetObjectItem(dd, "lastReportedAt");
  int score = (sc && cJSON_IsNumber(sc)) ? (int)sc->valuedouble : 0;

  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "source", "AbuseIPDB");
  cJSON_AddStringToObject(d, "ip", ip);
  cJSON_AddNumberToObject(d, "abuse_confidence_score", score);
  if (tr && cJSON_IsNumber(tr)) cJSON_AddNumberToObject(d, "total_reports", (int)tr->valuedouble);
  if (it && cJSON_IsBool(it)) cJSON_AddBoolToObject(d, "is_tor_exit", cJSON_IsTrue(it));
  if (cc && cJSON_IsString(cc)) cJSON_AddStringToObject(d, "country", cc->valuestring);
  if (is && cJSON_IsString(is)) cJSON_AddStringToObject(d, "isp", is->valuestring);
  if (lr && cJSON_IsString(lr)) cJSON_AddStringToObject(d, "last_reported", lr->valuestring);
  cJSON_AddBoolToObject(d, "is_malicious", score > 50);

  char rk[300]; snprintf(rk, sizeof rk, "abuseipdb:%s", ip);
  char title[320]; snprintf(title, sizeof title, "AbuseIPDB report — %s (score %d)", ip, score);
  const char *pub = (lr && cJSON_IsString(lr)) ? lr->valuestring : NULL;

  int emitted = emit_item(sink, ip, "AbuseIPDB", rk, title,
                          "AbuseIPDB reputation report", pub, d);
  cJSON_Delete(d);
  cJSON_Delete(j);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *t = ctx->entity;
  if (!t || !*t) return -1;

  int emitted = 0;
  if (looks_ipv4(t)) {
    emitted += abuseipdb_query(ctx->http, t, sink);
    emitted += otx_query(ctx->http, t, "IPv4", sink);
  } else {
    /* domain or fallback: handle_threat_intel lands on the domain analyzer for
     * a non-IP entity; only OTX is available there (AbuseIPDB is IP-only). */
    emitted += otx_query(ctx->http, t, "domain", sink);
  }

  (void)emitted;            /* nothing found → emit nothing, honest empty */
  return 0;
}

static const source_def threat_intel_def = {
  .id = "THREAT_INTEL", .collector = "osint",
  .name = "Threat Intel", .name_ja = "脅威インテリジェンス",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/threat-intel",
  .description = "Threat reputation for an IP or domain (AbuseIPDB).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(threat_intel_def)
