/* collectors/sources/cyi_vpngate_servers.c
 * VPN Gate (University of Tsukuba) public relay list — volunteer-run VPN exit
 * servers with live IP, country, session count and operator.
 * Endpoint: https://www.vpngate.net/api/iphone/                       (keyless)
 * TRAPS (parse_notes): "CSV after a '*vpn_servers' marker line and a '#' header
 * line; ends with '*'. The last column is a base64 OpenVPN config — ~13 KB per
 * row, so skip that column or the payload balloons to 1.28 MB for 97 rows."
 * Both are handled: the marker/terminator lines are skipped, the header row is
 * read for column names, and the OpenVPN_ConfigData_Base64 column is dropped on
 * the floor — never emitted.
 * VPN Gate publishes no coordinates -> has_geo 0 (R2).
 * Licence: VPN Gate (University of Tsukuba) public API, academic project.
 */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://www.vpngate.net/api/iphone/"
#define MAXCOL 32

static char *next_line(char **p) {
  char *s = *p;
  if (!s || !*s) return NULL;
  char *e = strchr(s, '\n');
  if (e) { *e = '\0'; *p = e + 1; } else { *p = s + strlen(s); }
  size_t n = strlen(s);
  while (n && (s[n - 1] == '\r')) s[--n] = '\0';
  return s;
}

/* split on commas in place; returns field count */
static int split(char *line, char *out[], int max) {
  int n = 0;
  char *tok = line;
  while (n < max) {
    out[n++] = tok;
    char *c = strchr(tok, ',');
    if (!c) break;
    *c = '\0';
    tok = c + 1;
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, CYI_URL, 40000);
  if (!body) { fprintf(stderr, "[vpngate-servers] fetch failed\n"); return -1; }

  char *cols[MAXCOL] = {0};
  int ncols = 0;
  int n = 0;
  char *cur = body, *line;
  while ((line = next_line(&cur)) != NULL) {
    if (!line[0]) continue;
    if (line[0] == '*') continue;              /* *vpn_servers marker / final * */
    if (line[0] == '#') {                      /* '#'-prefixed column-name row */
      ncols = split(line + 1, cols, MAXCOL);
      continue;
    }
    if (ncols == 0) continue;                  /* data before the header */

    char *f[MAXCOL] = {0};
    int nf = split(line, f, MAXCOL);
    if (nf < 3) continue;

    cJSON *p = cJSON_CreateObject();
    const char *host = NULL, *ip = NULL, *ccl = NULL, *ccs = NULL, *op = NULL;
    for (int i = 0; i < nf && i < ncols; i++) {
      const char *name = cols[i];
      const char *val = f[i];
      if (!name || !val || !val[0]) continue;
      /* never persist the base64 OpenVPN config column */
      if (strstr(name, "OpenVPN_ConfigData")) continue;
      cJSON_AddStringToObject(p, name, val);
      if (strcmp(name, "HostName") == 0)          host = val;
      else if (strcmp(name, "IP") == 0)           ip = val;
      else if (strcmp(name, "CountryLong") == 0)  ccl = val;
      else if (strcmp(name, "CountryShort") == 0) ccs = val;
      else if (strcmp(name, "Operator") == 0)     op = val;
    }
    if (!ip) { cJSON_Delete(p); continue; }     /* no address -> no row */
    cJSON_AddStringToObject(p, "network", "vpngate");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[160];
    snprintf(title, sizeof title, "%s (%s)", ip, host ? host : "vpngate");
    char summary[320];
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             ccl ? ccl : (ccs ? ccs : ""),
             (ccl || ccs) && op ? " · " : "", op ? op : "",
             host ? " · " : "", host ? host : "");

    intel_item it = {0};
    it.remote_key      = ip;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://www.vpngate.net/en/";
    it.lang            = "en";
    it.record_type     = "vpn-exit-node";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"vpn\",\"vpngate\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[vpngate-servers] emitted %d\n", n);
  return 0;
}

static const source_def cyi_vpngate_servers_def = {
  .id = "vpngate-servers", .collector = "cyber",
  .name = "VPN Gate public relay list",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api", .url = CYI_URL,
  .description = "Volunteer-run VPN Gate exit servers with live IP, country, session count and operator — the IPs that turn up constantly in abuse investigations.",
  .license = "VPN Gate (University of Tsukuba) public API, academic project.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_vpngate_servers_def)
