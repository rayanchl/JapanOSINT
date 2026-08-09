/* collectors/sources/mar_digitraffic_port_calls.c
 * Fintraffic Digitraffic — Finnish port calls (SafeSeaNet notifications).
 *
 * Endpoint: https://meri.digitraffic.fi/api/port-call/v1/port-calls
 * Emits (all fetched): portCallId, portCallTimestamp, vesselName, imoLloyds,
 *   mmsi, radioCallSign, portToVisit / prevPort / nextPort LOCODEs,
 *   nationality, vesselTypeCode, currentSecurityLevel and the first agent name
 *   from agentInfo[].
 * Keyless. Licence: Fintraffic Digitraffic, CC BY 4.0.
 *
 * parse_notes trap — "No coordinates in the payload; keep has_geo 0. Do NOT
 * resolve portToVisit into a lat/lon from the ports endpoint and present it as
 * the vessel's position — that is exactly the port-centroid-as-vessel-position
 * error." No row here sets has_geo (R2).
 * parse_notes — "prevPort/nextPort are LOCODEs and may be blank": blanks are
 * dropped rather than emitted as empty strings.
 * parse_notes trap — Accept-Encoding: gzip is required; the platform HTTP
 * layer already sends it, so no custom header is added.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define PC_URL "https://meri.digitraffic.fi/api/port-call/v1/port-calls"

/* Digitraffic pads unset text fields with a single space; treat as missing. */
static inline const char *nonblank(const cJSON *o, const char *k) {
  const char *s = jo_sv(o, k);
  if (!s) return NULL;
  const char *p = s;
  while (*p == ' ' || *p == '\t') p++;
  return *p ? s : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, PC_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[digitraffic-port-calls] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_GetObjectItem(doc, "portCalls");
  if (!cJSON_IsArray(arr) && cJSON_IsArray(doc)) arr = doc;
  int n = 0;
  cJSON *c;
  cJSON_ArrayForEach(c, arr) {
    if (!cJSON_IsObject(c)) continue;
    const cJSON *idv = cJSON_GetObjectItem(c, "portCallId");
    if (!cJSON_IsNumber(idv)) continue;
    char pcid[32];
    snprintf(pcid, sizeof pcid, "%lld", (long long)idv->valuedouble);

    const char *vessel = nonblank(c, "vesselName");
    const char *port   = nonblank(c, "portToVisit");
    const char *when   = nonblank(c, "portCallTimestamp");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "portCallId", pcid);
    if (vessel) cJSON_AddStringToObject(p, "vesselName", vessel);
    if (port)   cJSON_AddStringToObject(p, "portToVisit", port);
    const char *s;
    if ((s = nonblank(c, "prevPort")))      cJSON_AddStringToObject(p, "prevPort", s);
    if ((s = nonblank(c, "nextPort")))      cJSON_AddStringToObject(p, "nextPort", s);
    if ((s = nonblank(c, "radioCallSign"))) cJSON_AddStringToObject(p, "radioCallSign", s);
    if ((s = nonblank(c, "nationality")))   cJSON_AddStringToObject(p, "nationality", s);
    if ((s = nonblank(c, "vesselNamePrefix"))) cJSON_AddStringToObject(p, "vesselNamePrefix", s);
    if (when) cJSON_AddStringToObject(p, "portCallTimestamp", when);
    jo_copy_num(p, c, "imoLloyds");
    jo_copy_num(p, c, "mmsi");
    jo_copy_num(p, c, "vesselTypeCode");
    jo_copy_num(p, c, "currentSecurityLevel");
    const cJSON *ag = cJSON_GetObjectItem(c, "agentInfo");
    if (cJSON_IsArray(ag)) {
      const cJSON *a;
      cJSON_ArrayForEach(a, ag) {
        const char *an = nonblank(a, "name");
        if (an) { cJSON_AddStringToObject(p, "agent", an); break; }
      }
    }
    cJSON_AddStringToObject(p, "note",
      "port call notification; contains no vessel position");
    cJSON_AddStringToObject(p, "source", "Fintraffic Digitraffic / SafeSeaNet");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[200], summary[220], link[160];
    snprintf(title, sizeof title, "Port call %s%s%s",
             vessel ? vessel : pcid,
             port ? " at " : "", port ? port : "");
    const char *pv = nonblank(c, "prevPort"), *nx = nonblank(c, "nextPort");
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             pv ? "from " : "", pv ? pv : "",
             (pv && nx) ? " · " : "",
             nx ? "next " : "", nx ? nx : "");
    snprintf(link, sizeof link, "%s?portCallId=%s", PC_URL, pcid);

    intel_item it = {0};
    it.remote_key      = pcid;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.link            = link;
    it.published_at    = when;
    it.record_type     = "port-call";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"port-call\",\"safeseanet\"]";
    /* no has_geo (R2): the payload carries no coordinate */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[digitraffic-port-calls] emitted %d\n", n);
  return 0;
}

static const source_def mar_digitraffic_port_calls_def = {
  .id = "digitraffic-port-calls", .collector = "maritime",
  .name = "Fintraffic Digitraffic — Finnish port calls (SafeSeaNet)",
  .update_interval_sec = 900, .run = run,
  .category = "transport", .type = "api", .url = PC_URL,
  .description = "Official port-call notifications: vessel identity (IMO/MMSI/call sign), the LOCODE visited, previous and next declared port, flag, ship agent and security level — a movement trail independent of AIS.",
  .license = "Fintraffic Digitraffic, CC BY 4.0",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_digitraffic_port_calls_def)
