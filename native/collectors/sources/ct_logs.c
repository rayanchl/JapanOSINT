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
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>

/* crt.sh JSON → array of cert objects (dedup by clean_name, ≤500). */
static cJSON *query_crt_sh(http_client *http, const char *domain) {
  cJSON *results = cJSON_CreateArray();
  char enc[256];
  jo_uri_encode_buf(domain, enc, sizeof enc);
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

/* Has remote_key `rk` already been emitted this run? Linear scan over the
 * intra-run dedup set (records are bounded ≤550, sub-GB). */
static int seen_rk(char **set, int n, const char *rk) {
  for (int i = 0; i < n; i++) if (strcmp(set[i], rk) == 0) return 1;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *domain = ctx->entity;
  if (!domain || !*domain) return -1;
  if (strchr(domain, '@')) return -1;   /* upstream skips emails */

  /* Merge the two real feeds into one record array (same fetch logic). */
  cJSON *crt = query_crt_sh(ctx->http, domain);
  cJSON *cs = query_certspotter(ctx->http, domain);
  cJSON *it;
  cJSON_ArrayForEach(it, cs)
    cJSON_AddItemToArray(crt, cJSON_Duplicate(it, 1));
  cJSON_Delete(cs);

  /* PER-RECORD EMIT: one osint_service_result row per unique cert/subdomain
   * record. remote_key = cert_id when present, else the subdomain (prefixed
   * "ct:"). Intra-run dedup so the same key never emits twice. Zero records →
   * emit nothing, return 0. */
  int total = cJSON_GetArraySize(crt);
  char *seen[600]; int seen_n = 0;
  int emitted = 0;

  cJSON_ArrayForEach(it, crt) {
    if (seen_n >= (int)(sizeof seen / sizeof seen[0])) break;

    cJSON *sd_j  = cJSON_GetObjectItem(it, "subdomain");
    cJSON *cn_j  = cJSON_GetObjectItem(it, "clean_name");
    cJSON *iss_j = cJSON_GetObjectItem(it, "issuer");
    cJSON *vf_j  = cJSON_GetObjectItem(it, "valid_from");
    cJSON *vu_j  = cJSON_GetObjectItem(it, "valid_until");
    cJSON *cid_j = cJSON_GetObjectItem(it, "cert_id");

    const char *sd = (sd_j && sd_j->valuestring) ? sd_j->valuestring : NULL;
    const char *cn = (cn_j && cn_j->valuestring) ? cn_j->valuestring : sd;
    if (!sd && !cn) continue;

    /* stable per-record key: cert_id if present, else subdomain string */
    char rk[320];
    if (cid_j && cJSON_IsNumber(cid_j))
      snprintf(rk, sizeof rk, "ct:%d", cid_j->valueint);
    else
      snprintf(rk, sizeof rk, "ct:%s", cn ? cn : sd);

    if (seen_rk(seen, seen_n, rk)) continue;
    seen[seen_n++] = strdup(rk);

    /* body: this cert's record (subdomain, issuer, valid_from, valid_to,
     * cert_id) */
    cJSON *body = cJSON_CreateObject();
    if (sd) cJSON_AddStringToObject(body, "subdomain", sd);
    if (iss_j && iss_j->valuestring)
      cJSON_AddStringToObject(body, "issuer", iss_j->valuestring);
    if (vf_j && vf_j->valuestring)
      cJSON_AddStringToObject(body, "valid_from", vf_j->valuestring);
    if (vu_j && vu_j->valuestring)
      cJSON_AddStringToObject(body, "valid_to", vu_j->valuestring);
    if (cid_j && cJSON_IsNumber(cid_j))
      cJSON_AddNumberToObject(body, "cert_id", cid_j->valueint);
    char *bj = cJSON_PrintUnformatted(body);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "service", "CERTIFICATE_TRANSPARENCY");
    cJSON_AddStringToObject(props, "entity", domain);
    char *pj = cJSON_PrintUnformatted(props);

    char summary[320];
    snprintf(summary, sizeof summary, "CT record for %s", domain);

    intel_item iti = {0};
    iti.remote_key      = rk;
    iti.title           = cn ? cn : sd;   /* subdomain / cert CN */
    iti.body            = bj;
    iti.summary         = summary;
    iti.record_type     = "osint_service_result";
    iti.properties_json = pj;
    iti.tags_json       = "[\"osint-search\",\"CERTIFICATE_TRANSPARENCY\"]";
    int rc = sink->emit(sink, &iti);
    if (rc >= 0) emitted++;

    free(bj); free(pj);
    cJSON_Delete(body);
    cJSON_Delete(props);
  }

  for (int i = 0; i < seen_n; i++) free(seen[i]);
  cJSON_Delete(crt);
  (void)total; (void)emitted;
  return 0;
}

static const source_def ct_logs_def = {
  .id = "CERTIFICATE_TRANSPARENCY", .collector = "osint",
  .name = "Certificate Transparency", .name_ja = "証明書透過ログ",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/certificate-transparency",
  .description = "Find subdomains from CT logs via crt.sh and certspotter.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(ct_logs_def)
