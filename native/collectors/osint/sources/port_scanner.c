/* collectors/osint/sources/port_scanner.c
 * OSINT service — faithful port of OSINTsaas osint_tools/network_utils.c
 * (PORT_SCANNER handle_port_scanner; ctx->entity = host).
 * On-demand (interval 0). No API keys. Reproduces scan_ports over the exact
 * common ports[]/service-names with a non-blocking connect()+select() probe.
 * The upstream handler loops over entities[]; the pipeline pivots one entity at
 * a time so it runs over ctx->entity (count==1) — the result envelope keeps the
 * upstream {service,total_*,...,results:[...]} shape with a single result.
 * success=true, confidence 85. Emits ONE osint_service_result row; body =
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

/* check_port: non-blocking connect + select, verbatim. */
static int check_port(const char *host, int port, int timeout_ms) {
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons((uint16_t)port);
  struct hostent *he = gethostbyname(host);
  if (he) memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
  else if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) return 0;

  int sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) return 0;
  int flags = fcntl(sock, F_GETFL, 0);
  fcntl(sock, F_SETFL, flags | O_NONBLOCK);
  int res = connect(sock, (struct sockaddr *)&addr, sizeof addr);
  if (res < 0) {
    if (errno == EINPROGRESS) {
      fd_set fds;
      struct timeval tv;
      tv.tv_sec = timeout_ms / 1000;
      tv.tv_usec = (timeout_ms % 1000) * 1000;
      FD_ZERO(&fds);
      FD_SET(sock, &fds);
      res = select(sock + 1, NULL, &fds, NULL, &tv);
      if (res > 0) {
        int so_err;
        socklen_t l = sizeof so_err;
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_err, &l);
        if (so_err == 0) { close(sock); return 1; }
      }
    }
  } else {
    close(sock);
    return 1;
  }
  close(sock);
  return 0;
}

static cJSON *scan_ports(const char *host) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "host", host);
  static const int common_ports[] = {
    21,22,23,25,53,80,110,143,443,445,
    993,995,1433,1521,3306,3389,5432,5900,
    6379,8080,8443,27017
  };
  static const char *port_names[] = {
    "FTP","SSH","Telnet","SMTP","DNS","HTTP","POP3","IMAP","HTTPS","SMB",
    "IMAPS","POP3S","MSSQL","Oracle","MySQL","RDP","PostgreSQL","VNC",
    "Redis","HTTP-Alt","HTTPS-Alt","MongoDB"
  };
  int np = (int)(sizeof common_ports / sizeof common_ports[0]);
  cJSON *open_ports = cJSON_CreateArray();
  int open_count = 0;
  for (int i = 0; i < np; i++) {
    if (check_port(host, common_ports[i], 1000)) {
      cJSON *pi = cJSON_CreateObject();
      cJSON_AddNumberToObject(pi, "port", common_ports[i]);
      cJSON_AddStringToObject(pi, "service", port_names[i]);
      cJSON_AddStringToObject(pi, "state", "open");
      cJSON_AddItemToArray(open_ports, pi);
      open_count++;
    }
  }
  cJSON_AddItemToObject(result, "open_ports", open_ports);
  cJSON_AddNumberToObject(result, "ports_scanned", np);
  cJSON_AddNumberToObject(result, "open_count", open_count);
  return result;
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

static int run_ports(const source_ctx *ctx, intel_sink *sink) {
  const char *host = ctx->entity;
  if (!host || !*host) return -1;
  cJSON *root = cJSON_CreateObject();
  cJSON *results = cJSON_CreateArray();
  cJSON *sr = scan_ports(host);
  cJSON *oc = cJSON_GetObjectItem(sr, "open_count");
  int total_open = oc ? oc->valueint : 0;
  cJSON_AddItemToArray(results, sr);
  cJSON_AddStringToObject(root, "service", "PORT_SCANNER");
  cJSON_AddNumberToObject(root, "hosts_scanned", 1);
  cJSON_AddNumberToObject(root, "total_open_ports", total_open);
  cJSON_AddItemToObject(root, "results", results);
  cJSON_AddStringToObject(root, "note",
    "Port scanning should only be performed on systems you own or have permission to test");
  return emit_one(sink, "PORT_SCANNER", host, 85, root);
}

static const source_def port_scanner_def = {
  .id = "PORT_SCANNER", .collector = "osint",
  .name = "Port Scanner", .name_ja = "ポートスキャナ",
  .update_interval_sec = 0, .run = run_ports,
};
REGISTER_SOURCE(port_scanner_def)
