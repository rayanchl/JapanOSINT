/* collectors/sources/cyi_rpki_validity.c
 * OSINT service — RPKI_VALIDITY. ASN pivot: is a prefix legitimately announced
 * by this AS? Nothing else in the fleet does RPKI route-origin validation.
 * Endpoint: https://rpki-validator.ripe.net/api/v1/validity/<asn>/<prefix>
 * Keyless.
 * The pivot entity is an ASN, but the validator needs an (asn, prefix) pair, so:
 *   - "AS13335 1.1.1.0/24" (asn and prefix in one entity) is used directly;
 *   - a bare ASN has its announced prefixes enumerated first from RIPEstat
 *     (https://stat.ripe.net/data/announced-prefixes/data.json?resource=ASn,
 *     keyless), and the first MAX_PREFIXES of them are validated.
 * parse_notes: "ASN may be given bare (13335) or as AS13335" — both accepted.
 * "unmatched_as / unmatched_length arrays explain WHY a route is invalid —
 * carry them, an 'invalid' with unmatched_length is usually a config error
 * while unmatched_as is a possible hijack." Both arrays are carried verbatim.
 * Emits one row per validated prefix: origin ASN, prefix, validity state and
 * description, matched/unmatched VRPs, generatedTime.
 * No coordinates -> has_geo 0 (R2).
 * Licence: RIPE NCC public validator instance, keyless.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define MAX_PREFIXES 8

static unsigned long parse_asn_prefix(const char *s, char *prefix, size_t plen) {
  if (!s) return 0;
  prefix[0] = '\0';
  while (*s == ' ') s++;
  if ((s[0] == 'A' || s[0] == 'a') && (s[1] == 'S' || s[1] == 's')) s += 2;
  char *end = NULL;
  unsigned long v = strtoul(s, &end, 10);
  if (!end || end == s) return 0;
  if (v == 0 || v > 4294967295UL) return 0;
  while (*end == ' ' || *end == ',' || *end == ';') end++;
  if (*end) {
    /* the remainder must look like a prefix: digits/dots/colons/hex + '/' */
    if (!strchr(end, '/')) return 0;
    for (const char *q = end; *q; q++)
      if (!(isxdigit((unsigned char)*q) || *q == '.' || *q == ':' || *q == '/'))
        return 0;
    snprintf(prefix, plen, "%s", end);
  }
  return v;
}

static char *http_get(const source_ctx *ctx, const char *url, long *status) {
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  *status = hr.status;
  if (rc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  char *b = hr.body; hr.body = NULL; http_response_free(&hr);
  return b;
}

static int emit_validity(const source_ctx *ctx, intel_sink *sink,
                         unsigned long asn, const char *prefix) {
  char url[256];
  snprintf(url, sizeof url,
           "https://rpki-validator.ripe.net/api/v1/validity/%lu/%s", asn, prefix);
  long status = 0;
  char *body = http_get(ctx, url, &status);
  if (!body) return 0;                            /* validator had no answer */
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  const cJSON *vr = cJSON_GetObjectItem(root, "validated_route");
  const cJSON *route = cJSON_IsObject(vr) ? cJSON_GetObjectItem(vr, "route") : NULL;
  const cJSON *validity = cJSON_IsObject(vr) ? cJSON_GetObjectItem(vr, "validity") : NULL;
  if (!cJSON_IsObject(validity)) { cJSON_Delete(root); return 0; }

  const char *state = jo_sv(validity, "state");
  const char *desc  = jo_sv(validity, "description");
  const char *rpfx  = cJSON_IsObject(route) ? jo_sv(route, "prefix") : NULL;
  const char *gen   = jo_sv(root, "generatedTime");

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "RPKI_VALIDITY");
  cJSON_AddNumberToObject(p, "origin_asn", (double)asn);
  cJSON_AddStringToObject(p, "prefix", rpfx ? rpfx : prefix);
  if (state) cJSON_AddStringToObject(p, "rpki_state", state);
  if (desc)  cJSON_AddStringToObject(p, "rpki_description", desc);
  if (gen)   cJSON_AddStringToObject(p, "generated_time", gen);

  const cJSON *vrps = cJSON_GetObjectItem(validity, "VRPs");
  if (cJSON_IsObject(vrps)) {
    const cJSON *m = cJSON_GetObjectItem(vrps, "matched");
    if (cJSON_IsArray(m) && cJSON_GetArraySize(m) > 0)
      cJSON_AddItemToObject(p, "vrps_matched", cJSON_Duplicate(m, 1));
    /* unmatched_length => usually a maxLength config error;
     * unmatched_as     => a different origin holds a ROA: possible hijack */
    const cJSON *ua = cJSON_GetObjectItem(vrps, "unmatched_as");
    if (cJSON_IsArray(ua) && cJSON_GetArraySize(ua) > 0)
      cJSON_AddItemToObject(p, "vrps_unmatched_as", cJSON_Duplicate(ua, 1));
    const cJSON *ul = cJSON_GetObjectItem(vrps, "unmatched_length");
    if (cJSON_IsArray(ul) && cJSON_GetArraySize(ul) > 0)
      cJSON_AddItemToObject(p, "vrps_unmatched_length", cJSON_Duplicate(ul, 1));
  }
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char title[192];
  snprintf(title, sizeof title, "RPKI %s: AS%lu %s",
           state ? state : "unknown", asn, rpfx ? rpfx : prefix);
  char key[192];
  snprintf(key, sizeof key, "%lu|%s", asn, rpfx ? rpfx : prefix);

  intel_item it = {0};
  it.remote_key      = key;
  it.title           = title;
  it.summary         = desc ? desc : "RPKI route-origin validation";
  it.link            = url;
  it.lang            = "en";
  it.published_at    = gen;
  it.record_type     = "rpki-validity";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"cyber\",\"bgp\",\"rpki\"]";
  int rc = sink->emit(sink, &it);
  free(pj);
  cJSON_Delete(root);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char given[128];
  unsigned long asn = parse_asn_prefix(ctx->entity, given, sizeof given);
  if (!asn) return 0;                              /* wrong shape -> no-op */

  if (given[0]) {                                  /* explicit asn + prefix */
    int n = emit_validity(ctx, sink, asn, given);
    fprintf(stderr, "[RPKI_VALIDITY] emitted %d (AS%lu %s)\n", n, asn, given);
    return 0;
  }

  /* bare ASN: enumerate what it announces, then validate each */
  char url[192];
  snprintf(url, sizeof url,
           "https://stat.ripe.net/data/announced-prefixes/data.json?resource=AS%lu", asn);
  long status = 0;
  char *body = http_get(ctx, url, &status);
  if (!body) {
    fprintf(stderr, "[RPKI_VALIDITY] prefix enumeration status=%ld\n", status);
    if (status >= 400 && status < 500) return 0;
    return -1;
  }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[RPKI_VALIDITY] unparseable prefix list\n"); return -1; }

  const cJSON *pfxs = cJSON_GetObjectItem(
      cJSON_GetObjectItem(root, "data"), "prefixes");

  int n = 0, tried = 0;
  const cJSON *e;
  cJSON_ArrayForEach(e, pfxs) {
    if (tried >= MAX_PREFIXES) break;
    const char *pf = jo_sv(e, "prefix");
    if (!pf || !strchr(pf, '/')) continue;
    tried++;
    n += emit_validity(ctx, sink, asn, pf);
  }

  cJSON_Delete(root);
  fprintf(stderr, "[RPKI_VALIDITY] emitted %d of %d prefixes for AS%lu\n", n, tried, asn);
  return 0;                       /* an AS announcing nothing is not an error */
}

static const source_def cyi_rpki_validity_def = {
  .id = "RPKI_VALIDITY", .collector = "osint",
  .name = "RPKI route-origin validation (RIPE validator)",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "https://rpki-validator.ripe.net/api/v1/validity/",
  .description = "Is this prefix legitimately announced by this AS? valid/invalid/not-found plus the ROAs that matched or failed — the check that tells a hijack from a normal announcement.",
  .license = "RIPE NCC public validator instance, keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cyi_rpki_validity_def)
