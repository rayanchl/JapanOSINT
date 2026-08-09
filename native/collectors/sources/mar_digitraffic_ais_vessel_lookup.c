/* collectors/sources/mar_digitraffic_ais_vessel_lookup.c
 * OSINT service — DIGITRAFFIC_AIS_VESSEL. On-demand MMSI pivot
 * (ctx->entity = a 9-digit MMSI) against the Finnish AIS receiver network.
 *
 * Endpoint: https://meri.digitraffic.fi/api/ais/v1/vessels/<mmsi>
 * Emits (all fetched): mmsi, imo, name, callSign, destination (declared),
 *   draught (decimetres + derived metres), packed AIS eta, shipType, posType,
 *   timestamp (epoch ms -> ISO).
 * Keyless. Licence: Fintraffic Digitraffic, CC BY 4.0.
 *
 * parse_notes trap — "No coordinates returned; has_geo stays 0. Returns HTTP
 * 404 with a JSON error body when the MMSI has not been heard — treat as
 * honest empty (return 0), not a fetch failure. /api/ais/v1/locations/{mmsi}
 * does NOT exist (404) — positions are only available via the bbox/radius
 * locations endpoint." So: never a vessel position from here, and every
 * non-200 path returns 0.
 * parse_notes trap — Accept-Encoding: gzip is required; the platform HTTP
 * layer already sends it, so no custom header is added.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"

/* An MMSI is exactly 9 decimal digits. Anything else is not this pivot. */
static int looks_like_mmsi(const char *s) {
  if (!s) return 0;
  while (*s == ' ') s++;
  int d = 0;
  for (; *s; s++) {
    if (*s == ' ') break;
    if (!isdigit((unsigned char)*s)) return 0;
    d++;
  }
  while (*s == ' ') s++;
  return (*s == 0 && d == 9);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!looks_like_mmsi(q)) return 0;         /* wrong shape -> honest empty */

  char mmsi[16];
  size_t j = 0;
  for (const char *p = q; *p && j < sizeof mmsi - 1; p++)
    if (isdigit((unsigned char)*p)) mmsi[j++] = *p;
  mmsi[j] = 0;

  char url[160];
  snprintf(url, sizeof url,
           "https://meri.digitraffic.fi/api/ais/v1/vessels/%s", mmsi);

  char *body = jo_get(ctx, url, NULL, "DIGITRAFFIC_AIS_VESSEL");
  if (!body) return 0;              /* 404 = MMSI never heard: honest empty */
  cJSON *v = cJSON_Parse(body);
  free(body);
  if (!v || !cJSON_IsObject(v)) { cJSON_Delete(v); return 0; }
  if (!cJSON_IsNumber(cJSON_GetObjectItem(v, "mmsi"))) {
    cJSON_Delete(v);
    return 0;                                  /* error body, not a vessel */
  }

  const char *name = jo_sv(v, "name");
  const char *cs   = jo_sv(v, "callSign");
  const char *dest = jo_sv(v, "destination");

  char stamp[40];
  const char *ts = NULL;
  const cJSON *tv = cJSON_GetObjectItem(v, "timestamp");
  if (cJSON_IsNumber(tv) && tv->valuedouble > 0) {
    time_t t = (time_t)(tv->valuedouble / 1000.0);
    struct tm g;
    if (gmtime_r(&t, &g) &&
        strftime(stamp, sizeof stamp, "%Y-%m-%dT%H:%M:%SZ", &g) > 0)
      ts = stamp;
  }

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "DIGITRAFFIC_AIS_VESSEL");
  cJSON_AddStringToObject(p, "mmsi", mmsi);
  if (name) cJSON_AddStringToObject(p, "name", name);
  if (cs)   cJSON_AddStringToObject(p, "callSign", cs);
  if (dest) cJSON_AddStringToObject(p, "destination_declared", dest);
  jo_copy_num(p, v, "imo");
  jo_copy_num(p, v, "shipType");
  jo_copy_num(p, v, "posType");
  const cJSON *dr = cJSON_GetObjectItem(v, "draught");
  if (cJSON_IsNumber(dr)) {
    cJSON_AddNumberToObject(p, "draught_dm", dr->valuedouble);
    cJSON_AddNumberToObject(p, "draught_m", dr->valuedouble / 10.0);
  }
  const cJSON *eta = cJSON_GetObjectItem(v, "eta");
  if (cJSON_IsNumber(eta))
    cJSON_AddNumberToObject(p, "eta_ais_packed", eta->valuedouble);
  if (ts) cJSON_AddStringToObject(p, "static_report_time", ts);
  cJSON_AddStringToObject(p, "source", "Fintraffic Digitraffic AIS");
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char title[160], summary[220];
  if (name) snprintf(title, sizeof title, "%s (MMSI %s)", name, mmsi);
  else      snprintf(title, sizeof title, "MMSI %s", mmsi);
  const cJSON *imo = cJSON_GetObjectItem(v, "imo");
  snprintf(summary, sizeof summary, "AIS static: IMO %lld%s%s%s%s",
           cJSON_IsNumber(imo) ? (long long)imo->valuedouble : 0LL,
           cs ? " · call sign " : "", cs ? cs : "",
           dest ? " · destination " : "", dest ? dest : "");

  intel_item it = {0};
  it.remote_key      = mmsi;
  it.title           = title;
  it.summary         = summary;
  it.link            = url;
  it.published_at    = ts;
  it.record_type     = "ais-vessel-static";
  it.properties_json = pj ? pj : "{}";
  it.tags_json       = "[\"osint-search\",\"maritime\",\"ais\"]";
  /* deliberately no has_geo: this endpoint returns no coordinate */
  sink->emit(sink, &it);
  free(pj);
  cJSON_Delete(v);
  fprintf(stderr, "[DIGITRAFFIC_AIS_VESSEL] emitted 1 (%s)\n", mmsi);
  return 0;
}

static const source_def mar_digitraffic_ais_vessel_def = {
  .id = "DIGITRAFFIC_AIS_VESSEL", .collector = "osint",
  .name = "Digitraffic AIS vessel lookup by MMSI",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "api",
  .url = "https://meri.digitraffic.fi/api/ais/v1/vessels/",
  .description = "On-demand MMSI pivot: resolves an MMSI to IMO number, ship name, call sign, ship type and last declared destination from the Finnish AIS receiver network. No position is returned by this endpoint.",
  .license = "Fintraffic Digitraffic, CC BY 4.0",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_digitraffic_ais_vessel_def)
