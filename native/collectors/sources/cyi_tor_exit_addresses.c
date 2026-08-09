/* collectors/sources/cyi_tor_exit_addresses.c
 * Tor exit address list WITH relay fingerprints — the value this adds over the
 * flat torbulkexitlist already in the fleet is the ExitNode fingerprint and the
 * per-address LastStatus timestamp, which is what lets an exit IP be joined to
 * Onionoo relay metadata.
 * Endpoint: https://check.torproject.org/exit-addresses               (keyless)
 * parse_notes: "Stanza format: ExitNode / Published / LastStatus / one-or-more
 * ExitAddress lines, blank-line separated." Parsed exactly that way: one row
 * per ExitAddress, carrying the stanza's fingerprint and timestamps.
 * No coordinates -> has_geo 0 (R2).
 * Licence: Tor Project public data (CC0-ish); attribution to the Tor Project.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CYI_URL "https://check.torproject.org/exit-addresses"

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *body = feed_get_text(ctx->http, CYI_URL, 30000);
  if (!body) { fprintf(stderr, "[tor-exit-addresses] fetch failed\n"); return -1; }

  char fingerprint[64] = "", published[40] = "", laststatus[40] = "";
  int n = 0;
  char *cur = body, *line;
  while ((line = jo_next_line(&cur)) != NULL) {
    if (!line[0]) continue;

    if (strncmp(line, "ExitNode ", 9) == 0) {
      snprintf(fingerprint, sizeof fingerprint, "%s", line + 9);
      published[0] = laststatus[0] = '\0';
      continue;
    }
    if (strncmp(line, "Published ", 10) == 0) {
      snprintf(published, sizeof published, "%s", line + 10);
      continue;
    }
    if (strncmp(line, "LastStatus ", 11) == 0) {
      snprintf(laststatus, sizeof laststatus, "%s", line + 11);
      continue;
    }
    if (strncmp(line, "ExitAddress ", 12) != 0) continue;

    /* "ExitAddress <ip> <YYYY-MM-DD> <HH:MM:SS>" */
    char ip[64] = "", seen[64] = "";
    const char *rest = line + 12;
    size_t i = 0;
    while (rest[i] && rest[i] != ' ' && i + 1 < sizeof ip) { ip[i] = rest[i]; i++; }
    ip[i] = '\0';
    while (rest[i] == ' ') i++;
    snprintf(seen, sizeof seen, "%s", rest + i);
    if (!ip[0] || !fingerprint[0]) continue;

    /* "2026-08-01 20:38:33" -> ISO-8601 */
    char iso[48] = "";
    if (strlen(seen) >= 19 && seen[10] == ' ') {
      snprintf(iso, sizeof iso, "%.10sT%.8sZ", seen, seen + 11);
    }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "ip", ip);
    cJSON_AddStringToObject(p, "exit_node_fingerprint", fingerprint);
    if (published[0])  cJSON_AddStringToObject(p, "published", published);
    if (laststatus[0]) cJSON_AddStringToObject(p, "last_status", laststatus);
    if (seen[0])       cJSON_AddStringToObject(p, "exit_address_seen", seen);
    cJSON_AddStringToObject(p, "network", "tor");
    cJSON_AddStringToObject(p, "role", "exit");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char summary[192];
    snprintf(summary, sizeof summary, "Tor exit · relay %s", fingerprint);
    char key[160];
    snprintf(key, sizeof key, "%s|%s", fingerprint, ip);
    char link[160];
    snprintf(link, sizeof link, "https://metrics.torproject.org/rs.html#details/%s",
             fingerprint);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = ip;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.published_at    = iso[0] ? iso : NULL;
    it.record_type     = "tor-exit-address";
    it.properties_json = pj;
    it.tags_json       = "[\"cyber\",\"tor\",\"anonymity\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }

  free(body);
  fprintf(stderr, "[tor-exit-addresses] emitted %d\n", n);
  return 0;
}

static const source_def cyi_tor_exit_addresses_def = {
  .id = "tor-exit-addresses", .collector = "cyber",
  .name = "Tor exit address list with node fingerprints",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "dataset", .url = CYI_URL,
  .description = "Tor exit addresses with the relay fingerprint and per-address timestamps — the fingerprint is what joins an exit IP to Onionoo relay metadata.",
  .license = "Tor Project public data (CC0-ish); attribution to the Tor Project.",
  .free_tier = 1,
};
REGISTER_SOURCE(cyi_tor_exit_addresses_def)
