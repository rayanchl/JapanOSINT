/* collectors/osint/sources/reverse_dns.c
 * OSINT service — port of net.js reverseDns(). On-demand (interval 0); the
 * OSINT dispatcher runs it with ctx->entity = an IP. net.js does a PTR lookup
 * (dns.reverse(entity)). C equivalent: parse the IP into a sockaddr (v4/v6)
 * then getnameinfo(NI_NAMEREQD) for the PTR hostname.
 *
 * PER-RECORD EMIT: emits ONE clean intel_item PER resolved PTR hostname —
 * body = {ip,hostname}, title = "<ip> -> <hostname>", remote_key =
 * "ptr:<ip>:<hostname>". No PTR record / resolver failure / invalid IP →
 * emit NOTHING and return 0 (honest empty). No {success,confidence,data}
 * envelope. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Emit one intel row for a single resolved PTR hostname. Returns 1 if emitted. */
static int emit_ptr(intel_sink *sink, const char *ip, const char *hostname) {
  cJSON *body = cJSON_CreateObject();
  cJSON_AddStringToObject(body, "ip", ip);
  cJSON_AddStringToObject(body, "hostname", hostname);
  char *bj = cJSON_PrintUnformatted(body);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "REVERSE_DNS");
  cJSON_AddStringToObject(props, "entity", ip);
  cJSON_AddStringToObject(props, "hostname", hostname);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[400];
  snprintf(rk, sizeof rk, "ptr:%s:%s", ip, hostname);
  char title[400];
  snprintf(title, sizeof title, "%s -> %s", ip, hostname);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = hostname;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"REVERSE_DNS\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(body);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *ip = ctx->entity;
  if (!ip || !*ip) return 0;

  /* Parse the IP literal into a sockaddr (no network I/O). */
  struct sockaddr_storage ss;
  memset(&ss, 0, sizeof ss);
  socklen_t slen = 0;
  struct in_addr a4;
  struct in6_addr a6;
  if (inet_pton(AF_INET, ip, &a4) == 1) {
    struct sockaddr_in *s = (struct sockaddr_in *)&ss;
    s->sin_family = AF_INET;
    s->sin_addr = a4;
    slen = sizeof *s;
  } else if (inet_pton(AF_INET6, ip, &a6) == 1) {
    struct sockaddr_in6 *s = (struct sockaddr_in6 *)&ss;
    s->sin6_family = AF_INET6;
    s->sin6_addr = a6;
    slen = sizeof *s;
  } else {
    /* Not a valid IP → nothing to resolve → honest empty. */
    return 0;
  }

  char host[NI_MAXHOST] = {0};
  int gni = getnameinfo((struct sockaddr *)&ss, slen,
                        host, sizeof host, NULL, 0, NI_NAMEREQD);

  /* No PTR record / resolver failure → emit nothing. */
  if (gni != 0 || !host[0]) return 0;

  emit_ptr(sink, ip, host);
  return 0;
}

static const source_def reverse_dns_def = {
  .id = "REVERSE_DNS", .collector = "osint",
  .name = "Reverse DNS", .name_ja = "逆引きDNS",
  .update_interval_sec = 0, .run = run,
  .category = "cyber", .type = "api",
  .url = "internal://osint/reverse-dns",
  .description = "Resolve the PTR hostname for an IP via getnameinfo.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(reverse_dns_def)
