/* collectors/sources/mar_digitraffic_ais_vessels.c
 * Fintraffic Digitraffic — AIS message-5 static and voyage data. The identity
 * half of the AIS picture: MMSI -> IMO / name / call sign / ship type plus the
 * declared destination, draught and packed ETA.
 *
 * Endpoint: https://meri.digitraffic.fi/api/ais/v1/vessels
 * Emits (all fetched): mmsi, imo, name, callSign, destination, draught (raw
 *   decimetres + derived metres), eta (raw packed AIS field), shipType,
 *   posType, reference points A/B/C/D, timestamp (epoch ms -> ISO).
 * Keyless. Licence: Fintraffic Digitraffic, CC BY 4.0.
 *
 * parse_notes trap — "No geometry at all in this endpoint — has_geo must stay
 * 0. Do NOT geocode the 'destination' LOCODE into a position; it is a declared
 * intent, not a location." No row here sets has_geo (R2).
 * parse_notes — "draught is in decimetres (58 = 5.8 m); eta is the packed AIS
 * ETA field, not an epoch": both are carried verbatim and labelled.
 * parse_notes trap — the service requires Accept-Encoding: gzip; the platform
 * HTTP layer already sends it, so no custom header is added here.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "_jp_osint.inc"

#define VESSELS_URL "https://meri.digitraffic.fi/api/ais/v1/vessels"

static inline const char *iso_ms(double ms, char *buf, size_t n) {
  if (!(ms > 0)) return NULL;
  time_t t = (time_t)(ms / 1000.0);
  struct tm g;
  if (!gmtime_r(&t, &g)) return NULL;
  if (strftime(buf, n, "%Y-%m-%dT%H:%M:%SZ", &g) == 0) return NULL;
  return buf;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, VESSELS_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[digitraffic-ais-vessels] fetch/parse failed\n");
    return -1;
  }
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "vessels");
  int n = 0;
  cJSON *v;
  cJSON_ArrayForEach(v, arr) {
    if (!cJSON_IsObject(v)) continue;
    const cJSON *mm = cJSON_GetObjectItem(v, "mmsi");
    if (!cJSON_IsNumber(mm)) continue;
    char mmsi[32];
    snprintf(mmsi, sizeof mmsi, "%lld", (long long)mm->valuedouble);

    const char *name = jo_sv(v, "name");
    char stamp[40];
    const char *ts = NULL;
    const cJSON *tv = cJSON_GetObjectItem(v, "timestamp");
    if (cJSON_IsNumber(tv)) ts = iso_ms(tv->valuedouble, stamp, sizeof stamp);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "mmsi", mmsi);
    if (name) cJSON_AddStringToObject(p, "name", name);
    const char *cs = jo_sv(v, "callSign");
    if (cs) cJSON_AddStringToObject(p, "callSign", cs);
    const char *dest = jo_sv(v, "destination");
    if (dest) cJSON_AddStringToObject(p, "destination_declared", dest);
    jo_copy_num(p, v, "imo");
    jo_copy_num(p, v, "shipType");
    jo_copy_num(p, v, "posType");
    jo_copy_num(p, v, "referencePointA");
    jo_copy_num(p, v, "referencePointB");
    jo_copy_num(p, v, "referencePointC");
    jo_copy_num(p, v, "referencePointD");
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
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[160], summary[200], link[160];
    if (name) snprintf(title, sizeof title, "%s (MMSI %s)", name, mmsi);
    else      snprintf(title, sizeof title, "MMSI %s", mmsi);
    const cJSON *imo = cJSON_GetObjectItem(v, "imo");
    snprintf(summary, sizeof summary, "IMO %lld%s%s",
             cJSON_IsNumber(imo) ? (long long)imo->valuedouble : 0LL,
             dest ? " · destination " : "", dest ? dest : "");
    snprintf(link, sizeof link,
             "https://meri.digitraffic.fi/api/ais/v1/vessels/%s", mmsi);

    intel_item it = {0};
    it.remote_key      = mmsi;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.published_at    = ts;
    it.record_type     = "ais-vessel-static";
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"ais\",\"vessel\"]";
    /* no has_geo: this endpoint returns no coordinate at all (R2) */
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[digitraffic-ais-vessels] emitted %d\n", n);
  return 0;
}

static const source_def mar_digitraffic_ais_vessels_def = {
  .id = "digitraffic-ais-vessels", .collector = "maritime",
  .name = "Fintraffic Digitraffic — AIS static and voyage data",
  .update_interval_sec = 3600, .run = run,
  .category = "transport", .type = "api", .url = VESSELS_URL,
  .description = "AIS message-5 static/voyage records: MMSI to IMO/name/callsign mapping plus declared destination, draught and ETA — the join key for the AIS position feed. No coordinates in this endpoint.",
  .license = "Fintraffic Digitraffic, CC BY 4.0",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_digitraffic_ais_vessels_def)
