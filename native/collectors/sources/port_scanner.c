/* collectors/osint/sources/port_scanner.c
 * OSINT service — faithful port of OSINTsaas osint_tools/network_utils.c
 * (PORT_SCANNER handle_port_scanner; ctx->entity = host).
 * On-demand (interval 0). No API keys. Reproduces scan_ports over the exact
 * common ports[]/service-names with a non-blocking connect()+select() probe.
 * The upstream handler loops over entities[]; the pipeline pivots one entity at
 * a time so it runs over ctx->entity (count==1) — the result envelope keeps the
 * upstream {service,total_*,...,results:[...]} shape with a single result.
 *
 * PER-RECORD EMIT: emits ONE intel_item per OPEN port
 * (remote_key="port:<host>:<port>"), body {host,port,service,state:"open"}.
 * Only open ports surface; if none are open, emits nothing, returns 0. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
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

/* Emit ONE intel_item for a single open port. Returns 1 if emitted. */
static int emit_open_port(intel_sink *sink, const char *host, int port,
                          const char *service) {
  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "host", host);
  cJSON_AddNumberToObject(data, "port", port);
  cJSON_AddStringToObject(data, "service", service);
  cJSON_AddStringToObject(data, "state", "open");
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "PORT_SCANNER");
  cJSON_AddStringToObject(props, "host", host);
  cJSON_AddNumberToObject(props, "port", port);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[320];
  snprintf(rk, sizeof rk, "port:%s:%d", host, port);
  char title[360];
  snprintf(title, sizeof title, "%s:%d (%s) open", host, port, service);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.body            = bj;
  it.summary         = service;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"PORT_SCANNER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(data); cJSON_Delete(props);
  return rc >= 0 ? 1 : 0;
}

static int run_ports(const source_ctx *ctx, intel_sink *sink) {
  const char *host = ctx->entity;
  if (!host || !*host) return -1;

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

  int emitted = 0;
  for (int i = 0; i < np; i++)
    if (check_port(host, common_ports[i], 1000))
      emitted += emit_open_port(sink, host, common_ports[i], port_names[i]);

  (void)emitted;
  return 0;                  /* no open ports → honest empty, not an error */
}

static const source_def port_scanner_def = {
  .id = "PORT_SCANNER", .collector = "osint",
  .name = "Port Scanner", .name_ja = "ポートスキャナ",
  .update_interval_sec = 0, .run = run_ports,
  .category = "cyber", .type = "api",
  .url = "internal://osint/port-scanner",
  .description = "Probe common TCP ports on a host via connect scan.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(port_scanner_def)
