/* collectors/osint/sources/ct_logs.c
 * OSINT service — faithful port of OSINTsaas osint_tools/ct_logs.c
 * (handle_ct_logs). Canonical SERVICE id in osint_dispatcher.c:
 * {SERVICE_CERTIFICATE_TRANSPARENCY, handle_ct_logs,
 * "CERTIFICATE_TRANSPARENCY", true}. (Alias SSL_ANALYZER maps to a different
 * handler handle_ssl_analyzer → not this file.) Entity = domain. Upstream:
 * crt.sh (?q=%25.<dom>&output=json) primary + api.certspotter.com secondary;
 * no API key. Builds {service,total_subdomains_found,domains:[{domain,
 * unique_subdomains,certspotter_additional,certificates:[…]}],sources}.
 * success always true, confidence 95. Single-entity (pivot) form of the
 * upstream multi-entity loop. Emits one osint_service_result row (body =
 * {success,confidence,data}), like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
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

/* crt.sh JSON → array of cert objects (dedup by clean_name, ≤500). */
static cJSON *query_crt_sh(http_client *http, const char *domain) {
  cJSON *results = cJSON_CreateArray();
  char enc[256];
  uri_encode(domain, enc, sizeof enc);
  char url[512];
  snprintf(url, sizeof url, "https://crt.sh/?q=%%25.%s&output=json", enc);

  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return results; }
  cJSON *j = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!j || !cJSON_IsArray(j)) { if (j) cJSON_Delete(j); return results; }

  char *seen[500]; int sc = 0;
  int n = cJSON_GetArraySize(j);
  for (int i = 0; i < n && sc < 500; i++) {
    cJSON *cert = cJSON_GetArrayItem(j, i);
    if (!cert) continue;
    cJSON *nv = cJSON_GetObjectItem(cert, "name_value");
    cJSON *iss = cJSON_GetObjectItem(cert, "issuer_name");
    cJSON *nb = cJSON_GetObjectItem(cert, "not_before");
    cJSON *na = cJSON_GetObjectItem(cert, "not_after");
    cJSON *cid = cJSON_GetObjectItem(cert, "id");
    if (!nv || !nv->valuestring) continue;
    char *names = strdup(nv->valuestring), *sp = NULL;
    for (char *nm = strtok_r(names, "\n", &sp); nm && sc < 500;
         nm = strtok_r(NULL, "\n", &sp)) {
      char *clean = nm;
      if (strncmp(nm, "*.", 2) == 0) clean = nm + 2;
      int dup = 0;
      for (int k = 0; k < sc; k++)
        if (strcasecmp(seen[k], clean) == 0) { dup = 1; break; }
      if (dup) continue;
      seen[sc++] = strdup(clean);
      cJSON *r = cJSON_CreateObject();
      cJSON_AddStringToObject(r, "subdomain", nm);
      cJSON_AddStringToObject(r, "clean_name", clean);
      if (iss && iss->valuestring) cJSON_AddStringToObject(r, "issuer", iss->valuestring);
      if (nb && nb->valuestring) cJSON_AddStringToObject(r, "valid_from", nb->valuestring);
      if (na && na->valuestring) cJSON_AddStringToObject(r, "valid_until", na->valuestring);
      if (cid) cJSON_AddNumberToObject(r, "cert_id", cid->valueint);
      cJSON_AddItemToArray(results, r);
    }
    free(names);
  }
  for (int i = 0; i < sc; i++) free(seen[i]);
  cJSON_Delete(j);
  return results;
}

/* certspotter issuances → array of {subdomain,source:"certspotter"} (≤50). */
static cJSON *query_certspotter(http_client *http, const char *domain) {
  cJSON *results = cJSON_CreateArray();
  char url[512];
  snprintf(url, sizeof url,
    "https://api.certspotter.com/v1/issuances?domain=%s&include_subdomains=true&expand=dns_names",
    domain);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return results; }
  cJSON *j = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (j && cJSON_IsArray(j)) {
    int n = cJSON_GetArraySize(j);
    for (int i = 0; i < n && i < 50; i++) {
      cJSON *cert = cJSON_GetArrayItem(j, i);
      cJSON *dn = cJSON_GetObjectItem(cert, "dns_names");
      if (dn && cJSON_IsArray(dn)) {
        int m = cJSON_GetArraySize(dn);
        for (int k = 0; k < m; k++) {
          cJSON *nm = cJSON_GetArrayItem(dn, k);
          if (nm && nm->valuestring) {
            cJSON *r = cJSON_CreateObject();
            cJSON_AddStringToObject(r, "subdomain", nm->valuestring);
            cJSON_AddStringToObject(r, "source", "certspotter");
            cJSON_AddItemToArray(results, r);
          }
        }
      }
    }
  }
  if (j) cJSON_Delete(j);
  return results;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *domain = ctx->entity;
  if (!domain || !*domain) return -1;
  if (strchr(domain, '@')) return -1;   /* upstream skips emails */

  cJSON *all = cJSON_CreateArray();
  cJSON *dr = cJSON_CreateObject();
  cJSON_AddStringToObject(dr, "domain", domain);

  cJSON *crt = query_crt_sh(ctx->http, domain);
  int crt_count = cJSON_GetArraySize(crt);
  cJSON *cs = query_certspotter(ctx->http, domain);
  int cs_count = cJSON_GetArraySize(cs);
  cJSON *it;
  cJSON_ArrayForEach(it, cs)
    cJSON_AddItemToArray(crt, cJSON_Duplicate(it, 1));
  cJSON_Delete(cs);

  cJSON_AddNumberToObject(dr, "unique_subdomains", crt_count);
  cJSON_AddNumberToObject(dr, "certspotter_additional", cs_count);
  cJSON_AddItemToObject(dr, "certificates", crt);
  cJSON_AddItemToArray(all, dr);

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "service", "CERTIFICATE_TRANSPARENCY");
  cJSON_AddNumberToObject(data, "total_subdomains_found", crt_count);
  cJSON_AddItemToObject(data, "domains", all);
  cJSON_AddStringToObject(data, "sources", "crt.sh, certspotter");

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", 95);
  cJSON_AddItemToObject(env, "data", data);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "CERTIFICATE_TRANSPARENCY");
  cJSON_AddStringToObject(props, "entity", domain);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 95);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300], title[320];
  snprintf(rk, sizeof rk, "ctlogs:%s", domain);
  snprintf(title, sizeof title, "CERTIFICATE_TRANSPARENCY — %s", domain);

  intel_item iti = {0};
  iti.remote_key      = rk;
  iti.title           = title;
  iti.body            = bj;
  iti.summary         = crt_count > 0 ? "CT subdomains found"
                                      : "no CT records";
  iti.record_type     = "osint_service_result";
  iti.properties_json = pj;
  iti.tags_json       = "[\"osint-search\",\"CERTIFICATE_TRANSPARENCY\"]";
  int rc = sink->emit(sink, &iti);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def ct_logs_def = {
  .id = "CERTIFICATE_TRANSPARENCY", .collector = "osint",
  .name = "Certificate Transparency", .name_ja = "証明書透過ログ",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(ct_logs_def)
