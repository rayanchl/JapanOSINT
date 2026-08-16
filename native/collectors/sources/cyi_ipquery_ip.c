/* collectors/sources/cyi_ipquery_ip.c
 * OSINT service — IPQUERY_IP. IP pivot returning proxy/VPN/Tor/datacentre
 * classification alongside ASN and coordinates, with no key — the risk block
 * the existing ip-api.com lookups do not provide and AbuseIPDB/IPQualityScore
 * charge for.
 * Endpoint: https://api.ipquery.io/<ip>                               (keyless)
 * parse_notes: "Coordinates are upstream values so has_geo is legitimate (R2)"
 * — they are still range-checked and only used when they parse as real numbers.
 * "risk_score 0 with is_datacenter true (seen on a known Tor exit) shows the
 * flags disagree with the score — carry the INDIVIDUAL BOOLEANS, not just the
 * score." Every flag is emitted separately for exactly that reason.
 * Emits ONE row: ip, isp asn/org/isp, location country/city, and
 * is_vpn/is_tor/is_proxy/is_datacenter/is_mobile/risk_score.
 * Licence: ipquery.io free API, keyless, no stated rate limit.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static int looks_like_ip(const char *s) {
  if (!s || !*s) return 0;
  size_t n = strlen(s);
  if (n < 3 || n > 45) return 0;
  if (strchr(s, '/')) return 0;
  if (strchr(s, ':')) {
    for (const char *q = s; *q; q++)
      if (!(isxdigit((unsigned char)*q) || *q == ':')) return 0;
    return 1;
  }
  int parts = 0;
  const char *p = s;
  while (*p) {
    int v = 0, d = 0;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; d++; if (v > 255) return 0; }
    if (!d || d > 3) return 0;
    parts++;
    if (*p == '.') { p++; if (!*p) return 0; }
    else if (*p) return 0;
  }
  return parts == 4;
}

static void copy_bool(cJSON *dst, const cJSON *src, const char *k, const char *as,
                      int *out_flag) {
  const cJSON *v = cJSON_GetObjectItem(src, k);
  if (!cJSON_IsBool(v)) return;
  int t = cJSON_IsTrue(v);
  cJSON_AddBoolToObject(dst, as, t);
  if (out_flag && t) *out_flag = 1;
}

static char *http_get(const source_ctx *ctx, const char *url, long *status) {
  http_response hr = {0};
  const char *hdrs[] = { "Accept: application/json", NULL };
  int rc = http_request(ctx->http, "GET", url, hdrs, NULL, 0, 20000, 1, &hr);
  *status = hr.status;
  if (rc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  char *b = hr.body; hr.body = NULL; http_response_free(&hr);
  return b;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_ip(q)) return 0;                 /* wrong shape -> no-op */

  char url[128];
  snprintf(url, sizeof url, "https://api.ipquery.io/%s", q);

  long status = 0;
  char *body = http_get(ctx, url, &status);
  if (!body) {
    fprintf(stderr, "[IPQUERY_IP] http status=%ld\n", status);
    if (status >= 400 && status < 500) return 0;
    return -1;
  }
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[IPQUERY_IP] unparseable body\n"); return -1; }

  const char *ip = jo_sv(root, "ip");
  const cJSON *isp = cJSON_GetObjectItem(root, "isp");
  const cJSON *loc = cJSON_GetObjectItem(root, "location");
  const cJSON *risk = cJSON_GetObjectItem(root, "risk");
  if (!ip && !cJSON_IsObject(isp) && !cJSON_IsObject(loc)) {
    cJSON_Delete(root);
    fprintf(stderr, "[IPQUERY_IP] nothing returned for %s\n", q);
    return 0;
  }

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "IPQUERY_IP");
  cJSON_AddStringToObject(p, "ip", ip ? ip : q);

  const char *asn = NULL, *org = NULL;
  if (cJSON_IsObject(isp)) {
    if ((asn = jo_sv(isp, "asn"))) cJSON_AddStringToObject(p, "asn", asn);
    if ((org = jo_sv(isp, "org"))) cJSON_AddStringToObject(p, "org", org);
    const char *ispn = jo_sv(isp, "isp");
    if (ispn) cJSON_AddStringToObject(p, "isp", ispn);
  }
  const char *city = NULL, *country = NULL;
  if (cJSON_IsObject(loc)) {
    if ((country = jo_sv(loc, "country"))) cJSON_AddStringToObject(p, "country", country);
    if ((city = jo_sv(loc, "city")))       cJSON_AddStringToObject(p, "city", city);
    const char *state = jo_sv(loc, "state");
    if (state) cJSON_AddStringToObject(p, "state", state);
  }

  int flagged = 0;
  double score = 0; int hscore = 0;
  if (cJSON_IsObject(risk)) {
    copy_bool(p, risk, "is_vpn", "is_vpn", &flagged);
    copy_bool(p, risk, "is_tor", "is_tor", &flagged);
    copy_bool(p, risk, "is_proxy", "is_proxy", &flagged);
    copy_bool(p, risk, "is_datacenter", "is_datacenter", &flagged);
    copy_bool(p, risk, "is_mobile", "is_mobile", NULL);
    hscore = jo_numf(risk, "risk_score", &score);
    if (hscore) cJSON_AddNumberToObject(p, "risk_score", score);
  }

  intel_item it = {0};
  if (cJSON_IsObject(loc)) {
    double lat = 0, lon = 0;
    if (jo_numf(loc, "latitude", &lat) && jo_numf(loc, "longitude", &lon) &&
        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0)) {
      it.has_geo = 1; it.lat = lat; it.lon = lon;
      cJSON_AddStringToObject(p, "geo_precision", "city");
    }
  }
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char summary[416];
  snprintf(summary, sizeof summary, "%s%s%s%s%s%s%s",
           asn ? asn : "", (asn && org) ? " " : "", org ? org : "",
           city ? " · " : "", city ? city : "",
           country ? ", " : "", country ? country : "");
  if (flagged) {
    size_t l = strlen(summary);
    snprintf(summary + l, sizeof summary - l, " · risk flags set");
  }

  it.remote_key      = ip ? ip : q;
  it.title           = ip ? ip : q;
  it.summary         = summary;
  it.link            = url;
  it.lang            = "en";
  it.record_type     = "ip-intelligence";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"cyber\",\"ip\",\"reputation\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(root);
  fprintf(stderr, "[IPQUERY_IP] emitted 1 (%s)\n", q);
  return 0;
}

static const source_def cyi_ipquery_ip_def = {
  .id = "IPQUERY_IP", .collector = "osint",
  .name = "ipquery.io IP intelligence with risk flags",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api", .url = "https://api.ipquery.io/",
  .description = "Keyless IP lookup returning proxy/VPN/Tor/datacentre classification alongside ASN and coordinates — the risk block ip-api.com does not provide.",
  .license = "ipquery.io free API, keyless, no stated rate limit.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cyi_ipquery_ip_def)
