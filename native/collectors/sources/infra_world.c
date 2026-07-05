/* collectors/sources/infra_world.c
 * OSINT services — global internet-infrastructure lookups. On-demand entity
 * pivot (ctx->entity = an IP address, an ASN, or an organisation name) against
 * the major KEYLESS, LIVE public infra APIs. One run() dispatches on
 * ctx->source_id; every branch REAL-fetches and emits only parsed fields, or
 * honest-empty (return 0). No key required by any of these endpoints.
 *
 *   SHODAN_INTERNETDB  internetdb.shodan.io/<ip>  — FREE keyless: open ports,
 *                      CPEs, detected vulns (CVE ids), hostnames, tags.
 *   BGPVIEW            api.bgpview.io — /ip/<ip>, /asn/<n>, /search?query_term=
 *                      resolves an IP→prefix/ASN, an ASN→details/prefixes, or a
 *                      free-text org query → matched ASNs.
 *   PEERINGDB_GLOBAL   peeringdb.com/api/net?name_search=<entity> — networks
 *                      (ASN, name, info_type, policy, IRR).
 *   IPAPI_GLOBAL       ip-api.com/json/<ip> — geo + ASN/org/ISP for an IP.
 *   RIPESTAT_GLOBAL    stat.ripe.net/data/... — network-info + as-overview for
 *                      an IP or ASN (RIPE NCC RIPEstat public data API).
 *
 * entity semantics: IPv4/IPv6 literal, an ASN ("AS13335" or "13335"), or an
 * org/name string. Each source picks the queries that make sense for the shape
 * of the entity it received; irrelevant shapes → honest empty. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- entity-shape helpers ---------------------------------------------- */

/* looks like an IPv4/IPv6 literal (digits/dots or hex/colons, no letters that
 * aren't hex). Conservative: good enough to route the pivot. */
static int iw_is_ip(const char *s) {
  if (!s || !*s) return 0;
  int dots = 0, colons = 0;
  for (const char *p = s; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (c == '.') dots++;
    else if (c == ':') colons++;
    else if (!isxdigit(c)) return 0;
  }
  return (dots == 3) || (colons >= 2);
}

/* parse an ASN out of "AS13335", "as13335" or "13335"; returns >0 or 0. */
static long iw_asn(const char *s) {
  if (!s || !*s) return 0;
  const char *p = s;
  if ((p[0]=='A'||p[0]=='a') && (p[1]=='S'||p[1]=='s')) p += 2;
  if (!*p) return 0;
  for (const char *q = p; *q; q++) if (!isdigit((unsigned char)*q)) return 0;
  long v = strtol(p, NULL, 10);
  return v > 0 ? v : 0;
}

/* cJSON number field → formatted into buf; returns buf or NULL. */
static const char *iw_num(const cJSON *o, const char *k, char *buf, size_t n) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v || !cJSON_IsNumber(v)) return NULL;
  snprintf(buf, n, "%.0f", v->valuedouble);
  return buf;
}

/* Emit a single infra record. body/props are printed & freed by the caller's
 * cJSON objects; here we just build the item. Returns 1 on emit. */
static int iw_emit(intel_sink *sink, const char *service, const char *rectype,
                   const char *remote_key, const char *title,
                   const char *summary, const char *link,
                   cJSON *data, cJSON *props) {
  char *bj = data ? cJSON_PrintUnformatted(data) : NULL;
  if (props) cJSON_AddStringToObject(props, "service", service);
  if (props) cJSON_AddBoolToObject(props, "success", 1);
  char *pj = props ? cJSON_PrintUnformatted(props) : NULL;

  intel_item it = {0};
  it.remote_key      = remote_key;
  it.title           = title;
  it.summary         = summary;
  it.body            = bj;
  it.lang            = "en";
  it.link            = link;
  it.record_type     = rectype;
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"infrastructure\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* join a cJSON string-array into "a, b, c" (bounded). buf always NUL-term. */
static void iw_join_arr(const cJSON *arr, char *buf, size_t n) {
  buf[0] = 0;
  if (!cJSON_IsArray(arr)) return;
  size_t len = 0; int first = 1;
  const cJSON *e;
  cJSON_ArrayForEach(e, arr) {
    char tmp[64];
    const char *s = NULL;
    if (cJSON_IsString(e) && e->valuestring) s = e->valuestring;
    else if (cJSON_IsNumber(e)) { snprintf(tmp, sizeof tmp, "%.0f", e->valuedouble); s = tmp; }
    if (!s) continue;
    size_t need = strlen(s) + (first ? 0 : 2);
    if (len + need >= n - 1) break;
    if (!first) { buf[len++] = ','; buf[len++] = ' '; }
    memcpy(buf + len, s, strlen(s)); len += strlen(s); buf[len] = 0;
    first = 0;
  }
}

/* ---- SHODAN InternetDB (keyless) --------------------------------------- *
 * GET https://internetdb.shodan.io/<ip> → { ip, ports[], cpes[], hostnames[],
 * tags[], vulns[] }. 404/empty for an unknown/clean IP → honest empty. */
static int iw_shodan(const source_ctx *ctx, intel_sink *sink, const char *ip) {
  char url[256];
  snprintf(url, sizeof url, "https://internetdb.shodan.io/%s", ip);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (infra)", NULL };
  char *body = jo_get(ctx, url, hdrs, "SHODAN_INTERNETDB");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;

  const char *rip = jo_sv(root, "ip");
  if (!rip) rip = ip;
  char ports[512], cpes[1024], hosts[512], tags[256], vulns[1024];
  iw_join_arr(cJSON_GetObjectItem(root, "ports"), ports, sizeof ports);
  iw_join_arr(cJSON_GetObjectItem(root, "cpes"), cpes, sizeof cpes);
  iw_join_arr(cJSON_GetObjectItem(root, "hostnames"), hosts, sizeof hosts);
  iw_join_arr(cJSON_GetObjectItem(root, "tags"), tags, sizeof tags);
  iw_join_arr(cJSON_GetObjectItem(root, "vulns"), vulns, sizeof vulns);

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "ip", rip);
  if (ports[0]) cJSON_AddStringToObject(data, "ports", ports);
  if (hosts[0]) cJSON_AddStringToObject(data, "hostnames", hosts);
  if (cpes[0])  cJSON_AddStringToObject(data, "cpes", cpes);
  if (tags[0])  cJSON_AddStringToObject(data, "tags", tags);
  if (vulns[0]) cJSON_AddStringToObject(data, "vulns", vulns);
  cJSON_AddStringToObject(data, "source", "Shodan InternetDB");

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "ip", rip);
  if (ports[0]) cJSON_AddStringToObject(props, "ports", ports);
  if (vulns[0]) cJSON_AddStringToObject(props, "vulns", vulns);

  char summary[640];
  snprintf(summary, sizeof summary, "%s%s%s%s",
           ports[0] ? "ports: " : "no open ports", ports[0] ? ports : "",
           vulns[0] ? " | vulns: " : "", vulns[0] ? vulns : "");
  char link[256];
  snprintf(link, sizeof link, "https://www.shodan.io/host/%s", rip);
  char key[128];
  snprintf(key, sizeof key, "internetdb|%s", rip);

  int em = iw_emit(sink, "SHODAN_INTERNETDB", "infra-host", key, rip,
                   summary, link, data, props);
  cJSON_Delete(data); cJSON_Delete(props);
  cJSON_Delete(root);
  fprintf(stderr, "[SHODAN_INTERNETDB] emitted %d\n", em);
  return em;
}

/* ---- ip-api.com (keyless) ---------------------------------------------- *
 * GET http://ip-api.com/json/<ip>?fields=... → geo + as/org/isp. */
static int iw_ipapi(const source_ctx *ctx, intel_sink *sink, const char *ip) {
  char url[320];
  snprintf(url, sizeof url,
    "http://ip-api.com/json/%s?fields=status,message,country,regionName,"
    "city,lat,lon,isp,org,as,asname,query,reverse", ip);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (infra)", NULL };
  char *body = jo_get(ctx, url, hdrs, "IPAPI_GLOBAL");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  const char *status = jo_sv(root, "status");
  if (status && strcmp(status, "success") != 0) { cJSON_Delete(root); return 0; }

  const char *q       = jo_sv(root, "query");
  const char *country = jo_sv(root, "country");
  const char *region  = jo_sv(root, "regionName");
  const char *city    = jo_sv(root, "city");
  const char *isp     = jo_sv(root, "isp");
  const char *org     = jo_sv(root, "org");
  const char *as      = jo_sv(root, "as");
  const char *asname  = jo_sv(root, "asname");
  const char *rev     = jo_sv(root, "reverse");
  if (!q) q = ip;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "ip", q);
  if (country) cJSON_AddStringToObject(data, "country", country);
  if (region)  cJSON_AddStringToObject(data, "region", region);
  if (city)    cJSON_AddStringToObject(data, "city", city);
  if (isp)     cJSON_AddStringToObject(data, "isp", isp);
  if (org)     cJSON_AddStringToObject(data, "org", org);
  if (as)      cJSON_AddStringToObject(data, "as", as);
  if (asname)  cJSON_AddStringToObject(data, "asname", asname);
  if (rev)     cJSON_AddStringToObject(data, "reverse_dns", rev);
  cJSON_AddStringToObject(data, "source", "ip-api.com");

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "ip", q);
  if (as)      cJSON_AddStringToObject(props, "as", as);
  if (country) cJSON_AddStringToObject(props, "country", country);

  const cJSON *jlat = cJSON_GetObjectItem(root, "lat");
  const cJSON *jlon = cJSON_GetObjectItem(root, "lon");

  char summary[512];
  snprintf(summary, sizeof summary, "%s%s%s%s%s%s%s",
           city ? city : "", city && country ? ", " : "",
           country ? country : "",
           as ? " (" : "", as ? as : "",
           asname ? " " : "", asname ? asname : "");
  char key[128];
  snprintf(key, sizeof key, "ip-api|%s", q);

  char *bj = cJSON_PrintUnformatted(data);
  cJSON_AddStringToObject(props, "service", "IPAPI_GLOBAL");
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  intel_item it = {0};
  it.remote_key      = key;
  it.title           = q;
  it.summary         = summary;
  it.body            = bj;
  it.lang            = "en";
  it.record_type     = "infra-ip-geo";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"infrastructure\"]";
  if (jlat && cJSON_IsNumber(jlat) && jlon && cJSON_IsNumber(jlon)) {
    it.has_geo = 1; it.lat = jlat->valuedouble; it.lon = jlon->valuedouble;
  }
  int em = sink->emit(sink, &it) >= 0 ? 1 : 0;
  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  cJSON_Delete(root);
  fprintf(stderr, "[IPAPI_GLOBAL] emitted %d\n", em);
  return em;
}

/* ---- BGPView (keyless) -------------------------------------------------- *
 * /ip/<ip>     → data.prefixes[] {prefix, ip, cidr, asn:{asn,name,description}}
 * /asn/<n>     → data {asn, name, description_short, country_code, ...}
 * /search?q=   → data.asns[] {asn, name, description, country_code}
 * All under { status:"ok", data:{...} }. */
static int iw_bgpview_ip(const source_ctx *ctx, intel_sink *sink, const char *ip) {
  char url[256];
  snprintf(url, sizeof url, "https://api.bgpview.io/ip/%s", ip);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (infra)", NULL };
  char *body = jo_get(ctx, url, hdrs, "BGPVIEW");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  cJSON *d = cJSON_GetObjectItem(root, "data");
  cJSON *prefixes = d ? cJSON_GetObjectItem(d, "prefixes") : NULL;
  int em = 0;
  if (cJSON_IsArray(prefixes)) {
    cJSON *p;
    cJSON_ArrayForEach(p, prefixes) {
      const char *pfx = jo_sv(p, "prefix");
      cJSON *asn = cJSON_GetObjectItem(p, "asn");
      char asbuf[32];
      const char *asnum = asn ? iw_num(asn, "asn", asbuf, sizeof asbuf) : NULL;
      const char *asname = asn ? jo_sv(asn, "name") : NULL;
      const char *asdesc = asn ? jo_sv(asn, "description") : NULL;
      if (!pfx && !asnum) continue;

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "ip", ip);
      if (pfx)    cJSON_AddStringToObject(data, "prefix", pfx);
      if (asnum)  cJSON_AddStringToObject(data, "asn", asnum);
      if (asname) cJSON_AddStringToObject(data, "as_name", asname);
      if (asdesc) cJSON_AddStringToObject(data, "as_description", asdesc);
      cJSON_AddStringToObject(data, "source", "BGPView");
      cJSON *props = cJSON_CreateObject();
      if (asnum) cJSON_AddStringToObject(props, "asn", asnum);
      if (pfx)   cJSON_AddStringToObject(props, "prefix", pfx);

      char title[256], key[160], link[128], summary[320];
      snprintf(title, sizeof title, "%s%s%s", ip, pfx ? " in " : "", pfx ? pfx : "");
      snprintf(summary, sizeof summary, "%s%s%s%s",
               asnum ? "AS" : "", asnum ? asnum : "",
               asname ? " " : "", asname ? asname : (asdesc ? asdesc : ""));
      snprintf(key, sizeof key, "bgpview|ip|%s|%s", ip, pfx ? pfx : "");
      snprintf(link, sizeof link, "https://bgpview.io/ip/%s", ip);
      em += iw_emit(sink, "BGPVIEW", "infra-prefix", key, title, summary, link,
                    data, props);
      cJSON_Delete(data); cJSON_Delete(props);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[BGPVIEW/ip] emitted %d\n", em);
  return em;
}

static int iw_bgpview_asn(const source_ctx *ctx, intel_sink *sink, long asn) {
  char url[128];
  snprintf(url, sizeof url, "https://api.bgpview.io/asn/%ld", asn);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (infra)", NULL };
  char *body = jo_get(ctx, url, hdrs, "BGPVIEW");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  cJSON *d = cJSON_GetObjectItem(root, "data");
  if (!d) { cJSON_Delete(root); return 0; }
  const char *name = jo_sv(d, "name");
  const char *desc = jo_sv(d, "description_short");
  if (!desc) desc = jo_sv(d, "description_full");
  const char *cc = jo_sv(d, "country_code");
  const char *web = jo_sv(d, "website");
  if (!name && !desc) { cJSON_Delete(root); return 0; }

  cJSON *data = cJSON_CreateObject();
  char asbuf[24]; snprintf(asbuf, sizeof asbuf, "%ld", asn);
  cJSON_AddStringToObject(data, "asn", asbuf);
  if (name) cJSON_AddStringToObject(data, "name", name);
  if (desc) cJSON_AddStringToObject(data, "description", desc);
  if (cc)   cJSON_AddStringToObject(data, "country_code", cc);
  if (web)  cJSON_AddStringToObject(data, "website", web);
  cJSON_AddStringToObject(data, "source", "BGPView");
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "asn", asbuf);
  if (cc) cJSON_AddStringToObject(props, "country_code", cc);

  char title[256], key[64], link[128];
  snprintf(title, sizeof title, "AS%ld %s", asn, name ? name : (desc ? desc : ""));
  snprintf(key, sizeof key, "bgpview|asn|%ld", asn);
  snprintf(link, sizeof link, "https://bgpview.io/asn/%ld", asn);
  int em = iw_emit(sink, "BGPVIEW", "infra-asn", key, title, desc, link,
                   data, props);
  cJSON_Delete(data); cJSON_Delete(props);
  cJSON_Delete(root);
  fprintf(stderr, "[BGPVIEW/asn] emitted %d\n", em);
  return em;
}

static int iw_bgpview_search(const source_ctx *ctx, intel_sink *sink,
                             const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url, "https://api.bgpview.io/search?query_term=%s", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (infra)", NULL };
  char *body = jo_get(ctx, url, hdrs, "BGPVIEW");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  cJSON *d = cJSON_GetObjectItem(root, "data");
  cJSON *asns = d ? cJSON_GetObjectItem(d, "asns") : NULL;
  int em = 0, n = 0;
  if (cJSON_IsArray(asns)) {
    cJSON *a;
    cJSON_ArrayForEach(a, asns) {
      if (n++ >= 25) break;
      char asbuf[24];
      const char *asnum = iw_num(a, "asn", asbuf, sizeof asbuf);
      const char *name = jo_sv(a, "name");
      const char *desc = jo_sv(a, "description");
      const char *cc = jo_sv(a, "country_code");
      if (!asnum) continue;

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "asn", asnum);
      if (name) cJSON_AddStringToObject(data, "name", name);
      if (desc) cJSON_AddStringToObject(data, "description", desc);
      if (cc)   cJSON_AddStringToObject(data, "country_code", cc);
      cJSON_AddStringToObject(data, "matched_query", q);
      cJSON_AddStringToObject(data, "source", "BGPView");
      cJSON *props = cJSON_CreateObject();
      cJSON_AddStringToObject(props, "asn", asnum);
      if (cc) cJSON_AddStringToObject(props, "country_code", cc);

      char title[256], key[64], link[128];
      snprintf(title, sizeof title, "AS%s %s", asnum, name ? name : (desc ? desc : ""));
      snprintf(key, sizeof key, "bgpview|search|%s", asnum);
      snprintf(link, sizeof link, "https://bgpview.io/asn/%s", asnum);
      em += iw_emit(sink, "BGPVIEW", "infra-asn", key, title, desc, link,
                    data, props);
      cJSON_Delete(data); cJSON_Delete(props);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[BGPVIEW/search] emitted %d\n", em);
  return em;
}

/* ---- PeeringDB global (keyless read) ----------------------------------- *
 * GET https://www.peeringdb.com/api/net?name_search=<entity> →
 * { data: [ { id, name, asn, info_type, policy_general, irr_as_set,
 *   website, ... } ] }. */
static int iw_peeringdb(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
    "https://www.peeringdb.com/api/net?name_search=%s", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (infra)", NULL };
  char *body = jo_get(ctx, url, hdrs, "PEERINGDB_GLOBAL");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  cJSON *arr = cJSON_GetObjectItem(root, "data");
  int em = 0, n = 0;
  if (cJSON_IsArray(arr)) {
    cJSON *r;
    cJSON_ArrayForEach(r, arr) {
      if (n++ >= 25) break;
      const char *name = jo_sv(r, "name");
      char asbuf[24];
      const char *asn = iw_num(r, "asn", asbuf, sizeof asbuf);
      const char *itype = jo_sv(r, "info_type");
      const char *policy = jo_sv(r, "policy_general");
      const char *irr = jo_sv(r, "irr_as_set");
      const char *web = jo_sv(r, "website");
      char idbuf[24];
      const char *pid = iw_num(r, "id", idbuf, sizeof idbuf);
      if (!name && !asn) continue;

      cJSON *data = cJSON_CreateObject();
      if (name)  cJSON_AddStringToObject(data, "name", name);
      if (asn)   cJSON_AddStringToObject(data, "asn", asn);
      if (itype) cJSON_AddStringToObject(data, "info_type", itype);
      if (policy)cJSON_AddStringToObject(data, "policy_general", policy);
      if (irr)   cJSON_AddStringToObject(data, "irr_as_set", irr);
      if (web)   cJSON_AddStringToObject(data, "website", web);
      cJSON_AddStringToObject(data, "source", "PeeringDB");
      cJSON *props = cJSON_CreateObject();
      if (asn)   cJSON_AddStringToObject(props, "asn", asn);
      if (itype) cJSON_AddStringToObject(props, "info_type", itype);

      char title[256], key[64], link[128], summary[320];
      snprintf(title, sizeof title, "%s%s%s%s", name ? name : "",
               asn ? " (AS" : "", asn ? asn : "", asn ? ")" : "");
      snprintf(summary, sizeof summary, "%s%s%s",
               itype ? itype : "", (itype && policy) ? " | policy: " : "",
               policy ? policy : "");
      snprintf(key, sizeof key, "peeringdb|net|%s", pid ? pid : (asn ? asn : name));
      if (pid) snprintf(link, sizeof link, "https://www.peeringdb.com/net/%s", pid);
      else snprintf(link, sizeof link, "https://www.peeringdb.com/");
      em += iw_emit(sink, "PEERINGDB_GLOBAL", "infra-network", key, title,
                    summary[0] ? summary : NULL, link, data, props);
      cJSON_Delete(data); cJSON_Delete(props);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[PEERINGDB_GLOBAL] emitted %d\n", em);
  return em;
}

/* ---- RIPEstat global (keyless) ----------------------------------------- *
 * IP  → /data/network-info/data.json?resource=<ip> → data{prefix, asns[]}
 * ASN → /data/as-overview/data.json?resource=AS<n> → data{holder, announced,
 *        type, block{...}} */
static int iw_ripestat_ip(const source_ctx *ctx, intel_sink *sink, const char *ip) {
  char url[320];
  snprintf(url, sizeof url,
    "https://stat.ripe.net/data/network-info/data.json?resource=%s", ip);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (infra)", NULL };
  char *body = jo_get(ctx, url, hdrs, "RIPESTAT_GLOBAL");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  cJSON *d = cJSON_GetObjectItem(root, "data");
  const char *prefix = d ? jo_sv(d, "prefix") : NULL;
  char asns[256]; asns[0] = 0;
  if (d) iw_join_arr(cJSON_GetObjectItem(d, "asns"), asns, sizeof asns);
  if (!prefix && !asns[0]) { cJSON_Delete(root); return 0; }

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "ip", ip);
  if (prefix) cJSON_AddStringToObject(data, "prefix", prefix);
  if (asns[0])cJSON_AddStringToObject(data, "asns", asns);
  cJSON_AddStringToObject(data, "source", "RIPEstat");
  cJSON *props = cJSON_CreateObject();
  if (prefix) cJSON_AddStringToObject(props, "prefix", prefix);
  if (asns[0])cJSON_AddStringToObject(props, "asns", asns);

  char title[256], key[128], link[256], summary[320];
  snprintf(title, sizeof title, "%s%s%s", ip, prefix ? " in " : "", prefix ? prefix : "");
  snprintf(summary, sizeof summary, "%s%s", asns[0] ? "AS " : "", asns[0] ? asns : "");
  snprintf(key, sizeof key, "ripestat|netinfo|%s", ip);
  snprintf(link, sizeof link,
           "https://stat.ripe.net/app/launchpad/S1_A1_ADHOC/%s", ip);
  int em = iw_emit(sink, "RIPESTAT_GLOBAL", "infra-prefix", key, title,
                   summary[0] ? summary : NULL, link, data, props);
  cJSON_Delete(data); cJSON_Delete(props);
  cJSON_Delete(root);
  fprintf(stderr, "[RIPESTAT_GLOBAL/ip] emitted %d\n", em);
  return em;
}

static int iw_ripestat_asn(const source_ctx *ctx, intel_sink *sink, long asn) {
  char url[256];
  snprintf(url, sizeof url,
    "https://stat.ripe.net/data/as-overview/data.json?resource=AS%ld", asn);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0 (infra)", NULL };
  char *body = jo_get(ctx, url, hdrs, "RIPESTAT_GLOBAL");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  cJSON *d = cJSON_GetObjectItem(root, "data");
  if (!d) { cJSON_Delete(root); return 0; }
  const char *holder = jo_sv(d, "holder");
  const char *type = jo_sv(d, "type");
  const cJSON *announced = cJSON_GetObjectItem(d, "announced");
  cJSON *block = cJSON_GetObjectItem(d, "block");
  const char *bdesc = block ? jo_sv(block, "desc") : NULL;
  if (!holder) { cJSON_Delete(root); return 0; }

  cJSON *data = cJSON_CreateObject();
  char asbuf[24]; snprintf(asbuf, sizeof asbuf, "%ld", asn);
  cJSON_AddStringToObject(data, "asn", asbuf);
  cJSON_AddStringToObject(data, "holder", holder);
  if (type)  cJSON_AddStringToObject(data, "type", type);
  if (announced && cJSON_IsBool(announced))
    cJSON_AddBoolToObject(data, "announced", cJSON_IsTrue(announced));
  if (bdesc) cJSON_AddStringToObject(data, "block", bdesc);
  cJSON_AddStringToObject(data, "source", "RIPEstat");
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "asn", asbuf);
  if (type) cJSON_AddStringToObject(props, "type", type);

  char title[256], key[64], link[256];
  snprintf(title, sizeof title, "AS%ld %s", asn, holder);
  snprintf(key, sizeof key, "ripestat|asoverview|%ld", asn);
  snprintf(link, sizeof link,
           "https://stat.ripe.net/app/launchpad/S1_A1_ADHOC/AS%ld", asn);
  int em = iw_emit(sink, "RIPESTAT_GLOBAL", "infra-asn", key, title,
                   bdesc, link, data, props);
  cJSON_Delete(data); cJSON_Delete(props);
  cJSON_Delete(root);
  fprintf(stderr, "[RIPESTAT_GLOBAL/asn] emitted %d\n", em);
  return em;
}

/* ---- dispatch ----------------------------------------------------------- */
static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  if (!q) return -1;
  const char *id = ctx->source_id;
  int is_ip = iw_is_ip(q);
  long asn = iw_asn(q);

  if (strcmp(id, "SHODAN_INTERNETDB") == 0) {
    if (!is_ip) { fprintf(stderr, "[SHODAN_INTERNETDB] entity not an IP\n"); return 0; }
    iw_shodan(ctx, sink, q);
    return 0;
  }
  if (strcmp(id, "IPAPI_GLOBAL") == 0) {
    if (!is_ip) { fprintf(stderr, "[IPAPI_GLOBAL] entity not an IP\n"); return 0; }
    iw_ipapi(ctx, sink, q);
    return 0;
  }
  if (strcmp(id, "BGPVIEW") == 0) {
    if (is_ip) iw_bgpview_ip(ctx, sink, q);
    else if (asn) iw_bgpview_asn(ctx, sink, asn);
    else iw_bgpview_search(ctx, sink, q);
    return 0;
  }
  if (strcmp(id, "PEERINGDB_GLOBAL") == 0) {
    /* name_search accepts free text; also fine for an ASN string */
    iw_peeringdb(ctx, sink, q);
    return 0;
  }
  if (strcmp(id, "RIPESTAT_GLOBAL") == 0) {
    if (is_ip) iw_ripestat_ip(ctx, sink, q);
    else if (asn) iw_ripestat_asn(ctx, sink, asn);
    else fprintf(stderr, "[RIPESTAT_GLOBAL] entity not an IP/ASN\n");
    return 0;
  }
  return -1;
}

#define INFRA_DEF(SYM, ID, NAME, NAMEJA, URL, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", \
    .name = NAME, .name_ja = NAMEJA, .update_interval_sec = 0, .run = run, \
    .category = "cyber", .type = "api", .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(SYM)

INFRA_DEF(iw_shodan_def, "SHODAN_INTERNETDB",
  "Shodan InternetDB", "Shodan InternetDB",
  "https://internetdb.shodan.io/",
  "Shodan InternetDB (keyless) — open ports, CPEs, CVEs, hostnames & tags for an IP");
INFRA_DEF(iw_bgpview_def, "BGPVIEW",
  "BGPView IP/ASN Lookup", "BGPView IP/ASN 検索",
  "https://bgpview.io/",
  "BGPView (keyless) — resolve an IP to prefix/ASN, an ASN to details, or org text to ASNs");
INFRA_DEF(iw_peeringdb_def, "PEERINGDB_GLOBAL",
  "PeeringDB Network Search", "PeeringDB ネットワーク検索",
  "https://www.peeringdb.com/",
  "PeeringDB global (keyless) — networks by name: ASN, type, peering policy, IRR set");
INFRA_DEF(iw_ipapi_def, "IPAPI_GLOBAL",
  "ip-api Geolocation", "ip-api IP 位置情報",
  "https://ip-api.com/",
  "ip-api.com (keyless) — geolocation + ASN/org/ISP + reverse DNS for an IP");
INFRA_DEF(iw_ripestat_def, "RIPESTAT_GLOBAL",
  "RIPEstat Network Data", "RIPEstat ネットワークデータ",
  "https://stat.ripe.net/",
  "RIPEstat (keyless) — network-info for an IP or AS-overview for an ASN (RIPE NCC)");
