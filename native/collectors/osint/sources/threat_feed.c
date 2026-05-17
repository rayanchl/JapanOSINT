/* collectors/osint/sources/threat_feed.c
 * OSINT service — port of OSINTsaas osint_tools/threat_feed.c
 * (threat_feed_lookup → handle_threat_feed). Canonical SERVICE =
 * THREAT_FEED_LOOKUP (dispatcher row {SERVICE_THREAT_INTEL, handle_threat_feed,
 * "THREAT_FEED_LOOKUP", true} — distinct handler from handle_threat_intel, so
 * its own source id). On-demand (interval 0); ctx->entity = an IOC (ip / domain
 * / url / hash / email). No required key (OTX_API_KEY optional). Faithfully
 * reproduces detect_ioc_type, query_otx (X-OTX-API-KEY header if set; pulses →
 * threats[]/general_info), query_urlhaus (POST x-www-form-urlencoded, url|host),
 * query_threatfox (POST JSON {query:search_ioc,search_term}), query_circl
 * (SHA256 only), calculate_threat_score and the threat_level/summary/
 * recommendations block. confidence = 70 + sources*5 (cap 95) / 30. Emits ONE
 * osint_service_result row like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

typedef enum { IOC_IP, IOC_DOMAIN, IOC_URL, IOC_MD5, IOC_SHA1, IOC_SHA256,
               IOC_EMAIL, IOC_UNKNOWN } ioc_t;

static int all_hex(const char *s, size_t n) {
  for (size_t i = 0; i < n; i++) if (!isxdigit((unsigned char)s[i])) return 0;
  return 1;
}

static ioc_t detect_ioc(const char *s) {
  if (!s) return IOC_UNKNOWN;
  size_t len = strlen(s);
  if (len == 32 && all_hex(s, 32)) return IOC_MD5;
  if (len == 40 && all_hex(s, 40)) return IOC_SHA1;
  if (len == 64 && all_hex(s, 64)) return IOC_SHA256;
  if (strncmp(s, "http://", 7) == 0 || strncmp(s, "https://", 8) == 0) return IOC_URL;
  int octets = 0, nums = 0;
  for (size_t i = 0; i <= len; i++) {
    if (s[i] == '.' || s[i] == '\0') { if (nums > 0 && nums <= 3) octets++; nums = 0; }
    else if (isdigit((unsigned char)s[i])) nums++;
    else break;
  }
  if (octets == 4) return IOC_IP;
  if (strchr(s, '@') && strchr(s, '.')) return IOC_EMAIL;
  if (strchr(s, '.') && !strchr(s, ' ') && !strchr(s, '/')) return IOC_DOMAIN;
  return IOC_UNKNOWN;
}

static const char *ioc_str(ioc_t t) {
  switch (t) {
    case IOC_IP: return "IPv4";
    case IOC_DOMAIN: return "domain";
    case IOC_URL: return "url";
    case IOC_MD5: case IOC_SHA1: case IOC_SHA256: return "file";
    case IOC_EMAIL: return "email";
    default: return "general";
  }
}

static cJSON *query_otx(http_client *http, const char *ind, ioc_t t) {
  char url[512];
  snprintf(url, sizeof url, "https://otx.alienvault.com/api/v1/indicators/%s/%s",
           ioc_str(t), ind);
  const char *key = getenv("OTX_API_KEY");
  const char *hdrs[3]; int h = 0;
  if (key && *key) { hdrs[h++] = "X-OTX-API-KEY"; hdrs[h++] = key; }
  hdrs[h] = NULL;
  http_response hr = {0};
  int hc = http_request(http, "GET", url, h ? hdrs : NULL, NULL, 0, 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  if (!j) return NULL;
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "AlienVault OTX");
  const cJSON *pi = cJSON_GetObjectItem(j, "pulse_info");
  if (pi) {
    const cJSON *cnt = cJSON_GetObjectItem(pi, "count");
    const cJSON *pulses = cJSON_GetObjectItem(pi, "pulses");
    if (cnt && cJSON_IsNumber(cnt)) cJSON_AddNumberToObject(r, "threat_reports", cnt->valueint);
    if (pulses && cJSON_IsArray(pulses) && cJSON_GetArraySize(pulses) > 0) {
      cJSON *threats = cJSON_CreateArray();
      int pn = cJSON_GetArraySize(pulses), mx = pn > 5 ? 5 : pn;
      for (int i = 0; i < mx; i++) {
        const cJSON *p = cJSON_GetArrayItem(pulses, i);
        cJSON *th = cJSON_CreateObject();
        const cJSON *nm = cJSON_GetObjectItem(p, "name");
        const cJSON *de = cJSON_GetObjectItem(p, "description");
        const cJSON *tg = cJSON_GetObjectItem(p, "tags");
        const cJSON *cr = cJSON_GetObjectItem(p, "created");
        const cJSON *tl = cJSON_GetObjectItem(p, "tlp");
        const cJSON *ad = cJSON_GetObjectItem(p, "adversary");
        const cJSON *mf = cJSON_GetObjectItem(p, "malware_families");
        if (nm && cJSON_IsString(nm)) cJSON_AddStringToObject(th, "name", nm->valuestring);
        if (de && cJSON_IsString(de)) {
          char d[256]; strncpy(d, de->valuestring, 255); d[255] = 0;
          cJSON_AddStringToObject(th, "description", d);
        }
        if (cr && cJSON_IsString(cr)) cJSON_AddStringToObject(th, "date", cr->valuestring);
        if (tl && cJSON_IsString(tl)) cJSON_AddStringToObject(th, "tlp", tl->valuestring);
        if (ad && cJSON_IsString(ad) && ad->valuestring[0])
          cJSON_AddStringToObject(th, "threat_actor", ad->valuestring);
        if (tg && cJSON_IsArray(tg) && cJSON_GetArraySize(tg) > 0) {
          cJSON *ta = cJSON_CreateArray();
          int tn = cJSON_GetArraySize(tg), tm = tn > 5 ? 5 : tn;
          for (int k = 0; k < tm; k++) {
            const cJSON *x = cJSON_GetArrayItem(tg, k);
            if (cJSON_IsString(x)) cJSON_AddItemToArray(ta, cJSON_CreateString(x->valuestring));
          }
          cJSON_AddItemToObject(th, "tags", ta);
        }
        if (mf && cJSON_IsArray(mf) && cJSON_GetArraySize(mf) > 0) {
          cJSON *ma = cJSON_CreateArray();
          int mn = cJSON_GetArraySize(mf);
          for (int k = 0; k < mn; k++) {
            const cJSON *x = cJSON_GetArrayItem(mf, k);
            if (cJSON_IsString(x)) cJSON_AddItemToArray(ma, cJSON_CreateString(x->valuestring));
          }
          cJSON_AddItemToObject(th, "malware_families", ma);
        }
        cJSON_AddItemToArray(threats, th);
      }
      cJSON_AddItemToObject(r, "threats", threats);
    }
  }
  const cJSON *gen = cJSON_GetObjectItem(j, "general");
  if (gen) {
    cJSON *info = cJSON_CreateObject();
    const cJSON *rep = cJSON_GetObjectItem(gen, "reputation");
    const cJSON *val = cJSON_GetObjectItem(gen, "validation");
    if (rep && cJSON_IsNumber(rep)) cJSON_AddNumberToObject(info, "reputation_score", rep->valueint);
    if (val && cJSON_IsArray(val) && cJSON_GetArraySize(val) > 0)
      cJSON_AddItemToObject(info, "validation", cJSON_Duplicate(val, 1));
    cJSON_AddItemToObject(r, "general_info", info);
  }
  cJSON_Delete(j);
  return r;
}

static cJSON *query_urlhaus(http_client *http, const char *ind, ioc_t t) {
  char url[256], payload[512];
  if (t == IOC_URL) { snprintf(url, sizeof url, "https://urlhaus-api.abuse.ch/v1/url/");
                       snprintf(payload, sizeof payload, "url=%s", ind); }
  else if (t == IOC_DOMAIN) { snprintf(url, sizeof url, "https://urlhaus-api.abuse.ch/v1/host/");
                              snprintf(payload, sizeof payload, "host=%s", ind); }
  else return NULL;
  static const char *hdrs[] = { "Content-Type", "application/x-www-form-urlencoded", NULL };
  http_response hr = {0};
  int hc = http_request(http, "POST", url, hdrs, payload, strlen(payload), 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  if (!j) return NULL;
  const cJSON *qs = cJSON_GetObjectItem(j, "query_status");
  if (!qs || !cJSON_IsString(qs) || strcmp(qs->valuestring, "ok") != 0) { cJSON_Delete(j); return NULL; }
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "URLhaus");
  if (t == IOC_DOMAIN) {
    const cJSON *urls = cJSON_GetObjectItem(j, "urls");
    if (urls && cJSON_IsArray(urls)) {
      int uc = cJSON_GetArraySize(urls);
      cJSON_AddNumberToObject(r, "malicious_urls_count", uc);
      cJSON *ul = cJSON_CreateArray();
      int mx = uc > 10 ? 10 : uc;
      for (int i = 0; i < mx; i++) {
        const cJSON *u = cJSON_GetArrayItem(urls, i);
        cJSON *e = cJSON_CreateObject();
        const cJSON *ui = cJSON_GetObjectItem(u, "url");
        const cJSON *da = cJSON_GetObjectItem(u, "date_added");
        const cJSON *th = cJSON_GetObjectItem(u, "threat");
        const cJSON *us = cJSON_GetObjectItem(u, "url_status");
        if (ui && cJSON_IsString(ui)) cJSON_AddStringToObject(e, "url", ui->valuestring);
        if (da && cJSON_IsString(da)) cJSON_AddStringToObject(e, "date_added", da->valuestring);
        if (th && cJSON_IsString(th)) cJSON_AddStringToObject(e, "threat_type", th->valuestring);
        if (us && cJSON_IsString(us)) cJSON_AddStringToObject(e, "status", us->valuestring);
        cJSON_AddItemToArray(ul, e);
      }
      cJSON_AddItemToObject(r, "malicious_urls", ul);
    }
  } else {
    const cJSON *th = cJSON_GetObjectItem(j, "threat");
    const cJSON *da = cJSON_GetObjectItem(j, "date_added");
    const cJSON *us = cJSON_GetObjectItem(j, "url_status");
    const cJSON *tg = cJSON_GetObjectItem(j, "tags");
    if (th && cJSON_IsString(th)) cJSON_AddStringToObject(r, "threat_type", th->valuestring);
    if (da && cJSON_IsString(da)) cJSON_AddStringToObject(r, "date_added", da->valuestring);
    if (us && cJSON_IsString(us)) cJSON_AddStringToObject(r, "status", us->valuestring);
    if (tg && cJSON_IsArray(tg)) cJSON_AddItemToObject(r, "tags", cJSON_Duplicate(tg, 1));
  }
  cJSON_Delete(j);
  return r;
}

static cJSON *query_threatfox(http_client *http, const char *ind) {
  cJSON *req = cJSON_CreateObject();
  cJSON_AddStringToObject(req, "query", "search_ioc");
  cJSON_AddStringToObject(req, "search_term", ind);
  char *payload = cJSON_PrintUnformatted(req);
  cJSON_Delete(req);
  if (!payload) return NULL;
  static const char *hdrs[] = { "Content-Type", "application/json", NULL };
  http_response hr = {0};
  int hc = http_request(http, "POST", "https://threatfox-api.abuse.ch/api/v1/",
                         hdrs, payload, strlen(payload), 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  free(payload);
  if (!j) return NULL;
  const cJSON *qs = cJSON_GetObjectItem(j, "query_status");
  if (!qs || !cJSON_IsString(qs) || strcmp(qs->valuestring, "ok") != 0) { cJSON_Delete(j); return NULL; }
  const cJSON *data = cJSON_GetObjectItem(j, "data");
  if (!data || !cJSON_IsArray(data) || cJSON_GetArraySize(data) == 0) { cJSON_Delete(j); return NULL; }
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "ThreatFox");
  cJSON_AddNumberToObject(r, "ioc_count", cJSON_GetArraySize(data));
  cJSON *iocs = cJSON_CreateArray();
  int dc = cJSON_GetArraySize(data), mx = dc > 5 ? 5 : dc;
  for (int i = 0; i < mx; i++) {
    const cJSON *o = cJSON_GetArrayItem(data, i);
    cJSON *e = cJSON_CreateObject();
    const cJSON *it = cJSON_GetObjectItem(o, "ioc_type");
    const cJSON *tt = cJSON_GetObjectItem(o, "threat_type");
    const cJSON *mw = cJSON_GetObjectItem(o, "malware");
    const cJSON *cf = cJSON_GetObjectItem(o, "confidence_level");
    const cJSON *fs = cJSON_GetObjectItem(o, "first_seen_utc");
    const cJSON *tg = cJSON_GetObjectItem(o, "tags");
    if (it && cJSON_IsString(it)) cJSON_AddStringToObject(e, "ioc_type", it->valuestring);
    if (tt && cJSON_IsString(tt)) cJSON_AddStringToObject(e, "threat_type", tt->valuestring);
    if (mw && cJSON_IsString(mw)) cJSON_AddStringToObject(e, "malware", mw->valuestring);
    if (cf && cJSON_IsNumber(cf)) cJSON_AddNumberToObject(e, "confidence", cf->valueint);
    if (fs && cJSON_IsString(fs)) cJSON_AddStringToObject(e, "first_seen", fs->valuestring);
    if (tg && cJSON_IsArray(tg)) cJSON_AddItemToObject(e, "tags", cJSON_Duplicate(tg, 1));
    cJSON_AddItemToArray(iocs, e);
  }
  cJSON_AddItemToObject(r, "iocs", iocs);
  cJSON_Delete(j);
  return r;
}

static cJSON *query_circl(http_client *http, const char *hash) {
  if (strlen(hash) != 64) return NULL;
  char url[256];
  snprintf(url, sizeof url, "https://hashlookup.circl.lu/lookup/sha256/%s", hash);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  if (!j) return NULL;
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "source", "CIRCL hashlookup");
  const cJSON *fn = cJSON_GetObjectItem(j, "FileName");
  const cJSON *fz = cJSON_GetObjectItem(j, "FileSize");
  const cJSON *km = cJSON_GetObjectItem(j, "KnownMalicious");
  if (fn && cJSON_IsString(fn)) cJSON_AddStringToObject(r, "filename", fn->valuestring);
  if (fz && cJSON_IsString(fz)) cJSON_AddStringToObject(r, "filesize", fz->valuestring);
  if (km) cJSON_AddBoolToObject(r, "known_malicious", cJSON_IsTrue(km));
  cJSON_Delete(j);
  return r;
}

static int calc_score(cJSON *f) {
  int s = 0;
  const cJSON *otx = cJSON_GetObjectItem(f, "alienvault_otx");
  if (otx) { const cJSON *r = cJSON_GetObjectItem(otx, "threat_reports");
             if (r && cJSON_IsNumber(r)) s += r->valueint * 10; }
  const cJSON *uh = cJSON_GetObjectItem(f, "urlhaus");
  if (uh) {
    const cJSON *m = cJSON_GetObjectItem(uh, "malicious_urls_count");
    if (m && cJSON_IsNumber(m)) s += m->valueint * 15;
    const cJSON *tt = cJSON_GetObjectItem(uh, "threat_type");
    if (tt && cJSON_IsString(tt)) {
      if (strcmp(tt->valuestring, "malware_download") == 0) s += 30;
      else if (strcmp(tt->valuestring, "phishing") == 0) s += 25;
    }
  }
  const cJSON *tf = cJSON_GetObjectItem(f, "threatfox");
  if (tf) { const cJSON *c = cJSON_GetObjectItem(tf, "ioc_count");
            if (c && cJSON_IsNumber(c)) s += c->valueint * 20; }
  return s > 100 ? 100 : s;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *ind = ctx->entity;
  if (!ind || !*ind) return -1;

  ioc_t t = detect_ioc(ind);
  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "indicator", ind);
  cJSON_AddStringToObject(root, "ioc_type", ioc_str(t));

  cJSON *findings = cJSON_CreateObject();
  int sources = 0, threats = 0;

  cJSON *otx = query_otx(ctx->http, ind, t);
  if (otx) {
    cJSON_AddItemToObject(findings, "alienvault_otx", otx);
    sources++;
    const cJSON *r = cJSON_GetObjectItem(otx, "threat_reports");
    if (r && cJSON_IsNumber(r) && r->valueint > 0) threats++;
  }
  if (t == IOC_URL || t == IOC_DOMAIN) {
    cJSON *uh = query_urlhaus(ctx->http, ind, t);
    if (uh) {
      cJSON_AddItemToObject(findings, "urlhaus", uh);
      sources++;
      const cJSON *c = cJSON_GetObjectItem(uh, "malicious_urls_count");
      if (c && cJSON_IsNumber(c) && c->valueint > 0) threats++;
    }
  }
  cJSON *tf = query_threatfox(ctx->http, ind);
  if (tf) {
    cJSON_AddItemToObject(findings, "threatfox", tf);
    sources++;
    const cJSON *c = cJSON_GetObjectItem(tf, "ioc_count");
    if (c && cJSON_IsNumber(c) && c->valueint > 0) threats++;
  }
  if (t == IOC_SHA256) {
    cJSON *ci = query_circl(ctx->http, ind);
    if (ci) { cJSON_AddItemToObject(findings, "circl_hashlookup", ci); sources++; }
  }
  cJSON_AddItemToObject(root, "findings", findings);

  int score = calc_score(findings);
  cJSON_AddNumberToObject(root, "threat_score", score);
  const char *lvl = score >= 80 ? "critical" : score >= 60 ? "high"
                  : score >= 40 ? "medium" : score >= 20 ? "low" : "clean";
  cJSON_AddStringToObject(root, "threat_level", lvl);

  cJSON *sum = cJSON_CreateObject();
  cJSON_AddNumberToObject(sum, "sources_checked", sources);
  cJSON_AddNumberToObject(sum, "sources_with_threats", threats);
  cJSON_AddBoolToObject(sum, "is_malicious", score >= 40);
  cJSON *rec = cJSON_CreateArray();
  if (score >= 80) {
    cJSON_AddItemToArray(rec, cJSON_CreateString("BLOCK immediately - confirmed malicious"));
    cJSON_AddItemToArray(rec, cJSON_CreateString("Investigate systems that may have accessed this IOC"));
  } else if (score >= 60) {
    cJSON_AddItemToArray(rec, cJSON_CreateString("Consider blocking - high risk indicator"));
    cJSON_AddItemToArray(rec, cJSON_CreateString("Monitor for related activity"));
  } else if (score >= 40) {
    cJSON_AddItemToArray(rec, cJSON_CreateString("Exercise caution - suspicious indicator"));
    cJSON_AddItemToArray(rec, cJSON_CreateString("Add to watchlist"));
  } else if (score >= 20) {
    cJSON_AddItemToArray(rec, cJSON_CreateString("Low risk - continue monitoring"));
  } else {
    cJSON_AddItemToArray(rec, cJSON_CreateString("No known threats - appears clean"));
  }
  cJSON_AddItemToObject(sum, "recommendations", rec);
  cJSON_AddItemToObject(root, "summary", sum);

  int success = sources > 0;
  int conf = sources > 0 ? 70 + sources * 5 : 30;
  if (conf > 95) conf = 95;

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", conf);
  cJSON_AddItemToObject(env, "data", root);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "THREAT_FEED_LOOKUP");
  cJSON_AddStringToObject(props, "entity", ind);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", conf);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300]; snprintf(rk, sizeof rk, "threatfeed:%s", ind);
  char title[320]; snprintf(title, sizeof title, "THREAT_FEED_LOOKUP — %s", ind);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = success ? "threat feed" : "no threat sources";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"THREAT_FEED_LOOKUP\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def threat_feed_def = {
  .id = "THREAT_FEED_LOOKUP", .collector = "osint",
  .name = "Threat Feed Lookup", .name_ja = "脅威フィード照会",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(threat_feed_def)
