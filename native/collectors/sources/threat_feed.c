/* collectors/osint/sources/threat_feed.c
 * OSINT service — port of OSINTsaas osint_tools/threat_feed.c
 * (threat_feed_lookup → handle_threat_feed). Canonical SERVICE =
 * THREAT_FEED_LOOKUP (dispatcher row {SERVICE_THREAT_INTEL, handle_threat_feed,
 * "THREAT_FEED_LOOKUP", true} — distinct handler from handle_threat_intel, so
 * its own source id). On-demand (interval 0); ctx->entity = an IOC (ip / domain
 * / url / hash / email). No required key (OTX_API_KEY optional). Faithfully
 * reproduces detect_ioc_type and the upstream queries: query_otx (X-OTX-API-KEY
 * header if set; pulses → threat reports), query_urlhaus (POST
 * x-www-form-urlencoded, url|host), query_threatfox (POST JSON
 * {query:search_ioc,search_term}), query_circl (SHA256 only).
 *
 * PER-RECORD EMIT: emits ONE osint_service_result row per IOC / pulse /
 * malicious URL / threat report, each keyed by a stable per-item id
 * ("otx:<pulse_id>", "urlhaus:<url_id>", "threatfox:<ioc_id>",
 * "circl:<sha256>"). title = a short descriptor; body = that item's real
 * fetched JSON. No synthetic threat_score summary row. Nothing found → emit
 * nothing, return 0 (honest empty). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
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

/* Emit one intel row for a single threat item. Returns 1 if emitted. */
static int emit_item(intel_sink *sink, const char *ind, const char *feed_source,
                     const char *remote_key, const char *title,
                     const char *summary, const char *published_at,
                     cJSON *data /*owned by caller*/) {
  char *bj = cJSON_PrintUnformatted(data);
  if (!bj) return 0;

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "THREAT_FEED_LOOKUP");
  cJSON_AddStringToObject(props, "entity", ind);
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
  it.tags_json       = "[\"osint-search\",\"THREAT_FEED_LOOKUP\"]";
  int rc = sink->emit(sink, &it);

  free(bj);
  free(pj);
  cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* OTX: one row per pulse associated with the indicator. */
static int query_otx(http_client *http, const char *ind, ioc_t t, intel_sink *sink) {
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
  if (!j) return 0;

  int emitted = 0;
  const cJSON *pi = cJSON_GetObjectItem(j, "pulse_info");
  const cJSON *pulses = pi ? cJSON_GetObjectItem(pi, "pulses") : NULL;
  if (pulses && cJSON_IsArray(pulses)) {
    int pn = cJSON_GetArraySize(pulses), mx = pn > 5 ? 5 : pn;
    for (int i = 0; i < mx; i++) {
      const cJSON *p = cJSON_GetArrayItem(pulses, i);
      if (!p) continue;
      const cJSON *id = cJSON_GetObjectItem(p, "id");
      const cJSON *nm = cJSON_GetObjectItem(p, "name");
      const cJSON *de = cJSON_GetObjectItem(p, "description");
      const cJSON *tg = cJSON_GetObjectItem(p, "tags");
      const cJSON *cr = cJSON_GetObjectItem(p, "created");
      const cJSON *tl = cJSON_GetObjectItem(p, "tlp");
      const cJSON *ad = cJSON_GetObjectItem(p, "adversary");
      const cJSON *mf = cJSON_GetObjectItem(p, "malware_families");

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "source", "AlienVault OTX");
      cJSON_AddStringToObject(data, "indicator", ind);
      if (nm && cJSON_IsString(nm)) cJSON_AddStringToObject(data, "name", nm->valuestring);
      if (de && cJSON_IsString(de)) {
        char d[256]; strncpy(d, de->valuestring, 255); d[255] = 0;
        cJSON_AddStringToObject(data, "description", d);
      }
      if (cr && cJSON_IsString(cr)) cJSON_AddStringToObject(data, "date", cr->valuestring);
      if (tl && cJSON_IsString(tl)) cJSON_AddStringToObject(data, "tlp", tl->valuestring);
      if (ad && cJSON_IsString(ad) && ad->valuestring[0])
        cJSON_AddStringToObject(data, "threat_actor", ad->valuestring);
      if (tg && cJSON_IsArray(tg) && cJSON_GetArraySize(tg) > 0)
        cJSON_AddItemToObject(data, "tags", cJSON_Duplicate(tg, 1));
      if (mf && cJSON_IsArray(mf) && cJSON_GetArraySize(mf) > 0)
        cJSON_AddItemToObject(data, "malware_families", cJSON_Duplicate(mf, 1));

      char rk[300];
      if (id && cJSON_IsString(id)) snprintf(rk, sizeof rk, "otx:%s", id->valuestring);
      else snprintf(rk, sizeof rk, "otx:%s:%d", ind, i);
      char title[320];
      snprintf(title, sizeof title, "OTX pulse — %s",
               (nm && cJSON_IsString(nm)) ? nm->valuestring : ind);
      const char *pub = (cr && cJSON_IsString(cr)) ? cr->valuestring : NULL;

      emitted += emit_item(sink, ind, "AlienVault OTX", rk, title,
                           "OTX threat pulse", pub, data);
      cJSON_Delete(data);
    }
  }
  cJSON_Delete(j);
  return emitted;
}

/* URLhaus: one row per malicious URL (host lookup) or one row for the URL. */
static int query_urlhaus(http_client *http, const char *ind, ioc_t t, intel_sink *sink) {
  char url[256], payload[512];
  if (t == IOC_URL) { snprintf(url, sizeof url, "https://urlhaus-api.abuse.ch/v1/url/");
                       snprintf(payload, sizeof payload, "url=%s", ind); }
  else if (t == IOC_DOMAIN) { snprintf(url, sizeof url, "https://urlhaus-api.abuse.ch/v1/host/");
                              snprintf(payload, sizeof payload, "host=%s", ind); }
  else return 0;
  static const char *hdrs[] = { "Content-Type", "application/x-www-form-urlencoded", NULL };
  http_response hr = {0};
  int hc = http_request(http, "POST", url, hdrs, payload, strlen(payload), 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  if (!j) return 0;
  const cJSON *qs = cJSON_GetObjectItem(j, "query_status");
  if (!qs || !cJSON_IsString(qs) || strcmp(qs->valuestring, "ok") != 0) { cJSON_Delete(j); return 0; }

  int emitted = 0;
  if (t == IOC_DOMAIN) {
    const cJSON *urls = cJSON_GetObjectItem(j, "urls");
    if (urls && cJSON_IsArray(urls)) {
      int uc = cJSON_GetArraySize(urls), mx = uc > 10 ? 10 : uc;
      for (int i = 0; i < mx; i++) {
        const cJSON *u = cJSON_GetArrayItem(urls, i);
        if (!u) continue;
        const cJSON *uid = cJSON_GetObjectItem(u, "id");
        const cJSON *ui  = cJSON_GetObjectItem(u, "url");
        const cJSON *da  = cJSON_GetObjectItem(u, "date_added");
        const cJSON *th  = cJSON_GetObjectItem(u, "threat");
        const cJSON *us  = cJSON_GetObjectItem(u, "url_status");

        cJSON *data = cJSON_CreateObject();
        cJSON_AddStringToObject(data, "source", "URLhaus");
        cJSON_AddStringToObject(data, "host", ind);
        if (ui && cJSON_IsString(ui)) cJSON_AddStringToObject(data, "url", ui->valuestring);
        if (da && cJSON_IsString(da)) cJSON_AddStringToObject(data, "date_added", da->valuestring);
        if (th && cJSON_IsString(th)) cJSON_AddStringToObject(data, "threat_type", th->valuestring);
        if (us && cJSON_IsString(us)) cJSON_AddStringToObject(data, "status", us->valuestring);

        char rk[300];
        if (uid && cJSON_IsString(uid)) snprintf(rk, sizeof rk, "urlhaus:%s", uid->valuestring);
        else if (uid && cJSON_IsNumber(uid)) snprintf(rk, sizeof rk, "urlhaus:%d", uid->valueint);
        else snprintf(rk, sizeof rk, "urlhaus:%s:%d", ind, i);
        char title[320];
        snprintf(title, sizeof title, "URLhaus malicious URL — %s",
                 (ui && cJSON_IsString(ui)) ? ui->valuestring : ind);
        const char *pub = (da && cJSON_IsString(da)) ? da->valuestring : NULL;

        emitted += emit_item(sink, ind, "URLhaus", rk, title,
                             "URLhaus malicious URL", pub, data);
        cJSON_Delete(data);
      }
    }
  } else {
    const cJSON *uid = cJSON_GetObjectItem(j, "id");
    const cJSON *th  = cJSON_GetObjectItem(j, "threat");
    const cJSON *da  = cJSON_GetObjectItem(j, "date_added");
    const cJSON *us  = cJSON_GetObjectItem(j, "url_status");
    const cJSON *tg  = cJSON_GetObjectItem(j, "tags");

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "source", "URLhaus");
    cJSON_AddStringToObject(data, "url", ind);
    if (th && cJSON_IsString(th)) cJSON_AddStringToObject(data, "threat_type", th->valuestring);
    if (da && cJSON_IsString(da)) cJSON_AddStringToObject(data, "date_added", da->valuestring);
    if (us && cJSON_IsString(us)) cJSON_AddStringToObject(data, "status", us->valuestring);
    if (tg && cJSON_IsArray(tg)) cJSON_AddItemToObject(data, "tags", cJSON_Duplicate(tg, 1));

    char rk[300];
    if (uid && cJSON_IsString(uid)) snprintf(rk, sizeof rk, "urlhaus:%s", uid->valuestring);
    else if (uid && cJSON_IsNumber(uid)) snprintf(rk, sizeof rk, "urlhaus:%d", uid->valueint);
    else snprintf(rk, sizeof rk, "urlhaus:%s", ind);
    char title[320];
    snprintf(title, sizeof title, "URLhaus malicious URL — %s", ind);
    const char *pub = (da && cJSON_IsString(da)) ? da->valuestring : NULL;

    emitted += emit_item(sink, ind, "URLhaus", rk, title,
                         "URLhaus malicious URL", pub, data);
    cJSON_Delete(data);
  }
  cJSON_Delete(j);
  return emitted;
}

/* ThreatFox: one row per matched IOC. */
static int query_threatfox(http_client *http, const char *ind, intel_sink *sink) {
  cJSON *req = cJSON_CreateObject();
  cJSON_AddStringToObject(req, "query", "search_ioc");
  cJSON_AddStringToObject(req, "search_term", ind);
  char *payload = cJSON_PrintUnformatted(req);
  cJSON_Delete(req);
  if (!payload) return 0;
  static const char *hdrs[] = { "Content-Type", "application/json", NULL };
  http_response hr = {0};
  int hc = http_request(http, "POST", "https://threatfox-api.abuse.ch/api/v1/",
                         hdrs, payload, strlen(payload), 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  free(payload);
  if (!j) return 0;
  const cJSON *qs = cJSON_GetObjectItem(j, "query_status");
  if (!qs || !cJSON_IsString(qs) || strcmp(qs->valuestring, "ok") != 0) { cJSON_Delete(j); return 0; }
  const cJSON *data = cJSON_GetObjectItem(j, "data");
  if (!data || !cJSON_IsArray(data) || cJSON_GetArraySize(data) == 0) { cJSON_Delete(j); return 0; }

  int emitted = 0;
  int dc = cJSON_GetArraySize(data), mx = dc > 5 ? 5 : dc;
  for (int i = 0; i < mx; i++) {
    const cJSON *o = cJSON_GetArrayItem(data, i);
    if (!o) continue;
    const cJSON *id = cJSON_GetObjectItem(o, "id");
    const cJSON *iv = cJSON_GetObjectItem(o, "ioc");
    const cJSON *it = cJSON_GetObjectItem(o, "ioc_type");
    const cJSON *tt = cJSON_GetObjectItem(o, "threat_type");
    const cJSON *mw = cJSON_GetObjectItem(o, "malware");
    const cJSON *cf = cJSON_GetObjectItem(o, "confidence_level");
    const cJSON *fs = cJSON_GetObjectItem(o, "first_seen_utc");
    const cJSON *tg = cJSON_GetObjectItem(o, "tags");

    cJSON *d = cJSON_CreateObject();
    cJSON_AddStringToObject(d, "source", "ThreatFox");
    cJSON_AddStringToObject(d, "indicator", ind);
    if (iv && cJSON_IsString(iv)) cJSON_AddStringToObject(d, "ioc", iv->valuestring);
    if (it && cJSON_IsString(it)) cJSON_AddStringToObject(d, "ioc_type", it->valuestring);
    if (tt && cJSON_IsString(tt)) cJSON_AddStringToObject(d, "threat_type", tt->valuestring);
    if (mw && cJSON_IsString(mw)) cJSON_AddStringToObject(d, "malware", mw->valuestring);
    if (cf && cJSON_IsNumber(cf)) cJSON_AddNumberToObject(d, "confidence", cf->valueint);
    if (fs && cJSON_IsString(fs)) cJSON_AddStringToObject(d, "first_seen", fs->valuestring);
    if (tg && cJSON_IsArray(tg)) cJSON_AddItemToObject(d, "tags", cJSON_Duplicate(tg, 1));

    char rk[300];
    if (id && cJSON_IsString(id)) snprintf(rk, sizeof rk, "threatfox:%s", id->valuestring);
    else if (id && cJSON_IsNumber(id)) snprintf(rk, sizeof rk, "threatfox:%d", id->valueint);
    else snprintf(rk, sizeof rk, "threatfox:%s:%d", ind, i);
    char title[320];
    snprintf(title, sizeof title, "ThreatFox IOC — %s",
             (mw && cJSON_IsString(mw)) ? mw->valuestring
             : (iv && cJSON_IsString(iv)) ? iv->valuestring : ind);
    const char *pub = (fs && cJSON_IsString(fs)) ? fs->valuestring : NULL;

    emitted += emit_item(sink, ind, "ThreatFox", rk, title,
                         "ThreatFox indicator", pub, d);
    cJSON_Delete(d);
  }
  cJSON_Delete(j);
  return emitted;
}

/* CIRCL hashlookup: one row for the SHA256 if known. */
static int query_circl(http_client *http, const char *hash, intel_sink *sink) {
  if (strlen(hash) != 64) return 0;
  char url[256];
  snprintf(url, sizeof url, "https://hashlookup.circl.lu/lookup/sha256/%s", hash);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  cJSON *j = (hc == 0 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);
  if (!j) return 0;
  /* Unknown hash → CIRCL returns a message/404 body with no file metadata. */
  const cJSON *fn = cJSON_GetObjectItem(j, "FileName");
  const cJSON *fz = cJSON_GetObjectItem(j, "FileSize");
  const cJSON *km = cJSON_GetObjectItem(j, "KnownMalicious");
  if (!fn && !fz && !km) { cJSON_Delete(j); return 0; }

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "source", "CIRCL hashlookup");
  cJSON_AddStringToObject(data, "sha256", hash);
  if (fn && cJSON_IsString(fn)) cJSON_AddStringToObject(data, "filename", fn->valuestring);
  if (fz && cJSON_IsString(fz)) cJSON_AddStringToObject(data, "filesize", fz->valuestring);
  if (km) cJSON_AddBoolToObject(data, "known_malicious", cJSON_IsTrue(km));

  char rk[300]; snprintf(rk, sizeof rk, "circl:%s", hash);
  char title[320];
  snprintf(title, sizeof title, "CIRCL hashlookup — %s",
           (fn && cJSON_IsString(fn)) ? fn->valuestring : hash);
  int emitted = emit_item(sink, hash, "CIRCL hashlookup", rk, title,
                          "known file hash", NULL, data);
  cJSON_Delete(data);
  cJSON_Delete(j);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *ind = ctx->entity;
  if (!ind || !*ind) return -1;

  ioc_t t = detect_ioc(ind);
  int emitted = 0;

  emitted += query_otx(ctx->http, ind, t, sink);
  if (t == IOC_URL || t == IOC_DOMAIN)
    emitted += query_urlhaus(ctx->http, ind, t, sink);
  emitted += query_threatfox(ctx->http, ind, sink);
  if (t == IOC_SHA256)
    emitted += query_circl(ctx->http, ind, sink);

  (void)emitted;            /* nothing found → emit nothing, honest empty */
  return 0;
}

static const source_def threat_feed_def = {
  .id = "THREAT_FEED_LOOKUP", .collector = "osint",
  .name = "Threat Feed Lookup", .name_ja = "脅威フィード照会",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/threat-feed-lookup",
  .description = "Look up an indicator in threat-intel feeds (OTX).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(threat_feed_def)
