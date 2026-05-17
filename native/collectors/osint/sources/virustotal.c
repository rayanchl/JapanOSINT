/* collectors/osint/sources/virustotal.c
 * OSINT service — port of OSINTsaas osint_tools/virustotal.c
 * (handle_virustotal). Canonical SERVICE = MALWARE_ANALYSIS — the FIRST
 * registry name bound to handle_virustotal (osint_dispatcher.c:210
 * {SERVICE_MALWARE_ANALYSIS, handle_virustotal, "MALWARE_ANALYSIS", true};
 * URL_SCANNER aliases the same handler, so MALWARE_ANALYSIS is the dedup id).
 * On-demand (interval 0); ctx->entity = ip / url / hash / domain. Upstream:
 * VirusTotal API v3 (www.virustotal.com/api/v3). VIRUSTOTAL_API_KEY /
 * VT_API_KEY is optional — the C original explicitly ignores the key for the
 * request ((void)api_key) and runs the public no-key path, only flagging
 * api_key_configured + a note; we reproduce that exactly (no return-0 gate).
 * Faithfully reproduces analyze_entity routing (is_valid_ip → ip, http(s):// →
 * url via base64url id, 32/40/64 hex → file, else domain) and the per-type
 * field extraction, root {service,api_key_configured,entities_analyzed,
 * threats_detected,results[],note?}. confidence 90. Emits ONE
 * osint_service_result row like dns_records.c. */
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

/* base64url(url) without padding — VT URL id. */
static char *vt_url_id(const char *in) {
  static const char *b64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  size_t n = strlen(in);
  char *out = malloc(((n + 2) / 3) * 4 + 1);
  if (!out) return NULL;
  size_t o = 0, i = 0;
  while (i + 2 < n) {
    unsigned x = ((unsigned char)in[i] << 16) | ((unsigned char)in[i+1] << 8) | (unsigned char)in[i+2];
    out[o++] = b64[(x >> 18) & 63]; out[o++] = b64[(x >> 12) & 63];
    out[o++] = b64[(x >> 6) & 63];  out[o++] = b64[x & 63];
    i += 3;
  }
  if (n - i == 1) {
    unsigned x = (unsigned char)in[i] << 16;
    out[o++] = b64[(x >> 18) & 63]; out[o++] = b64[(x >> 12) & 63];
  } else if (n - i == 2) {
    unsigned x = ((unsigned char)in[i] << 16) | ((unsigned char)in[i+1] << 8);
    out[o++] = b64[(x >> 18) & 63]; out[o++] = b64[(x >> 12) & 63];
    out[o++] = b64[(x >> 6) & 63];
  }
  out[o] = 0;
  for (size_t k = 0; k < o; k++) {
    if (out[k] == '+') out[k] = '-';
    else if (out[k] == '/') out[k] = '_';
  }
  return out;
}

static cJSON *vt_get(http_client *http, const char *url, long *status_out) {
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  long st = hc == 0 ? hr.status : 0;
  cJSON *j = (hc == 0 && st == 200 && hr.body) ? cJSON_Parse(hr.body) : NULL;
  *status_out = st;
  http_response_free(&hr);
  return j;
}

static cJSON *analyze_url(http_client *http, const char *url) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "type", "url");
  cJSON_AddStringToObject(r, "target", url);
  char *id = vt_url_id(url);
  if (!id) { cJSON_AddStringToObject(r, "error", "Failed to encode URL"); return r; }
  char api[512];
  snprintf(api, sizeof api, "https://www.virustotal.com/api/v3/urls/%s", id);
  free(id);
  long st = 0;
  cJSON *j = vt_get(http, api, &st);
  if (st == 404) { cJSON_AddStringToObject(r, "status", "not_found");
                   cJSON_AddStringToObject(r, "message", "URL not previously scanned");
                   if (j) cJSON_Delete(j); return r; }
  if (st != 200) { cJSON_AddStringToObject(r, "error", "API error");
                   cJSON_AddNumberToObject(r, "http_code", (double)st);
                   if (j) cJSON_Delete(j); return r; }
  if (!j) return r;
  const cJSON *d = cJSON_GetObjectItem(j, "data");
  const cJSON *a = d ? cJSON_GetObjectItem(d, "attributes") : NULL;
  if (a) {
    const cJSON *s = cJSON_GetObjectItem(a, "last_analysis_stats");
    if (s) {
      const cJSON *m = cJSON_GetObjectItem(s, "malicious");
      const cJSON *su = cJSON_GetObjectItem(s, "suspicious");
      const cJSON *h = cJSON_GetObjectItem(s, "harmless");
      const cJSON *u = cJSON_GetObjectItem(s, "undetected");
      int mal = m ? m->valueint : 0, sus = su ? su->valueint : 0;
      cJSON_AddNumberToObject(r, "malicious_votes", mal);
      cJSON_AddNumberToObject(r, "suspicious_votes", sus);
      cJSON_AddNumberToObject(r, "harmless_votes", h ? h->valueint : 0);
      cJSON_AddNumberToObject(r, "undetected_votes", u ? u->valueint : 0);
      cJSON_AddStringToObject(r, "threat_level",
        mal > 3 ? "HIGH" : (mal > 0 || sus > 2) ? "MEDIUM" : sus > 0 ? "LOW" : "CLEAN");
    }
    const cJSON *cat = cJSON_GetObjectItem(a, "categories");
    if (cat) cJSON_AddItemToObject(r, "categories", cJSON_Duplicate(cat, 1));
    const cJSON *fu = cJSON_GetObjectItem(a, "last_final_url");
    if (fu && cJSON_IsString(fu)) cJSON_AddStringToObject(r, "final_url", fu->valuestring);
  }
  cJSON_Delete(j);
  return r;
}

static cJSON *analyze_domain(http_client *http, const char *dom) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "type", "domain");
  cJSON_AddStringToObject(r, "target", dom);
  char api[512];
  snprintf(api, sizeof api, "https://www.virustotal.com/api/v3/domains/%s", dom);
  long st = 0;
  cJSON *j = vt_get(http, api, &st);
  if (st != 200 || !j) { cJSON_AddStringToObject(r, "status", "error"); if (j) cJSON_Delete(j); return r; }
  const cJSON *d = cJSON_GetObjectItem(j, "data");
  const cJSON *a = d ? cJSON_GetObjectItem(d, "attributes") : NULL;
  if (a) {
    const cJSON *rep = cJSON_GetObjectItem(a, "reputation");
    if (rep) cJSON_AddNumberToObject(r, "reputation_score", rep->valueint);
    const cJSON *s = cJSON_GetObjectItem(a, "last_analysis_stats");
    if (s) {
      const cJSON *m = cJSON_GetObjectItem(s, "malicious");
      const cJSON *su = cJSON_GetObjectItem(s, "suspicious");
      cJSON_AddNumberToObject(r, "malicious_votes", m ? m->valueint : 0);
      cJSON_AddNumberToObject(r, "suspicious_votes", su ? su->valueint : 0);
    }
    const cJSON *rg = cJSON_GetObjectItem(a, "registrar");
    if (rg && cJSON_IsString(rg)) cJSON_AddStringToObject(r, "registrar", rg->valuestring);
    const cJSON *cd = cJSON_GetObjectItem(a, "creation_date");
    if (cd) cJSON_AddNumberToObject(r, "creation_date", cd->valueint);
    const cJSON *cat = cJSON_GetObjectItem(a, "categories");
    if (cat) cJSON_AddItemToObject(r, "categories", cJSON_Duplicate(cat, 1));
  }
  cJSON_Delete(j);
  return r;
}

static cJSON *analyze_ip(http_client *http, const char *ip) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "type", "ip_address");
  cJSON_AddStringToObject(r, "target", ip);
  char api[512];
  snprintf(api, sizeof api, "https://www.virustotal.com/api/v3/ip_addresses/%s", ip);
  long st = 0;
  cJSON *j = vt_get(http, api, &st);
  if (st != 200 || !j) { cJSON_AddStringToObject(r, "status", "error"); if (j) cJSON_Delete(j); return r; }
  const cJSON *d = cJSON_GetObjectItem(j, "data");
  const cJSON *a = d ? cJSON_GetObjectItem(d, "attributes") : NULL;
  if (a) {
    const cJSON *asn = cJSON_GetObjectItem(a, "asn");
    const cJSON *ao = cJSON_GetObjectItem(a, "as_owner");
    const cJSON *co = cJSON_GetObjectItem(a, "country");
    if (asn) cJSON_AddNumberToObject(r, "asn", asn->valueint);
    if (ao && cJSON_IsString(ao)) cJSON_AddStringToObject(r, "as_owner", ao->valuestring);
    if (co && cJSON_IsString(co)) cJSON_AddStringToObject(r, "country", co->valuestring);
    const cJSON *s = cJSON_GetObjectItem(a, "last_analysis_stats");
    if (s) {
      const cJSON *m = cJSON_GetObjectItem(s, "malicious");
      const cJSON *su = cJSON_GetObjectItem(s, "suspicious");
      cJSON_AddNumberToObject(r, "malicious_votes", m ? m->valueint : 0);
      cJSON_AddNumberToObject(r, "suspicious_votes", su ? su->valueint : 0);
    }
    const cJSON *rep = cJSON_GetObjectItem(a, "reputation");
    if (rep) cJSON_AddNumberToObject(r, "reputation_score", rep->valueint);
  }
  cJSON_Delete(j);
  return r;
}

static cJSON *analyze_hash(http_client *http, const char *hash) {
  cJSON *r = cJSON_CreateObject();
  cJSON_AddStringToObject(r, "type", "file_hash");
  cJSON_AddStringToObject(r, "target", hash);
  char api[512];
  snprintf(api, sizeof api, "https://www.virustotal.com/api/v3/files/%s", hash);
  long st = 0;
  cJSON *j = vt_get(http, api, &st);
  if (st != 200 || !j) { cJSON_AddStringToObject(r, "status", st == 404 ? "not_found" : "error");
                          if (j) cJSON_Delete(j); return r; }
  const cJSON *d = cJSON_GetObjectItem(j, "data");
  const cJSON *a = d ? cJSON_GetObjectItem(d, "attributes") : NULL;
  if (a) {
    const cJSON *nm = cJSON_GetObjectItem(a, "names");
    if (nm && cJSON_IsArray(nm) && cJSON_GetArraySize(nm) > 0) {
      const cJSON *f = cJSON_GetArrayItem(nm, 0);
      if (f && cJSON_IsString(f)) cJSON_AddStringToObject(r, "file_name", f->valuestring);
    }
    const cJSON *td = cJSON_GetObjectItem(a, "type_description");
    if (td && cJSON_IsString(td)) cJSON_AddStringToObject(r, "file_type", td->valuestring);
    const cJSON *sz = cJSON_GetObjectItem(a, "size");
    if (sz) cJSON_AddNumberToObject(r, "file_size", sz->valueint);
    const cJSON *s = cJSON_GetObjectItem(a, "last_analysis_stats");
    if (s) {
      const cJSON *m = cJSON_GetObjectItem(s, "malicious");
      const cJSON *su = cJSON_GetObjectItem(s, "suspicious");
      int mal = m ? m->valueint : 0;
      cJSON_AddNumberToObject(r, "malicious_detections", mal);
      cJSON_AddNumberToObject(r, "suspicious_detections", su ? su->valueint : 0);
      cJSON_AddStringToObject(r, "verdict",
        mal > 5 ? "MALICIOUS" : mal > 0 ? "SUSPICIOUS" : "CLEAN");
    }
    const cJSON *pt = cJSON_GetObjectItem(a, "popular_threat_classification");
    if (pt) {
      const cJSON *l = cJSON_GetObjectItem(pt, "suggested_threat_label");
      if (l && cJSON_IsString(l)) cJSON_AddStringToObject(r, "threat_label", l->valuestring);
    }
  }
  cJSON_Delete(j);
  return r;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *e = ctx->entity;
  if (!e || !*e) return -1;
  if (strchr(e, '@')) return -1;   /* C handler skips emails (no result) */

  const char *key = getenv("VIRUSTOTAL_API_KEY");
  if (!key) key = getenv("VT_API_KEY");
  int has_key = key && *key;

  cJSON *analysis;
  if (looks_ipv4(e)) analysis = analyze_ip(ctx->http, e);
  else if (strncmp(e, "http://", 7) == 0 || strncmp(e, "https://", 8) == 0)
    analysis = analyze_url(ctx->http, e);
  else {
    size_t n = strlen(e);
    int hex = 1;
    for (size_t i = 0; i < n && hex; i++) if (!isxdigit((unsigned char)e[i])) hex = 0;
    if (hex && (n == 32 || n == 40 || n == 64)) analysis = analyze_hash(ctx->http, e);
    else analysis = analyze_domain(ctx->http, e);
  }

  int threat = 0;
  const cJSON *m = cJSON_GetObjectItem(analysis, "malicious_votes");
  if (!m) m = cJSON_GetObjectItem(analysis, "malicious_detections");
  if (m && m->valueint > 0) threat = 1;

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "service", "VIRUSTOTAL");
  cJSON_AddBoolToObject(root, "api_key_configured", has_key);
  cJSON_AddNumberToObject(root, "entities_analyzed", 1);
  cJSON_AddNumberToObject(root, "threats_detected", threat);
  cJSON *results = cJSON_CreateArray();
  cJSON_AddItemToArray(results, analysis);
  cJSON_AddItemToObject(root, "results", results);
  if (!has_key)
    cJSON_AddStringToObject(root, "note",
      "Using free tier. Set VIRUSTOTAL_API_KEY for better rate limits.");

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", 90);
  cJSON_AddItemToObject(env, "data", root);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "MALWARE_ANALYSIS");
  cJSON_AddStringToObject(props, "entity", e);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 90);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300]; snprintf(rk, sizeof rk, "virustotal:%s", e);
  char title[320]; snprintf(title, sizeof title, "MALWARE_ANALYSIS — %s", e);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = threat ? "VT threat detected" : "VT analyzed";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"MALWARE_ANALYSIS\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def virustotal_def = {
  .id = "MALWARE_ANALYSIS", .collector = "osint",
  .name = "VirusTotal Malware Analysis", .name_ja = "VirusTotalマルウェア解析",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(virustotal_def)
