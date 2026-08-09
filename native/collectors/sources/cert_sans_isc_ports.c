/* cert_sans_isc_ports.c — SANS Internet Storm Center / DShield top attacked
 * ports: the measured internet-wide scanning pressure that sits underneath the
 * advisory feeds.
 *
 * Endpoint: https://isc.sans.edu/api/topports/records/20?json   (keyless)
 * Shape trap: the body is a JSON OBJECT keyed by index string ("0", "1", …),
 *             NOT an array — recorded in the probe notes and handled here (an
 *             array body is accepted too, in case ISC changes it).
 *
 * Emits per port, straight from the sensor network: rank, targetport, the
 * packet `records` count, the number of distinct `targets` and distinct
 * `sources` observed. Numeric fields arrive as strings in this API, so both
 * shapes are read.
 *
 * The row key is the port, so a run updates the live ranking in place rather
 * than accumulating a new row per poll.
 *
 * R2: no geometry. DShield reports ports, not places; the sensor locations are
 * not published and a port number has no coordinate.
 * R3: fetch/parse failure -> -1; a fetched-but-empty ranking -> 0.
 *
 * Licence: ISC asks API users to identify themselves with a contact
 * User-Agent, which is set below. Keyless. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ISC_URL "https://isc.sans.edu/api/topports/records/20?json"

/* String value of a field that ISC may serve as a string or as a number. */
static int field_str(cJSON *o, const char *k, char *out, size_t n) {
  out[0] = 0;
  cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return 0;
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    snprintf(out, n, "%s", v->valuestring);
    return 1;
  }
  if (cJSON_IsNumber(v)) { snprintf(out, n, "%.0f", v->valuedouble); return 1; }
  return 0;
}

static int emit_port(intel_sink *s, cJSON *rec) {
  if (!cJSON_IsObject(rec)) return 0;
  char port[32], rank[32], records[48], targets[48], sources[48];
  if (!field_str(rec, "targetport", port, sizeof port)) return 0;  /* R1 */
  field_str(rec, "rank",    rank,    sizeof rank);
  field_str(rec, "records", records, sizeof records);
  field_str(rec, "targets", targets, sizeof targets);
  field_str(rec, "sources", sources, sizeof sources);

  char rk[64], title[320];
  snprintf(rk, sizeof rk, "topport-%s", port);
  snprintf(title, sizeof title,
           "DShield top attacked port %s%s%s%s - %s records from %s sources",
           port, rank[0] ? " (rank " : "", rank[0] ? rank : "",
           rank[0] ? ")" : "",
           records[0] ? records : "0", sources[0] ? sources : "0");

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "target_port", port);
  if (rank[0])    cJSON_AddStringToObject(p, "rank", rank);
  if (records[0]) cJSON_AddStringToObject(p, "records", records);
  if (targets[0]) cJSON_AddStringToObject(p, "targets", targets);
  if (sources[0]) cJSON_AddStringToObject(p, "sources", sources);
  cJSON_AddStringToObject(p, "source", "sans_isc_dshield_topports");
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.lang            = "en";
  it.record_type     = "scanning-telemetry";
  it.properties_json = pj ? pj : "{}";
  it.tags_json       = "[\"cyber\",\"telemetry\",\"dshield\",\"sans-isc\",\"ports\"]";
  int rc = s->emit(s, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run(const source_ctx *c, intel_sink *s) {
  const char *hdrs[] = {
    "accept: application/json",
    "User-Agent: JapanOSINT/1.0 (+https://github.com/RCorp/OSINTsaas; "
    "ISC API consumer; contact via repo issues)",
    NULL };
  cJSON *doc = feed_get_json_h(c->http, ISC_URL, hdrs, 20000);
  if (!doc) {
    fprintf(stderr, "[sans-isc-top-attacked-ports] fetch/parse failed\n");
    return -1;
  }
  int n = 0;
  cJSON *e;
  /* Object keyed "0","1",… — cJSON_ArrayForEach walks object children too, so
   * the same loop covers the array shape. */
  cJSON_ArrayForEach(e, doc) n += emit_port(s, e);
  cJSON_Delete(doc);
  fprintf(stderr, "[sans-isc-top-attacked-ports] emitted %d\n", n);
  return 0;                                       /* fetched fine (R3) */
}

static const source_def cert_isc_ports_def = {
  .id = "sans-isc-top-attacked-ports", .collector = "cyber",
  .name = "SANS Internet Storm Center - DShield Top Attacked Ports",
  .update_interval_sec = 3600, .run = run,
  .category = "cyber", .type = "api", .url = ISC_URL,
  .description = "Live DShield sensor-network ranking of the most-attacked "
                 "TCP/UDP ports with packet, target and unique-source counts - "
                 "measured internet-wide scanning pressure, the empirical "
                 "counterpart to advisory feeds.",
  .license = "ISC asks API users to set a contact User-Agent; keyless.",
  .free_tier = 1 };
REGISTER_SOURCE(cert_isc_ports_def)
