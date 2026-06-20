/* collectors/osint/sources/dns_records.c
 * OSINT_SERVICE source — port of the DNS_RECORDS service (net.js). On-demand
 * (interval 0); the pipeline dispatches it with ctx->entity = a domain.
 *
 * PER-RECORD EMIT: emits ONE intel_item per resolved address
 * (remote_key="dns:<domain>:<ip>"), body {domain,address,family:"A"|"AAAA"}.
 * If nothing resolves, emits nothing, returns 0 (honest empty). */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Emit ONE intel_item for a single resolved address. Returns 1 if emitted. */
static int emit_addr(intel_sink *sink, const char *domain, const char *ip,
                     const char *family) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "domain", domain);
  cJSON_AddStringToObject(data, "address", ip);
  cJSON_AddStringToObject(data, "family", family);
  char *dj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "DNS_RECORDS");
  cJSON_AddStringToObject(props, "entity", domain);
  cJSON_AddStringToObject(props, "family", family);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[320];
  snprintf(rk, sizeof rk, "dns:%s:%s", domain, ip);
  char title[360];
  snprintf(title, sizeof title, "%s -> %s", domain, ip);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = dj;
  it.summary         = family;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"DNS_RECORDS\"]";
  int rc = sink->emit(sink, &it);

  free(dj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *domain = ctx->entity;
  if (!domain || !*domain) return -1;

  struct addrinfo hints = {0}, *res = NULL;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  int emitted = 0;
  if (getaddrinfo(domain, NULL, &hints, &res) == 0) {
    for (struct addrinfo *p = res; p; p = p->ai_next) {
      char ip[INET6_ADDRSTRLEN] = {0};
      void *sa = (p->ai_family == AF_INET)
        ? (void *)&((struct sockaddr_in *)p->ai_addr)->sin_addr
        : (void *)&((struct sockaddr_in6 *)p->ai_addr)->sin6_addr;
      if (inet_ntop(p->ai_family, sa, ip, sizeof ip))
        emitted += emit_addr(sink, domain, ip,
                             p->ai_family == AF_INET ? "A" : "AAAA");
    }
    freeaddrinfo(res);
  }

  (void)emitted;
  return 0;                  /* nothing resolved → honest empty, not an error */
}

static const source_def dns_records_def = {
  .id = "DNS_RECORDS", .collector = "osint",
  .name = "DNS Records", .name_ja = "DNSレコード",
   .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/dns-records",
  .description = "Resolve a domain's A/AAAA addresses via getaddrinfo.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(dns_records_def)
