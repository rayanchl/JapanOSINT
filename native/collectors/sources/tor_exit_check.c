/* collectors/osint/sources/tor_exit_check.c
 * OSINT service — faithful port of OSINTsaas osint_tools/network_utils.c
 * (TOR_EXIT_CHECK handle_tor_exit_check; ctx->entity = IP).
 * On-demand (interval 0). No API keys. Reproduces: Tor bulk-exit list from
 * check.torproject.org (1h static cache, == upstream) + check_tor_exit. The
 * upstream handler loops over entities[]; the pipeline pivots one entity at a
 * time so it runs over ctx->entity (count==1) — the result envelope keeps the
 * upstream {service,total_*,...,results:[...]} shape with a single result.
 * success=true, confidence 95. Emits ONE osint_service_result row; body =
 * {success,confidence,data} envelope, like ip_geolocation.c. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* is_valid_ip (== OSINTsaas common): v4 or v6 literal. */
static int is_valid_ip(const char *ip) {
  struct in_addr a4;
  struct in6_addr a6;
  return inet_pton(AF_INET, ip, &a4) == 1 ||
         inet_pton(AF_INET6, ip, &a6) == 1;
}

/* ---- Tor exit list (1h static cache, mirrors upstream globals) ---- */
static char **tor_exit_nodes = NULL;
static int tor_exit_count = 0;
static time_t tor_list_loaded = 0;
#define TOR_LIST_CACHE_HOURS 1

static void load_tor_exit_list(http_client *http) {
  time_t now = time(NULL);
  if (tor_exit_nodes && (now - tor_list_loaded) < (TOR_LIST_CACHE_HOURS * 3600))
    return;
  if (tor_exit_nodes) {
    for (int i = 0; i < tor_exit_count; i++) free(tor_exit_nodes[i]);
    free(tor_exit_nodes);
    tor_exit_nodes = NULL;
    tor_exit_count = 0;
  }
  http_response hr = {0};
  int hc = http_request(http, "GET",
    "https://check.torproject.org/torbulkexitlist", NULL, NULL, 0,
    30000, 1, &hr);
  if (hc != 0 || hr.status != 200 || !hr.body) { http_response_free(&hr); return; }

  int count = 0;
  for (char *p = hr.body; *p; p++) if (*p == '\n') count++;
  count++;
  tor_exit_nodes = calloc((size_t)count, sizeof(char *));
  tor_exit_count = 0;
  char *save = NULL;
  char *line = strtok_r(hr.body, "\n", &save);
  while (line && tor_exit_count < count) {
    if (line[0] != '#' && line[0] != '\0' && strchr(line, '.'))
      tor_exit_nodes[tor_exit_count++] = strdup(line);
    line = strtok_r(NULL, "\n", &save);
  }
  tor_list_loaded = now;
  http_response_free(&hr);
}

static int check_tor_exit(http_client *http, const char *ip) {
  load_tor_exit_list(http);
  if (!tor_exit_nodes || tor_exit_count == 0) return 0;
  for (int i = 0; i < tor_exit_count; i++)
    if (strcmp(tor_exit_nodes[i], ip) == 0) return 1;
  return 0;
}

/* handle_tor_exit_check: skip non-IPs (is_valid_ip). The boolean verdict from
 * the downloaded exit list IS the record — emit it even when false. Emit
 * nothing if the IP is invalid or the exit list failed to download (no data
 * to verdict against). */
static int run_tor(const source_ctx *ctx, intel_sink *sink) {
  const char *ip = ctx->entity;
  if (!ip || !*ip) return 0;
  if (!is_valid_ip(ip)) return 0;

  int is_tor = check_tor_exit(ctx->http, ip);
  /* Download fail → no exit list cached → no real verdict; emit nothing. */
  if (!tor_exit_nodes || tor_exit_count == 0) return 0;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "ip", ip);
  cJSON_AddBoolToObject(data, "is_tor_exit", is_tor);
  cJSON_AddNumberToObject(data, "exit_nodes_in_database", tor_exit_count);
  cJSON_AddStringToObject(data, "risk_level", is_tor ? "high" : "normal");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "TOR_EXIT_CHECK");
  cJSON_AddStringToObject(props, "ip", ip);
  cJSON_AddBoolToObject(props, "is_tor_exit", is_tor);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char rk[320];  snprintf(rk, sizeof rk, "torexit:%s", ip);
  char title[360]; snprintf(title, sizeof title, "%s — Tor exit: %s",
                            ip, is_tor ? "yes" : "no");

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = is_tor ? "Tor exit node" : "not a Tor exit node";
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"TOR_EXIT_CHECK\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  return rc >= 0 ? 0 : -1;
}

static const source_def tor_exit_check_def = {
  .id = "TOR_EXIT_CHECK", .collector = "osint",
  .name = "Tor Exit Check", .name_ja = "Tor出口ノード判定",
  .update_interval_sec = 0, .run = run_tor,
  .category = "cyber", .type = "api",
  .url = "internal://osint/tor-exit-check",
  .description = "Check whether an IP is a Tor exit node.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(tor_exit_check_def)
