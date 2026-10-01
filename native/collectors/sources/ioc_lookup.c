/* collectors/osint/sources/ioc_lookup.c — IOC_LOOKUP, ported from OSINTsaas
 * threat_feed.c onto the JapanOSINT source ABI. Enriches an indicator of
 * compromise:
 *   - file hash (md5/sha1/sha256) → CIRCL hashlookup
 *   - ip / domain / url          → abuse.ch ThreatFox
 * CIRCL is free and keyless. ThreatFox is NO LONGER keyless — abuse.ch moved
 * every *-api.abuse.ch endpoint behind an Auth-Key (ABUSE_CH_AUTH_KEY), so
 * that leg is gated rather than silently returning "no match".
 * Honest-empty when the indicator is genuinely unknown. */
#include "lib/jocore.h"
#include "source.h"
#include "core/httpclient.h"
#include "third_party/cJSON.h"
#include "lib/threatintel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int hex_len(const char *s) {
  int n = 0; for (const char *p = s; *p; p++, n++) if (!isxdigit((unsigned char)*p)) return -1;
  return n;
}

static int emit(intel_sink *sink, const char *entity, const char *src, cJSON *data) {
  char *bj = cJSON_PrintUnformatted(data);
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "IOC_LOOKUP");
  cJSON_AddStringToObject(props, "entity", entity);
  char *pj = cJSON_PrintUnformatted(props);
  char rk[256], title[256];
  snprintf(rk, sizeof rk, "ioc:%s:%s", src, entity);
  snprintf(title, sizeof title, "IOC %s — %s", entity, src);
  intel_item it = {0};
  it.remote_key = rk; it.title = title; it.body = bj; it.summary = "ioc match";
  it.record_type = "osint_service_result"; it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"IOC_LOOKUP\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* AUDIT 2026-07-31: ioc_run used to return the number of rows it emitted.
 * core/scheduler.c does `status = rc == 0 ? "ok" : "error"` and feeds any
 * non-zero rc to anomaly_detect(), so a lookup that actually FOUND the
 * indicator was recorded as an errored run and quarantined the source, while
 * a lookup that found nothing looked healthy. run() is a status code. */
static int ioc_run(const source_ctx *ctx, intel_sink *sink) {
  const char *e = ctx->entity;
  if (!e || !*e) return 0;
  int hl = hex_len(e);

  if (hl == 32 || hl == 40 || hl == 64) {          /* file hash → CIRCL */
    const char *algo = hl == 32 ? "md5" : (hl == 40 ? "sha1" : "sha256");
    char url[256]; snprintf(url, sizeof url, "https://hashlookup.circl.lu/lookup/%s/%s", algo, e);
    const char *headers[] = { "Accept: application/json", NULL };
    http_response hr = {0};
    if (http_request(ctx->http, "GET", url, headers, NULL, 0, 12000, 1, &hr) != 0 ||
        hr.status != 200 || !hr.body) { http_response_free(&hr); return 0; }
    cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
    if (!j) return 0;
    if (cJSON_GetObjectItem(j, "message")) { cJSON_Delete(j); return 0; }  /* not found */
    cJSON_AddStringToObject(j, "source", "hashlookup.circl.lu");
    int r = emit(sink, e, "circl-hashlookup", j);
    fprintf(stderr, "[IOC_LOOKUP] circl-hashlookup emitted %d\n", r);
    cJSON_Delete(j);
    return 0;
  }

  /* ip / domain / url → ThreatFox.
   * The indicator is analyst-supplied and lands verbatim in a JSON literal; a
   * URL indicator with a `"` or a `\` in it (both legal in a path or query)
   * used to produce a malformed body that abuse.ch rejects — and an IOC check
   * that silently fails reads as "clean". Let cJSON do the escaping. */
  cJSON *qo = cJSON_CreateObject();
  cJSON_AddStringToObject(qo, "query", "search_ioc");
  cJSON_AddStringToObject(qo, "search_term", e);
  char *body = cJSON_PrintUnformatted(qo);
  cJSON_Delete(qo);
  if (!body) return 0;
  /* abuse.ch made Auth-Key mandatory on every *-api.abuse.ch endpoint
   * (measured 2026-08-01: this URL returns 401 without one). Unauthenticated
   * this lookup silently found nothing for every indicator — which for an IOC
   * check is the most dangerous possible failure, because "no hit" reads as
   * "clean". Gate honestly instead: no key -> say so once and return no
   * result, rather than manufacturing an all-clear. */
  const char *auth = abusech_auth_header();
  if (!auth) {
    fprintf(stderr, "[IOC_LOOKUP] threatfox skipped: no ABUSE_CH_AUTH_KEY "
                    "(abuse.ch requires one; an unauthenticated query would "
                    "return an empty result that looks like 'not malicious')\n");
    free(body);
    return 0;
  }
  const char *headers[] = { "Content-Type: application/json", auth, NULL };
  http_response hr = {0};
  if (http_request(ctx->http, "POST", "https://threatfox-api.abuse.ch/api/v1/",
                   headers, body, strlen(body), 12000, 1, &hr) != 0 ||
      hr.status != 200 || !hr.body) { http_response_free(&hr); free(body); return 0; }
  free(body);
  cJSON *j = cJSON_Parse(hr.body); http_response_free(&hr);
  if (!j) return 0;
  const char *status = jo_str(j, "query_status");
  cJSON *data = cJSON_GetObjectItem(j, "data");
  int emitted = 0;
  if (status && strcmp(status, "ok") == 0 && data && cJSON_IsArray(data)) {
    int n = cJSON_GetArraySize(data);
    for (int i = 0; i < n && i < 15; i++) {
      cJSON *d = cJSON_GetArrayItem(data, i);
      cJSON *out = cJSON_CreateObject();
      cJSON_AddStringToObject(out, "source", "threatfox.abuse.ch");
      const char *keep[] = {"ioc","threat_type","malware","malware_printable",
                            "confidence_level","first_seen","tags",NULL};
      for (int k = 0; keep[k]; k++) {
        cJSON *v = cJSON_GetObjectItem(d, keep[k]);
        if (v) cJSON_AddItemToObject(out, keep[k], cJSON_Duplicate(v, 1));
      }
      emitted += emit(sink, e, "threatfox", out);
      cJSON_Delete(out);
    }
  }
  fprintf(stderr, "[IOC_LOOKUP] threatfox emitted %d (query_status=%s)\n",
          emitted, status ? status : "(none)");
  cJSON_Delete(j);
  return 0;
}

static const source_def ioc_def = {
  .id = "IOC_LOOKUP", .collector = "osint", .name = "IOC Lookup", .run = ioc_run,
  .category = "investigation", .type = "api", .url = "https://threatfox.abuse.ch/",
  .description = "Indicator-of-compromise enrichment: hashes (CIRCL) + IP/domain/URL (ThreatFox), free.",
  .free_tier = 1,
};
REGISTER_SOURCE(ioc_def)
