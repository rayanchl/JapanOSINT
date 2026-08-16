/* collectors/sources/netintel_world2.c
 * OSINT services — net-new keyless network/vuln pivots on ctx->entity. Shared
 * run() switches on ctx->source_id. Real fetch or honest-empty. No API keys.
 *
 *   GREYNOISE_COMMUNITY  api.greynoise.io/v3/community/<ip>              (JSON)
 *   REDHAT_CVE           access.redhat.com/hydra/rest/securitydata/cve/<id>.json */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "_jp_osint.inc"

static const char *NI_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

/* GreyNoise Community answers a CLEAN ip with HTTP 404 and a fully-formed
 * JSON body: {"ip":…,"noise":false,"riot":false,"message":"IP not observed
 * scanning the internet."}. That 404 is the ANSWER, not an error — jo_get()
 * only accepts 200, so every quiet IP (the common case) silently produced
 * zero rows and the source looked dead. Do the request here so 404 is kept. */
static char *gn_get(const source_ctx *ctx, const char *url) {
  const char *hdrs[] = { "Accept: application/json", NI_UA, NULL };
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 20000, 1, &hr);
  if (rc != 0 || !hr.body || (hr.status != 200 && hr.status != 404)) {
    fprintf(stderr, "[GREYNOISE_COMMUNITY] http status=%ld\n", hr.status);
    http_response_free(&hr);
    return NULL;
  }
  char *body = hr.body;
  hr.body = NULL;
  http_response_free(&hr);
  return body;
}

static int run_greynoise(const source_ctx *ctx, intel_sink *sink, const char *enc) {
  char url[512];
  snprintf(url, sizeof url, "https://api.greynoise.io/v3/community/%s", enc);
  char *body = gn_get(ctx, url);
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *ip = jo_sv(root, "ip");
  if (!ip) { cJSON_Delete(root); return 0; }
  const cJSON *noise = cJSON_GetObjectItem(root, "noise");
  const cJSON *riot = cJSON_GetObjectItem(root, "riot");
  const char *classification = jo_sv(root, "classification");
  const char *name = jo_sv(root, "name");
  const char *last = jo_sv(root, "last_seen");
  const char *link = jo_sv(root, "link");
  const char *msg = jo_sv(root, "message");
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "ip", ip);
  if (cJSON_IsBool(noise)) cJSON_AddBoolToObject(d, "noise", cJSON_IsTrue(noise));
  if (cJSON_IsBool(riot)) cJSON_AddBoolToObject(d, "riot", cJSON_IsTrue(riot));
  if (classification) cJSON_AddStringToObject(d, "classification", classification);
  if (name) cJSON_AddStringToObject(d, "name", name);
  if (last) cJSON_AddStringToObject(d, "last_seen", last);
  if (msg) cJSON_AddStringToObject(d, "message", msg);
  cJSON_AddStringToObject(d, "source", "GreyNoise Community");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "GREYNOISE_COMMUNITY");
  cJSON_AddStringToObject(p, "ip", ip);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char title[256];
  if (classification)
    snprintf(title, sizeof title, "%s — %s%s%s", ip, classification,
             name ? " · " : "", name ? name : "");
  else if (cJSON_IsBool(noise) && !cJSON_IsTrue(noise))
    snprintf(title, sizeof title, "%s — not observed scanning (GreyNoise)", ip);
  else
    snprintf(title, sizeof title, "%s — unknown", ip);
  intel_item it = {0};
  it.remote_key = ip; it.title = title;
  it.summary = classification ? classification : msg;
  it.body = bj; it.link = link; it.lang = "en";
  it.record_type = "ip-reputation";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"GREYNOISE_COMMUNITY\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int is_cve_id(const char *s) {
  /* CVE-YYYY-NNNN.. (case-insensitive) */
  if (strncasecmp(s, "CVE-", 4) != 0) return 0;
  return 1;
}
static int run_redhat_cve(const source_ctx *ctx, intel_sink *sink, const char *raw) {
  if (!is_cve_id(raw)) return 0;
  char enc[64]; { char *e = jo_urlencode(raw); if (!e) return 0; snprintf(enc, sizeof enc, "%s", e); free(e); }
  char url[512];
  snprintf(url, sizeof url, "https://access.redhat.com/hydra/rest/securitydata/cve/%s.json", enc);
  const char *hdrs[] = { "Accept: application/json", NI_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "REDHAT_CVE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const char *name = jo_sv(root, "name");
  if (!name) { cJSON_Delete(root); return 0; }
  const char *sev = jo_sv(root, "threat_severity");
  const char *pubdate = jo_sv(root, "public_date");
  const cJSON *bugzilla = cJSON_GetObjectItem(root, "bugzilla");
  const char *desc = bugzilla ? jo_sv(bugzilla, "description") : NULL;
  const cJSON *cvss3 = cJSON_GetObjectItem(root, "cvss3");
  const char *score = cvss3 ? jo_sv(cvss3, "cvss3_base_score") : NULL;
  cJSON *d = cJSON_CreateObject();
  cJSON_AddStringToObject(d, "cve", name);
  if (sev) cJSON_AddStringToObject(d, "severity", sev);
  if (score) cJSON_AddStringToObject(d, "cvss3_score", score);
  if (desc) cJSON_AddStringToObject(d, "description", desc);
  cJSON_AddStringToObject(d, "source", "Red Hat Security Data");
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "REDHAT_CVE");
  cJSON_AddStringToObject(p, "cve", name);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
  char link[256], title[300];
  snprintf(link, sizeof link, "https://access.redhat.com/security/cve/%s", name);
  snprintf(title, sizeof title, "%s%s%s", name, sev ? " — " : "", sev ? sev : "");
  intel_item it = {0};
  it.remote_key = name; it.title = title; it.summary = sev;
  it.body = bj; it.link = link; it.lang = "en"; it.published_at = pubdate;
  it.record_type = "cve";
  it.properties_json = pj; it.tags_json = "[\"osint-search\",\"REDHAT_CVE\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "GREYNOISE_COMMUNITY")) n = run_greynoise(ctx, sink, enc);
  else if (!strcmp(sid, "REDHAT_CVE"))          n = run_redhat_cve(ctx, sink, q);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define NI_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "cyber", \
    .type = "api", .url = "internal://osint/netintel", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

NI_DEF(ni_greynoise_def, "GREYNOISE_COMMUNITY", "GreyNoise Community IP",
  "GreyNoise keyless community: is an IP a known scanner/benign (classification, name).");
NI_DEF(ni_redhat_def, "REDHAT_CVE", "Red Hat CVE Lookup",
  "Red Hat keyless security data for a CVE id (severity, CVSS3, description).");
