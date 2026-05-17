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
 * resolve then the resolved IP (key only). Builds the identical root:
 * { service:"SHODAN_SEARCH", api_key_configured, exposed_hosts,
 *   total_vulnerabilities, results:[{query, internetdb, shodan_api?, …}],
 *   note? }. This service is NOT key-only (InternetDB is the no-key path), so
 * it does NOT return 0 when the key is absent. success=true, confidence 85.
 * Emits one osint_service_result row. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
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

/* InternetDB (free, no key): ports, hostnames, cpes, vulns, tags for an IP */
static cJSON *query_internetdb(http_client *http, const char *ip) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "internetdb");
  cJSON_AddStringToObject(result, "ip", ip);

  char url[256];
  snprintf(url, sizeof url, INTERNETDB_URL, ip);

  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 2, &hr);
  if (hc != 0) { cJSON_AddStringToObject(result, "status", "error"); http_response_free(&hr); return result; }
  if (hr.status == 404) {
    cJSON_AddStringToObject(result, "status", "not_found");
    cJSON_AddStringToObject(result, "message", "IP not in InternetDB database");
    http_response_free(&hr);
    return result;
  }
  if (hr.status != 200) {
    cJSON_AddStringToObject(result, "status", "error");
    cJSON_AddNumberToObject(result, "http_code", hr.status);
    http_response_free(&hr);
    return result;
  }
  cJSON *json = hr.body ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);

  if (json) {
    cJSON_AddStringToObject(result, "status", "found");

    cJSON *ports = cJSON_GetObjectItem(json, "ports");
    if (ports && cJSON_IsArray(ports)) {
      cJSON_AddItemToObject(result, "open_ports", cJSON_Duplicate(ports, 1));
      cJSON_AddNumberToObject(result, "port_count", cJSON_GetArraySize(ports));
    }
    cJSON *hostnames = cJSON_GetObjectItem(json, "hostnames");
    if (hostnames && cJSON_IsArray(hostnames))
      cJSON_AddItemToObject(result, "hostnames", cJSON_Duplicate(hostnames, 1));
    cJSON *cpes = cJSON_GetObjectItem(json, "cpes");
    if (cpes && cJSON_IsArray(cpes)) {
      cJSON_AddItemToObject(result, "cpes", cJSON_Duplicate(cpes, 1));
      cJSON_AddNumberToObject(result, "software_detected", cJSON_GetArraySize(cpes));
    }
    cJSON *vulns = cJSON_GetObjectItem(json, "vulns");
    if (vulns && cJSON_IsArray(vulns)) {
      cJSON_AddItemToObject(result, "vulnerabilities", cJSON_Duplicate(vulns, 1));
      int vc = cJSON_GetArraySize(vulns);
      cJSON_AddNumberToObject(result, "vulnerability_count", vc);
      if (vc > 10) cJSON_AddStringToObject(result, "risk_level", "CRITICAL");
      else if (vc > 5) cJSON_AddStringToObject(result, "risk_level", "HIGH");
      else if (vc > 0) cJSON_AddStringToObject(result, "risk_level", "MEDIUM");
      else cJSON_AddStringToObject(result, "risk_level", "LOW");
    }
    cJSON *tags = cJSON_GetObjectItem(json, "tags");
    if (tags && cJSON_IsArray(tags))
      cJSON_AddItemToObject(result, "tags", cJSON_Duplicate(tags, 1));
    cJSON_Delete(json);
  }
  return result;
}

/* Shodan API host detail (requires key) */
static cJSON *query_shodan_host(http_client *http, const char *ip, const char *api_key) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "shodan_api");
  cJSON_AddStringToObject(result, "ip", ip);
  if (!api_key || !*api_key) { cJSON_AddStringToObject(result, "status", "no_api_key"); return result; }

  char url[512];
  snprintf(url, sizeof url, "%s/shodan/host/%s?key=%s", SHODAN_API_BASE, ip, api_key);

  http_response hr = {0};
  int hc = http_request(http, "GET", url, NULL, NULL, 0, 30000, 2, &hr);
  if (hc != 0) { cJSON_AddStringToObject(result, "status", "error"); http_response_free(&hr); return result; }
  if (hr.status == 404) { cJSON_AddStringToObject(result, "status", "not_found"); http_response_free(&hr); return result; }
  if (hr.status == 401) { cJSON_AddStringToObject(result, "status", "invalid_api_key"); http_response_free(&hr); return result; }
  if (hr.status != 200) {
    cJSON_AddStringToObject(result, "status", "error");
    cJSON_AddNumberToObject(result, "http_code", hr.status);
    http_response_free(&hr);
    return result;
  }
  cJSON *json = hr.body ? cJSON_Parse(hr.body) : NULL;
  http_response_free(&hr);

  if (json) {
    cJSON_AddStringToObject(result, "status", "found");
    cJSON *org = cJSON_GetObjectItem(json, "org");
    cJSON *asn = cJSON_GetObjectItem(json, "asn");
    cJSON *isp = cJSON_GetObjectItem(json, "isp");
    cJSON *country = cJSON_GetObjectItem(json, "country_name");
    cJSON *city = cJSON_GetObjectItem(json, "city");
    cJSON *os = cJSON_GetObjectItem(json, "os");
    cJSON *last_update = cJSON_GetObjectItem(json, "last_update");
    if (org && org->valuestring) cJSON_AddStringToObject(result, "organization", org->valuestring);
    if (asn && asn->valuestring) cJSON_AddStringToObject(result, "asn", asn->valuestring);
    if (isp && isp->valuestring) cJSON_AddStringToObject(result, "isp", isp->valuestring);
    if (country && country->valuestring) cJSON_AddStringToObject(result, "country", country->valuestring);
    if (city && city->valuestring) cJSON_AddStringToObject(result, "city", city->valuestring);
    if (os && os->valuestring) cJSON_AddStringToObject(result, "operating_system", os->valuestring);
    if (last_update && last_update->valuestring) cJSON_AddStringToObject(result, "last_update", last_update->valuestring);

    cJSON *ports = cJSON_GetObjectItem(json, "ports");
    if (ports && cJSON_IsArray(ports)) cJSON_AddItemToObject(result, "open_ports", cJSON_Duplicate(ports, 1));
    cJSON *hostnames = cJSON_GetObjectItem(json, "hostnames");
    if (hostnames && cJSON_IsArray(hostnames)) cJSON_AddItemToObject(result, "hostnames", cJSON_Duplicate(hostnames, 1));
    cJSON *domains = cJSON_GetObjectItem(json, "domains");
    if (domains && cJSON_IsArray(domains)) cJSON_AddItemToObject(result, "domains", cJSON_Duplicate(domains, 1));
    cJSON *vulns = cJSON_GetObjectItem(json, "vulns");
    if (vulns && cJSON_IsArray(vulns)) {
      cJSON_AddItemToObject(result, "vulnerabilities", cJSON_Duplicate(vulns, 1));
      cJSON_AddNumberToObject(result, "vulnerability_count", cJSON_GetArraySize(vulns));
    }
    cJSON *data = cJSON_GetObjectItem(json, "data");
    if (data && cJSON_IsArray(data)) {
      cJSON *services = cJSON_CreateArray();
      int count = cJSON_GetArraySize(data);
      for (int i = 0; i < count && i < 20; i++) {
        cJSON *item = cJSON_GetArrayItem(data, i);
        cJSON *service = cJSON_CreateObject();
        cJSON *port = cJSON_GetObjectItem(item, "port");
        cJSON *transport = cJSON_GetObjectItem(item, "transport");
        cJSON *product = cJSON_GetObjectItem(item, "product");
        cJSON *version = cJSON_GetObjectItem(item, "version");
        if (port) cJSON_AddNumberToObject(service, "port", port->valueint);
        if (transport && transport->valuestring) cJSON_AddStringToObject(service, "transport", transport->valuestring);
        if (product && product->valuestring) cJSON_AddStringToObject(service, "product", product->valuestring);
        if (version && version->valuestring) cJSON_AddStringToObject(service, "version", version->valuestring);
        cJSON_AddItemToArray(services, service);
      }
      cJSON_AddItemToObject(result, "services", services);
    }
    cJSON_Delete(json);
  }
  return result;
}

/* Shodan DNS resolve domain→IP (requires key) */
static cJSON *shodan_dns_resolve(http_client *http, const char *domain, const char *api_key) {
  cJSON *result = cJSON_CreateObject();
  if (!api_key || !*api_key) return result;
  char url[512];
  snprintf(url, sizeof url, "%s/dns/resolve?hostnames=%s&key=%s", SHODAN_API_BASE, domain, api_key);
  cJSON *json = feed_get_json(http, url, 30000);
  if (json) {
    cJSON *ip = cJSON_GetObjectItem(json, domain);
    if (ip && ip->valuestring) cJSON_AddStringToObject(result, "resolved_ip", ip->valuestring);
    cJSON_Delete(json);
  }
  return result;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *entity = ctx->entity;
  if (!entity || !*entity) return -1;

  const char *api_key = getenv("SHODAN_API_KEY");
  int has_key = api_key && *api_key;

  cJSON *root = cJSON_CreateObject();
  cJSON *all_results = cJSON_CreateArray();
  int total_exposed = 0, total_vulns = 0;

  /* upstream skips emails; non-email single entity processed once */
  if (!strchr(entity, '@')) {
    cJSON *er = cJSON_CreateObject();
    cJSON_AddStringToObject(er, "query", entity);

    if (is_valid_ip(entity)) {
      cJSON *idb = query_internetdb(ctx->http, entity);
      cJSON_AddItemToObject(er, "internetdb", idb);
      cJSON *pc = cJSON_GetObjectItem(idb, "port_count");
      cJSON *vc = cJSON_GetObjectItem(idb, "vulnerability_count");
      if (pc && pc->valueint > 0) total_exposed++;
      if (vc) total_vulns += vc->valueint;
      if (has_key) {
        cJSON *sh = query_shodan_host(ctx->http, entity, api_key);
        cJSON_AddItemToObject(er, "shodan_api", sh);
      }
    } else {
      if (has_key) {
        cJSON *dns = shodan_dns_resolve(ctx->http, entity, api_key);
        cJSON *rip = cJSON_GetObjectItem(dns, "resolved_ip");
        if (rip && rip->valuestring) {
          cJSON_AddStringToObject(er, "resolved_ip", rip->valuestring);
          cJSON *idb = query_internetdb(ctx->http, rip->valuestring);
          cJSON_AddItemToObject(er, "internetdb", idb);
          cJSON *sh = query_shodan_host(ctx->http, rip->valuestring, api_key);
          cJSON_AddItemToObject(er, "shodan_api", sh);
          cJSON *pc = cJSON_GetObjectItem(idb, "port_count");
          cJSON *vc = cJSON_GetObjectItem(idb, "vulnerability_count");
          if (pc && pc->valueint > 0) total_exposed++;
          if (vc) total_vulns += vc->valueint;
        }
        cJSON_Delete(dns);
      } else {
        cJSON_AddStringToObject(er, "note",
          "Domain lookup requires SHODAN_API_KEY for DNS resolution");
      }
    }
    cJSON_AddItemToArray(all_results, er);
  }

  cJSON_AddStringToObject(root, "service", "SHODAN_SEARCH");
  cJSON_AddBoolToObject(root, "api_key_configured", has_key);
  cJSON_AddNumberToObject(root, "exposed_hosts", total_exposed);
  cJSON_AddNumberToObject(root, "total_vulnerabilities", total_vulns);
  cJSON_AddItemToObject(root, "results", all_results);
  if (!has_key)
    cJSON_AddStringToObject(root, "note",
      "Using InternetDB (free). Set SHODAN_API_KEY for detailed host data.");

  char *bj = cJSON_PrintUnformatted(root);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "SHODAN_SEARCH");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", 1);          /* upstream success=true */
  cJSON_AddNumberToObject(props, "confidence", 85);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "shodan:%s", entity);
  char title[320];
  snprintf(title, sizeof title, "SHODAN_SEARCH — %s", entity);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = total_exposed > 0 ? "exposed host(s) found" : "no exposure found";
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"SHODAN_SEARCH\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(props);
  cJSON_Delete(root);
  return rc >= 0 ? 0 : -1;
}

static const source_def shodan_search_def = {
  .id = "SHODAN_SEARCH", .collector = "osint",
  .name = "Shodan Search", .name_ja = "Shodan検索",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(shodan_search_def)
