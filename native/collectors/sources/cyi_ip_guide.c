/* collectors/sources/cyi_ip_guide.c
 * OSINT service — IP_GUIDE. IP pivot returning the announced network and its AS
 * in one keyless call — the routing answer rather than a marketing geolocation.
 * Fills the prefix half the ip-api.com-based lookups skip.
 * Endpoint: https://ip.guide/<ip>                                     (keyless)
 * TRAPS (parse_notes): "Content-Type is text/plain but the body is JSON — parse
 * regardless of MIME" (so the raw body is handed to cJSON_Parse), and "Location
 * may be absent entirely for infrastructure IPs (it was for 1.1.1.1): if there
 * is no location object, leave has_geo=0 (R2)." Coordinates are therefore taken
 * only from a present location object and range-checked before use.
 * Emits ONE row: ip, network.cidr, hosts.start/end, and the autonomous_system
 * asn/name/org/country.
 * Licence: ip.guide free API, no key, no stated commercial restriction.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static int looks_like_ip(const char *s) {
  if (!s || !*s) return 0;
  size_t n = strlen(s);
  if (n < 3 || n > 45) return 0;
  if (strchr(s, '/')) return 0;                  /* a bare address, not a CIDR */
  if (strchr(s, ':')) {                          /* IPv6 */
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

static char *http_get(const source_ctx *ctx, const char *url, long *status) {
  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 20000, 1, &hr);
  *status = hr.status;
  if (rc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  char *b = hr.body; hr.body = NULL; http_response_free(&hr);
  return b;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_ip(q)) return 0;                 /* wrong shape -> no-op */

  char url[128];
  snprintf(url, sizeof url, "https://ip.guide/%s", q);

  long status = 0;
  char *body = http_get(ctx, url, &status);
  if (!body) {
    fprintf(stderr, "[IP_GUIDE] http status=%ld\n", status);
    if (status >= 400 && status < 500) return 0;
    return -1;
  }
  /* Content-Type is text/plain; parse the body as JSON regardless. */
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) { fprintf(stderr, "[IP_GUIDE] unparseable body\n"); return -1; }

  const char *ip = jo_sv(root, "ip");
  const cJSON *net = cJSON_GetObjectItem(root, "network");
  const char *cidr = cJSON_IsObject(net) ? jo_sv(net, "cidr") : NULL;
  if (!ip && !cidr) {
    cJSON_Delete(root);
    fprintf(stderr, "[IP_GUIDE] nothing returned for %s\n", q);
    return 0;
  }

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "IP_GUIDE");
  cJSON_AddStringToObject(p, "ip", ip ? ip : q);
  if (cidr) cJSON_AddStringToObject(p, "cidr", cidr);

  const char *asname = NULL, *asorg = NULL, *ascc = NULL;
  double asn = 0; int hasn = 0;
  if (cJSON_IsObject(net)) {
    const cJSON *hosts = cJSON_GetObjectItem(net, "hosts");
    if (cJSON_IsObject(hosts)) {
      const char *hs = jo_sv(hosts, "start"), *he = jo_sv(hosts, "end");
      if (hs) cJSON_AddStringToObject(p, "hosts_start", hs);
      if (he) cJSON_AddStringToObject(p, "hosts_end", he);
    }
    const cJSON *as = cJSON_GetObjectItem(net, "autonomous_system");
    if (cJSON_IsObject(as)) {
      hasn = jo_numf(as, "asn", &asn);
      if (hasn) cJSON_AddNumberToObject(p, "asn", asn);
      if ((asname = jo_sv(as, "name")))   cJSON_AddStringToObject(p, "as_name", asname);
      if ((asorg = jo_sv(as, "org")))     cJSON_AddStringToObject(p, "as_org", asorg);
      if ((ascc = jo_sv(as, "country")))  cJSON_AddStringToObject(p, "as_country", ascc);
    }
  }

  intel_item it = {0};
  /* location is absent for many infrastructure IPs — no object, no geo (R2) */
  const cJSON *loc = cJSON_GetObjectItem(root, "location");
  if (cJSON_IsObject(loc)) {
    double lat = 0, lon = 0;
    if (jo_numf(loc, "latitude", &lat) && jo_numf(loc, "longitude", &lon) &&
        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0)) {
      it.has_geo = 1; it.lat = lat; it.lon = lon;
      cJSON_AddStringToObject(p, "geo_precision", "city");
    }
    const char *city = jo_sv(loc, "city"), *country = jo_sv(loc, "country");
    if (city)    cJSON_AddStringToObject(p, "city", city);
    if (country) cJSON_AddStringToObject(p, "country", country);
  }
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char asnbuf[32] = "";
  if (hasn) snprintf(asnbuf, sizeof asnbuf, "AS%.0f ", asn);
  char summary[384];
  snprintf(summary, sizeof summary, "%s%s%s%s%s%s",
           cidr ? cidr : "", cidr ? " · " : "",
           asnbuf, asname ? asname : (asorg ? asorg : ""),
           ascc ? " · " : "", ascc ? ascc : "");

  it.remote_key      = ip ? ip : q;
  it.title           = ip ? ip : q;
  it.summary         = summary;
  it.link            = url;
  it.lang            = "en";
  it.record_type     = "ip-network";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"cyber\",\"bgp\",\"ip\"]";
  sink->emit(sink, &it);

  free(pj);
  cJSON_Delete(root);
  fprintf(stderr, "[IP_GUIDE] emitted 1 (%s)\n", q);
  return 0;
}

static const source_def cyi_ip_guide_def = {
  .id = "IP_GUIDE", .collector = "osint",
  .name = "ip.guide network and routing lookup",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api", .url = "https://ip.guide/",
  .description = "The announced network and its AS for an IP in one keyless call — the routing answer rather than a marketing geolocation.",
  .license = "ip.guide free API, no key, no stated commercial restriction.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(cyi_ip_guide_def)
