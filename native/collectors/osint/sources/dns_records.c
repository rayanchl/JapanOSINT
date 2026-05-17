/* collectors/osint/sources/dns_records.c
 * OSINT_SERVICE source — port of the DNS_RECORDS service (net.js). On-demand
 * (interval 0); the pipeline dispatches it with ctx->entity = a domain. Emits
 * one osint_service_result intel row so service output unifies into the same
 * intel store + entity graph as feeds. Proves the SRC_OSINT_SERVICE path. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *domain = ctx->entity;
  if (!domain || !*domain) return -1;

  struct addrinfo hints = {0}, *res = NULL;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  cJSON *a = cJSON_CreateArray();
  int count = 0;
  if (getaddrinfo(domain, NULL, &hints, &res) == 0) {
    for (struct addrinfo *p = res; p; p = p->ai_next) {
      char ip[INET6_ADDRSTRLEN] = {0};
      void *sa = (p->ai_family == AF_INET)
        ? (void *)&((struct sockaddr_in *)p->ai_addr)->sin_addr
        : (void *)&((struct sockaddr_in6 *)p->ai_addr)->sin6_addr;
      if (inet_ntop(p->ai_family, sa, ip, sizeof ip)) {
        cJSON_AddItemToArray(a, cJSON_CreateString(ip));
        count++;
      }
    }
    freeaddrinfo(res);
  }

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "domain", domain);
  cJSON_AddItemToObject(data, "addresses", a);
  char *dj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "DNS_RECORDS");
  cJSON_AddStringToObject(props, "entity", domain);
  cJSON_AddBoolToObject(props, "success", count > 0);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "dns:%s", domain);
  char title[320];
  snprintf(title, sizeof title, "DNS_RECORDS — %s", domain);

  intel_item it = {0};
  it.remote_key   = rk;                 /* uid = osint-search|dns:<domain> */
  it.title        = title;
  it.body         = dj;
  it.summary      = count > 0 ? "DNS resolved" : "no DNS records";
  it.record_type  = "osint_service_result";
  it.properties_json = pj;
  it.tags_json    = "[\"osint-search\",\"DNS_RECORDS\"]";
  int rc = sink->emit(sink, &it);

  free(dj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 0 : -1;
}

static const source_def dns_records_def = {
  .id = "DNS_RECORDS", .collector = "osint",
  .name = "DNS Records", .name_ja = "DNSレコード",
   .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(dns_records_def)
