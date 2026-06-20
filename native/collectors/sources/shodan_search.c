/* collectors/osint/sources/shodan_search.c
 * OSINT service — faithful port of OSINTsaas osint_tools/shodan_search.c
 * (handle_shodan_search). Canonical SERVICE name in osint_dispatcher.c
 * service_registry[] bound to handle_shodan_search is "SHODAN_SEARCH"
 * (line 214). On-demand (interval 0); dispatcher runs it with ctx->entity =
 * an IP or domain (single entity → upstream entity loop runs once).
 *
 * Reproduces handle_shodan_search() faithfully: emails are skipped; for an IP
 * it queries InternetDB (https://internetdb.shodan.io/<ip>, FREE — no key)
 * for ports/hostnames/cpes/vulns/tags and, when SHODAN_API_KEY is set, the
 * authenticated /shodan/host/<ip> endpoint; for a domain it uses the API DNS
 * resolve then the resolved IP (key only). This service is NOT key-only
 * (InternetDB is the no-key path), so it does NOT return 0 when the key is
 * absent.
 *
 * PER-RECORD EMIT: instead of one summary blob, this explodes the real fetched
 * data into distinct intel_items:
 *   - one item per OPEN PORT      → remote_key "shodan:<ip>:port:<port>"
 *   - one item per VULN (CVE)     → remote_key "shodan:<ip>:vuln:<cve>"
 *   - one host-summary item       → remote_key "shodan:<ip>"  (real hostnames/
 *                                    tags/org — only when there is something
 *                                    real to carry)
 * All ports/vulns/hostnames/tags are real values fetched from InternetDB/the
 * Shodan API; nothing is fabricated. If the host is not found / errors / yields
 * no ports, vulns or host metadata, NOTHING is emitted and run() returns 0. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#define SHODAN_API_BASE  "https://api.shodan.io"
#define INTERNETDB_URL   "https://internetdb.shodan.io/%s"

static int is_valid_ip(const char *s) {
  struct in_addr a4;
  struct in6_addr a6;
  return inet_pton(AF_INET, s, &a4) == 1 || inet_pton(AF_INET6, s, &a6) == 1;
}

/* InternetDB (free, no key): ports, hostnames, cpes, vulns, tags for an IP.
 * Returns the raw parsed JSON on a 200 (caller owns it), NULL otherwise. */
static cJSON *query_internetdb(http_client *http, const char *ip) {
  char url[256];
  snprintf(url, sizeof url, INTERNETDB_URL, ip);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 2, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  return json;
}

/* Shodan API host detail (requires key). Returns parsed JSON on 200, else NULL. */
static cJSON *query_shodan_host(http_client *http, const char *ip, const char *api_key) {
  if (!api_key || !*api_key) return NULL;
  char url[512];
  snprintf(url, sizeof url, "%s/shodan/host/%s?key=%s", SHODAN_API_BASE, ip, api_key);
  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 2, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return NULL; }
  cJSON *json = cJSON_Parse(hr.body);
  http_response_free(&hr);
  return json;
}

/* Shodan DNS resolve domain→IP (requires key). */
static char *shodan_dns_resolve(http_client *http, const char *domain, const char *api_key) {
  if (!api_key || !*api_key) return NULL;
  char url[512];
  snprintf(url, sizeof url, "%s/dns/resolve?hostnames=%s&key=%s", SHODAN_API_BASE, domain, api_key);
  cJSON *json = feed_get_json(http, url, 30000);
  char *ipdup = NULL;
  if (json) {
    cJSON *ip = cJSON_GetObjectItem(json, domain);
    if (ip && ip->valuestring) ipdup = strdup(ip->valuestring);
    cJSON_Delete(json);
  }
  return ipdup;
}

/* Emit one intel row for a single open port on <ip>. Returns 1 if emitted.
 * `svc` (optional) is the matching Shodan-API service object for richer body. */
static int emit_port(intel_sink *sink, const char *ip, int port,
                     cJSON *hostnames, cJSON *cpes, cJSON *svc) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "ip", ip);
  cJSON_AddNumberToObject(data, "port", port);
  if (hostnames && cJSON_IsArray(hostnames) && cJSON_GetArraySize(hostnames) > 0)
    cJSON_AddItemToObject(data, "hostnames", cJSON_Duplicate(hostnames, 1));
  if (cpes && cJSON_IsArray(cpes) && cJSON_GetArraySize(cpes) > 0)
    cJSON_AddItemToObject(data, "cpes", cJSON_Duplicate(cpes, 1));
  if (svc) {
    cJSON *transport = cJSON_GetObjectItem(svc, "transport");
    cJSON *product   = cJSON_GetObjectItem(svc, "product");
    cJSON *version   = cJSON_GetObjectItem(svc, "version");
    if (transport && transport->valuestring) cJSON_AddStringToObject(data, "transport", transport->valuestring);
    if (product && product->valuestring)     cJSON_AddStringToObject(data, "product", product->valuestring);
    if (version && version->valuestring)      cJSON_AddStringToObject(data, "version", version->valuestring);
  }
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SHODAN_SEARCH");
  cJSON_AddStringToObject(props, "ip", ip);
  cJSON_AddNumberToObject(props, "port", port);
  cJSON_AddStringToObject(props, "record", "open_port");
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 85);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[160], title[160];
  snprintf(rk, sizeof rk, "shodan:%s:port:%d", ip, port);
  snprintf(title, sizeof title, "%s:%d open", ip, port);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "open port";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SHODAN_SEARCH\",\"open-port\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* Emit one intel row for a single CVE on <ip>. Returns 1 if emitted. */
static int emit_vuln(intel_sink *sink, const char *ip, const char *cve,
                     cJSON *hostnames) {
  if (!cve || !*cve) return 0;
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "ip", ip);
  cJSON_AddStringToObject(data, "cve", cve);
  if (hostnames && cJSON_IsArray(hostnames) && cJSON_GetArraySize(hostnames) > 0)
    cJSON_AddItemToObject(data, "hostnames", cJSON_Duplicate(hostnames, 1));
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SHODAN_SEARCH");
  cJSON_AddStringToObject(props, "ip", ip);
  cJSON_AddStringToObject(props, "cve", cve);
  cJSON_AddStringToObject(props, "record", "vulnerability");
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 85);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[200], title[200];
  snprintf(rk, sizeof rk, "shodan:%s:vuln:%s", ip, cve);
  snprintf(title, sizeof title, "%s — %s", ip, cve);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "vulnerability";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SHODAN_SEARCH\",\"vulnerability\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* Emit one host-summary row carrying real hostnames/tags/org metadata. Only
 * emits when there is at least one real metadata field. Returns 1 if emitted. */
static int emit_host(intel_sink *sink, const char *ip,
                     cJSON *hostnames, cJSON *tags, cJSON *host_api) {
  int have_hn  = hostnames && cJSON_IsArray(hostnames) && cJSON_GetArraySize(hostnames) > 0;
  int have_tag = tags && cJSON_IsArray(tags) && cJSON_GetArraySize(tags) > 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "ip", ip);
  if (have_hn)  cJSON_AddItemToObject(data, "hostnames", cJSON_Duplicate(hostnames, 1));
  if (have_tag) cJSON_AddItemToObject(data, "tags", cJSON_Duplicate(tags, 1));

  int have_meta = 0;
  if (host_api) {
    const char *keys[] = { "org", "asn", "isp", "country_name", "city",
                           "os", "last_update", NULL };
    const char *outk[] = { "organization", "asn", "isp", "country", "city",
                           "operating_system", "last_update", NULL };
    for (int i = 0; keys[i]; i++) {
      cJSON *v = cJSON_GetObjectItem(host_api, keys[i]);
      if (v && v->valuestring) { cJSON_AddStringToObject(data, outk[i], v->valuestring); have_meta = 1; }
    }
    cJSON *domains = cJSON_GetObjectItem(host_api, "domains");
    if (domains && cJSON_IsArray(domains) && cJSON_GetArraySize(domains) > 0) {
      cJSON_AddItemToObject(data, "domains", cJSON_Duplicate(domains, 1));
      have_meta = 1;
    }
  }

  if (!have_hn && !have_tag && !have_meta) { cJSON_Delete(data); return 0; }
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SHODAN_SEARCH");
  cJSON_AddStringToObject(props, "ip", ip);
  cJSON_AddStringToObject(props, "record", "host");
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 85);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[160], title[160];
  snprintf(rk, sizeof rk, "shodan:%s", ip);
  snprintf(title, sizeof title, "SHODAN host %s", ip);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = "host metadata";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SHODAN_SEARCH\",\"host\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

/* Find the Shodan-API "data" service object matching `port`, or NULL. */
static cJSON *find_service(cJSON *host_api, int port) {
  if (!host_api) return NULL;
  cJSON *data = cJSON_GetObjectItem(host_api, "data");
  if (!data || !cJSON_IsArray(data)) return NULL;
  int n = cJSON_GetArraySize(data);
  for (int i = 0; i < n; i++) {
    cJSON *item = cJSON_GetArrayItem(data, i);
    cJSON *p = cJSON_GetObjectItem(item, "port");
    if (p && p->valueint == port) return item;
  }
  return NULL;
}

/* Explode one IP's fetched data into per-record items. Returns count emitted. */
static int emit_ip(intel_sink *sink, http_client *http, const char *ip,
                   const char *api_key) {
  cJSON *idb = query_internetdb(http, ip);                 /* free path */
  cJSON *host_api = query_shodan_host(http, ip, api_key);  /* NULL if no key */
  if (!idb && !host_api) return 0;

  /* Prefer InternetDB arrays; fall back to Shodan-API arrays. */
  cJSON *ports     = idb ? cJSON_GetObjectItem(idb, "ports") : NULL;
  cJSON *hostnames = idb ? cJSON_GetObjectItem(idb, "hostnames") : NULL;
  cJSON *cpes      = idb ? cJSON_GetObjectItem(idb, "cpes") : NULL;
  cJSON *vulns     = idb ? cJSON_GetObjectItem(idb, "vulns") : NULL;
  cJSON *tags      = idb ? cJSON_GetObjectItem(idb, "tags") : NULL;

  if ((!ports || !cJSON_IsArray(ports)) && host_api)
    ports = cJSON_GetObjectItem(host_api, "ports");
  if ((!hostnames || !cJSON_IsArray(hostnames)) && host_api)
    hostnames = cJSON_GetObjectItem(host_api, "hostnames");
  if ((!vulns || !cJSON_IsArray(vulns)) && host_api)
    vulns = cJSON_GetObjectItem(host_api, "vulns");

  int emitted = 0;

  /* host-summary (real hostnames/tags/org) */
  emitted += emit_host(sink, ip, hostnames, tags, host_api);

  /* one item per open port */
  if (ports && cJSON_IsArray(ports)) {
    int n = cJSON_GetArraySize(ports);
    for (int i = 0; i < n; i++) {
      cJSON *p = cJSON_GetArrayItem(ports, i);
      if (!p || !cJSON_IsNumber(p)) continue;
      int port = p->valueint;
      emitted += emit_port(sink, ip, port, hostnames, cpes,
                           find_service(host_api, port));
    }
  }

  /* one item per vuln (CVE) */
  if (vulns && cJSON_IsArray(vulns)) {
    int n = cJSON_GetArraySize(vulns);
    for (int i = 0; i < n; i++) {
      cJSON *v = cJSON_GetArrayItem(vulns, i);
      if (v && v->valuestring) emitted += emit_vuln(sink, ip, v->valuestring, hostnames);
    }
  }

  if (idb) cJSON_Delete(idb);
  if (host_api) cJSON_Delete(host_api);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return -1;

  /* upstream skips emails */
  if (strchr(entity, '@')) return 0;

  const char *api_key = getenv("SHODAN_API_KEY");

  if (is_valid_ip(entity)) {
    emit_ip(sink, ctx->http, entity, api_key);
    return 0;                               /* honest empty is not an error */
  }

  /* domain → requires key for DNS resolution */
  char *rip = shodan_dns_resolve(ctx->http, entity, api_key);
  if (rip) {
    emit_ip(sink, ctx->http, rip, api_key);
    free(rip);
  }
  return 0;
}

static const source_def shodan_search_def = {
  .id = "SHODAN_SEARCH", .collector = "osint",
  .name = "Shodan Search", .name_ja = "Shodan検索",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/shodan-search",
  .description = "Query exposed ports/vulns via Shodan InternetDB and API.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(shodan_search_def)
