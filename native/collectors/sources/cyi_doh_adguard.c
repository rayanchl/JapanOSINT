/* collectors/sources/cyi_doh_adguard.c
 * OSINT service — DOH_ADGUARD. Domain pivot (ctx->entity = a domain) resolved
 * through AdGuard's malware/phishing-FILTERING resolver, so the answer can be
 * diffed against the unfiltered resolvers the platform already queries: a
 * filtered domain comes back NXDOMAIN or 0.0.0.0.
 * Endpoint: https://dns.adguard-dns.com/resolve?name=<domain>&type=A  (keyless)
 * parse_notes: "Plain GET, no HTTP/2 requirement and no special Accept header
 * needed (returns Content-Type application/x-javascript)" — the body is parsed
 * regardless of MIME. (Quad9's JSON endpoint rejects HTTP/1.1 with 505, which
 * is why AdGuard is the filtered resolver used here.)
 * Emits ONE row: Status, the RFC-8484 Answer[] records (data/TTL/type) actually
 * returned, and a blocked verdict derived only from those returned values.
 * Non-domain / empty entity -> return 0 (no-op, not a failure). A 4xx from the
 * resolver is an honest "no answer" -> 0. No coordinates -> has_geo 0 (R2).
 * Licence: AdGuard public DNS, free, no key.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static int looks_like_domain(const char *s) {
  if (!s || !*s) return 0;
  if (strchr(s, '@') || strchr(s, '/') || strchr(s, ' ') || strchr(s, ':')) return 0;
  size_t n = strlen(s);
  if (n < 3 || n > 253) return 0;
  const char *dot = strrchr(s, '.');
  if (!dot || dot == s || !dot[1]) return 0;
  for (const char *p = s; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (!(isalnum(c) || c == '-' || c == '.' || c == '_' || c >= 0x80)) return 0;
  }
  if ((unsigned char)dot[1] < 0x80 && !isalpha((unsigned char)dot[1])) return 0;
  return 1;
}

static char *http_get(const source_ctx *ctx, const char *url, long *status) {
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 15000, 1, &hr);
  *status = hr.status;
  if (rc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  char *b = hr.body; hr.body = NULL; http_response_free(&hr);
  return b;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_domain(q)) return 0;          /* wrong shape -> no-op */

  char *enc = jo_urlencode(q);
  if (!enc) return -1;
  char url[512];
  snprintf(url, sizeof url,
           "https://dns.adguard-dns.com/resolve?name=%s&type=A", enc);
  free(enc);

  long status = 0;
  char *body = http_get(ctx, url, &status);
  if (!body) {
    fprintf(stderr, "[DOH_ADGUARD] http status=%ld\n", status);
    if (status >= 400 && status < 500) return 0;   /* honest "no answer" */
    return -1;                                     /* real fetch failure */
  }

  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[DOH_ADGUARD] unparseable body\n"); return -1; }

  const cJSON *st = cJSON_GetObjectItem(root, "Status");
  int rcode = cJSON_IsNumber(st) ? (int)st->valuedouble : -1;

  cJSON *answers = cJSON_CreateArray();
  int nanswers = 0, sinkholed = 0;
  const char *first = NULL;
  const cJSON *a;
  cJSON_ArrayForEach(a, cJSON_GetObjectItem(root, "Answer")) {
    const char *data = jo_sv(a, "data");
    if (!data) continue;
    cJSON *rec = cJSON_CreateObject();
    cJSON_AddStringToObject(rec, "data", data);
    const cJSON *ttl = cJSON_GetObjectItem(a, "TTL");
    if (cJSON_IsNumber(ttl)) cJSON_AddNumberToObject(rec, "ttl", ttl->valuedouble);
    const cJSON *ty = cJSON_GetObjectItem(a, "type");
    if (cJSON_IsNumber(ty)) cJSON_AddNumberToObject(rec, "type", ty->valuedouble);
    cJSON_AddItemToArray(answers, rec);
    if (!first) first = data;
    /* a filtering resolver answers a blocked name with the null route */
    if (strcmp(data, "0.0.0.0") == 0 || strcmp(data, "::") == 0) sinkholed = 1;
    nanswers++;
  }

  /* verdict derived only from what came back */
  const char *verdict = sinkholed ? "filtered (null-routed answer)"
                      : (rcode == 3) ? "filtered or nonexistent (NXDOMAIN)"
                      : (nanswers > 0) ? "resolved"
                      : "no answer records";

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "DOH_ADGUARD");
  cJSON_AddStringToObject(p, "resolver", "dns.adguard-dns.com");
  cJSON_AddStringToObject(p, "domain", q);
  if (rcode >= 0) cJSON_AddNumberToObject(p, "dns_status", rcode);
  cJSON_AddNumberToObject(p, "answer_count", nanswers);
  cJSON_AddBoolToObject(p, "null_routed", sinkholed);
  cJSON_AddStringToObject(p, "verdict", verdict);
  cJSON_AddItemToObject(p, "answers", answers);          /* ownership moves */
  const cJSON *ad = cJSON_GetObjectItem(root, "AD");
  if (cJSON_IsBool(ad)) cJSON_AddBoolToObject(p, "ad", cJSON_IsTrue(ad));
  const cJSON *cd = cJSON_GetObjectItem(root, "CD");
  if (cJSON_IsBool(cd)) cJSON_AddBoolToObject(p, "cd", cJSON_IsTrue(cd));
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char summary[320];
  snprintf(summary, sizeof summary, "AdGuard filtering resolver: %s%s%s",
           verdict, first ? " · " : "", first ? first : "");

  intel_item it = {0};
  it.remote_key      = q;
  it.title           = q;
  it.summary         = summary;
  it.link            = url;
  it.lang            = "en";
  it.record_type     = "doh-filtered-resolution";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"cyber\",\"dns\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(root);
  fprintf(stderr, "[DOH_ADGUARD] emitted 1 (%s, %d answers)\n", q, nanswers);
  return 0;
}

static const source_def cyi_doh_adguard_def = {
  .id = "DOH_ADGUARD", .collector = "osint",
  .name = "AdGuard filtered DNS resolution (DoH JSON)",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://dns.adguard-dns.com/resolve",
  .description = "Resolves a domain through AdGuard's malware/phishing-filtering resolver; a null-routed or NXDOMAIN answer against an unfiltered resolver's answer is the block signal.",
  .license = "AdGuard public DNS, free, no key.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cyi_doh_adguard_def)
