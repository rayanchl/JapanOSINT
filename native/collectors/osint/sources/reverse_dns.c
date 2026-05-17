/* collectors/osint/sources/reverse_dns.c
 * OSINT service — port of net.js reverseDns(). On-demand (interval 0); the
 * OSINT dispatcher runs it with ctx->entity = an IP. net.js:
 *   const names = await dns.reverse(entity);
 *   return ok({ ip: entity, ptr: names }, names.length ? 90 : 40);
 *   catch -> fail(e)
 * dns.reverse() is a PTR lookup on the IP. C equivalent: parse the IP into a
 * sockaddr (v4/v6) then getnameinfo(NI_NAMEREQD) for the PTR hostname. ptr is
 * emitted as an array (net.js `names` is an array) — empty → confidence 40,
 * non-empty → 90. A resolver failure (EAI_*) mirrors the JS throw → fail().
 * No DNS records at all is NOT an error in net.js (catch only on throw); a
 * NONAME / NODATA result yields an empty ptr array at confidence 40. Emits one
 * osint_service_result intel row (body = envelope), like dns_records.c. */
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int emit_result(intel_sink *sink, const char *entity, int success,
                        int confidence, cJSON *data /*owned*/,
                        const char *error) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", success);
  cJSON_AddNumberToObject(env, "confidence", confidence);
  if (data) cJSON_AddItemToObject(env, "data", data);
  else cJSON_AddNullToObject(env, "data");
  if (error) cJSON_AddStringToObject(env, "error", error);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "REVERSE_DNS");
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", success);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "rdns:%s", entity);
  char title[320];
  snprintf(title, sizeof title, "REVERSE_DNS — %s", entity);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = !success ? (error ? error : "lookup failed")
                                : (confidence >= 90 ? "PTR resolved" : "no PTR record");
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"REVERSE_DNS\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *ip = ctx->entity;
  if (!ip || !*ip) return -1;

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
    /* net.js: dns.reverse on a non-IP throws → fail(e). */
    return emit_result(sink, ip, 0, 0, NULL, "invalid_ip");
  }

  char host[NI_MAXHOST] = {0};
  int gni = getnameinfo((struct sockaddr *)&ss, slen,
                        host, sizeof host, NULL, 0, NI_NAMEREQD);

  cJSON *ptr = cJSON_CreateArray();
  if (gni == 0 && host[0]) {
    cJSON_AddItemToArray(ptr, cJSON_CreateString(host));
  } else if (gni != 0 && gni != EAI_NONAME
#ifdef EAI_NODATA
             && gni != EAI_NODATA
#endif
            ) {
    /* Real resolver/transport error → mirror the JS throw → fail(). */
    cJSON_Delete(ptr);
    return emit_result(sink, ip, 0, 0, NULL, gai_strerror(gni));
  }
  /* EAI_NONAME / EAI_NODATA → empty ptr array, not an error (== JS: no throw,
   * names = [] is impossible there but a 0-length result maps to conf 40). */

  int n = cJSON_GetArraySize(ptr);
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "ip", ip);
  cJSON_AddItemToObject(data, "ptr", ptr);
  return emit_result(sink, ip, 1, n ? 90 : 40, data, NULL);
}

static const source_def reverse_dns_def = {
  .id = "REVERSE_DNS", .collector = "osint",
  .name = "Reverse DNS", .name_ja = "逆引きDNS",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(reverse_dns_def)
