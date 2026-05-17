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
#include "../../../source.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
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

static int emit_one(intel_sink *sink, const char *svc, const char *entity,
                    int confidence, cJSON *data /*owned*/) {
  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);          /* OSINTsaas: always true */
  cJSON_AddNumberToObject(env, "confidence", confidence);
  cJSON_AddItemToObject(env, "data", data);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", svc);
  cJSON_AddStringToObject(props, "entity", entity);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", confidence);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[320];
  snprintf(rk, sizeof rk, "%s:%s", svc, entity);
  char title[360];
  snprintf(title, sizeof title, "%s — %s", svc, entity);
  char tags[96];
  snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", svc);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = "network util result";
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = tags;
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

/* handle_tor_exit_check: skip non-IPs (is_valid_ip); 1 entity. */
static int run_tor(const source_ctx *ctx, intel_sink *sink) {
  const char *ip = ctx->entity;
  if (!ip || !*ip) return -1;
  cJSON *root = cJSON_CreateObject();
  cJSON *results = cJSON_CreateArray();
  int found = 0, checked = 0;
  if (is_valid_ip(ip)) {
    checked = 1;
    int is_tor = check_tor_exit(ctx->http, ip);
    cJSON *r = cJSON_CreateObject();
    cJSON_AddStringToObject(r, "ip", ip);
    cJSON_AddBoolToObject(r, "is_tor_exit", is_tor);
    if (is_tor) {
      found++;
      cJSON_AddStringToObject(r, "risk_level", "high");
      cJSON_AddStringToObject(r, "note",
        "Traffic from this IP may be anonymized");
    } else {
      cJSON_AddStringToObject(r, "risk_level", "normal");
    }
    cJSON_AddItemToArray(results, r);
  }
  cJSON_AddStringToObject(root, "service", "TOR_EXIT_CHECK");
  cJSON_AddNumberToObject(root, "total_checked", 1);
  cJSON_AddNumberToObject(root, "tor_nodes_found", found);
  cJSON_AddNumberToObject(root, "exit_nodes_in_database", tor_exit_count);
  cJSON_AddItemToObject(root, "results", results);
  (void)checked;
  return emit_one(sink, "TOR_EXIT_CHECK", ip, 95, root);
}

static const source_def tor_exit_check_def = {
  .id = "TOR_EXIT_CHECK", .collector = "osint",
  .name = "Tor Exit Check", .name_ja = "Tor出口ノード判定",
  .update_interval_sec = 0, .run = run_tor,
};
REGISTER_SOURCE(tor_exit_check_def)
