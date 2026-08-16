/* collectors/sources/av_adsbdb.c
 * adsbdb (api.adsbdb.com) — three keyless entity-pivot services. One run()
 * dispatches on ctx->source_id; every emitted value is parsed from the live
 * JSON reply. All three payloads are wrapped: { "response": … }.
 *
 *   ADSBDB_AIRCRAFT  GET /v0/aircraft/{hex_or_registration}   (entity = tail)
 *       → response.aircraft.{type, icao_type, manufacturer, mode_s,
 *         registration, registered_owner, registered_owner_country_name,
 *         registered_owner_country_iso_name, registered_owner_operator_flag_code,
 *         url_photo, url_photo_thumbnail}. The SAME path accepts a Mode-S hex
 *         and a registration, so one collector serves both key forms.
 *       NO COORDINATES ANYWHERE — this is registry data. has_geo is always 0;
 *       the registered owner's country/address is never geocoded (R2).
 *
 *   ADSBDB_CALLSIGN  GET /v0/callsign/{callsign}              (entity = callsign)
 *       → response.flightroute.{callsign, callsign_icao, callsign_iata,
 *         airline{…}, origin{…}, destination{…}}.
 *       The ONLY coordinates present are AIRPORT coordinates
 *         (origin.latitude/longitude, destination.latitude/longitude).
 *       They are NEVER the aircraft's position. Two rows are emitted, one per
 *       airport, each pinned to that airport and tagged
 *       geo_precision:"airport"; no row is placed at a route midpoint.
 *
 *   ADSBDB_AIRLINE   GET /v0/airline/{icao_or_iata}           (entity = callsign)
 *       → response[] (an ARRAY — several operators can share a code) of
 *         {name, icao, iata, country, country_iso, callsign}.
 *       NO COORDINATES — `country` is not geocoded.
 *
 * Keyless. An unknown key returns HTTP 404 with a JSON error body; jo_get()
 * yields NULL on any non-200, which we treat as an honest empty (return 0),
 * not a fetch failure (R3). A NULL/empty/wrong-shape entity is a no-op.
 *
 * Licence: keyless open API (adsbdb, open-source project); no stated
 * prohibition on programmatic use. Route data is community-sourced and must be
 * presented as indicative, not authoritative.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "av_common.inc"

#define ADSBDB_LICENSE \
  "Keyless open API (adsbdb, open-source). Route data is community-sourced — " \
  "indicative, not authoritative."

/* Entity must be a bare alphanumeric-ish identifier (hex, N-number, callsign,
 * operator code). Anything with whitespace/slashes/@ is the wrong shape. */
static int adsbdb_key_ok(const char *s, size_t lo, size_t hi) {
  if (!s || !*s) return 0;
  size_t n = strlen(s);
  if (n < lo || n > hi) return 0;
  for (const char *p = s; *p; p++)
    if (!(isalnum((unsigned char)*p) || *p == '-')) return 0;
  return 1;
}

static cJSON *adsbdb_get(const source_ctx *ctx, const char *path,
                         const char *key, const char *tag) {
  char *enc = jo_urlencode(key);
  if (!enc) return NULL;
  char url[256];
  snprintf(url, sizeof url, "https://api.adsbdb.com/v0/%s/%s", path, enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json", AV_UA_HDR, NULL };
  char *body = jo_get(ctx, url, hdrs, tag);   /* 404 → NULL → honest empty */
  if (!body) return NULL;
  cJSON *doc = cJSON_Parse(body);
  free(body);
  return doc;
}

/* ------------------------------------------------------- ADSBDB_AIRCRAFT -- */
static int run_aircraft(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!adsbdb_key_ok(q, 3, 12)) return 0;
  cJSON *doc = adsbdb_get(ctx, "aircraft", q, "ADSBDB_AIRCRAFT");
  if (!doc) return 0;
  cJSON *resp = cJSON_GetObjectItem(doc, "response");
  cJSON *ac = resp ? cJSON_GetObjectItem(resp, "aircraft") : NULL;
  if (!cJSON_IsObject(ac)) { cJSON_Delete(doc); return 0; }

  const char *reg   = jo_sv(ac, "registration");
  const char *mode_s= jo_sv(ac, "mode_s");
  const char *type  = jo_sv(ac, "type");
  const char *itype = jo_sv(ac, "icao_type");
  const char *mfr   = jo_sv(ac, "manufacturer");
  const char *owner = jo_sv(ac, "registered_owner");
  const char *ctry  = jo_sv(ac, "registered_owner_country_name");
  const char *photo = jo_sv(ac, "url_photo");
  if (!reg && !mode_s) { cJSON_Delete(doc); return 0; }

  cJSON *data = cJSON_CreateObject();
  av_copy(data, ac, "registration");
  av_copy(data, ac, "mode_s");
  av_copy(data, ac, "manufacturer");
  av_copy(data, ac, "type");
  av_copy(data, ac, "icao_type");
  av_copy(data, ac, "registered_owner");
  av_copy(data, ac, "registered_owner_country_name");
  av_copy(data, ac, "registered_owner_country_iso_name");
  av_copy(data, ac, "registered_owner_operator_flag_code");
  av_copy(data, ac, "url_photo");
  av_copy(data, ac, "url_photo_thumbnail");
  cJSON_AddStringToObject(data, "source", "adsbdb");
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_Duplicate(data, 1);
  cJSON_AddStringToObject(props, "service", "ADSBDB_AIRCRAFT");
  cJSON_AddStringToObject(props, "query", q);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);
  cJSON_Delete(data);

  char title[256];
  snprintf(title, sizeof title, "%s%s%s%s%s", reg ? reg : mode_s,
           mfr ? " · " : "", mfr ? mfr : "", type ? " " : "", type ? type : "");
  char summary[320];
  snprintf(summary, sizeof summary, "%s%s%s",
           owner ? owner : (itype ? itype : ""),
           (owner && ctry) ? " · " : "", ctry ? ctry : "");

  intel_item it = {0};
  it.remote_key      = mode_s ? mode_s : reg;
  it.title           = title;
  it.summary         = summary[0] ? summary : NULL;
  it.body            = bj;
  it.link            = photo;                 /* real photo URL when returned */
  it.lang            = "en";
  it.record_type     = "aircraft-registry";
  /* NO GEO: registry data carries no coordinate, and the owner's country is
   * deliberately not geocoded (R2). */
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"aviation\",\"ADSBDB\"]";
  int n = sink->emit(sink, &it) >= 0 ? 1 : 0;
  free(bj); free(pj);
  cJSON_Delete(doc);
  fprintf(stderr, "[ADSBDB_AIRCRAFT] emitted %d (%s)\n", n, q);
  return 0;
}

/* ------------------------------------------------------- ADSBDB_CALLSIGN -- */
/* Emit one row per airport end of the route, pinned to THAT airport's own
 * coordinate and tagged geo_precision "airport". */
static int adsbdb_airport_row(intel_sink *sink, const cJSON *ap,
                              const char *role, const char *callsign,
                              const cJSON *airline) {
  if (!cJSON_IsObject(ap)) return 0;
  const char *icao = jo_sv(ap, "icao_code");
  const char *name = jo_sv(ap, "name");
  if (!icao && !name) return 0;
  double lat, lon;
  int geo = av_dbl(ap, "latitude", &lat) && av_dbl(ap, "longitude", &lon) &&
            lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180;

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "callsign", callsign);
  cJSON_AddStringToObject(data, "role", role);
  av_copy(data, ap, "icao_code");
  av_copy(data, ap, "iata_code");
  av_copy(data, ap, "name");
  av_copy(data, ap, "municipality");
  av_copy(data, ap, "country_name");
  av_copy(data, ap, "country_iso_name");
  av_copy(data, ap, "elevation");
  if (airline) {
    av_copy(data, airline, "name");
    av_copy(data, airline, "icao");
    av_copy(data, airline, "iata");
  }
  cJSON_AddStringToObject(data, "source", "adsbdb");
  char *bj = cJSON_PrintUnformatted(data);

  cJSON *props = cJSON_Duplicate(data, 1);
  cJSON_AddStringToObject(props, "service", "ADSBDB_CALLSIGN");
  /* The coordinate is the AIRPORT's, never the aircraft's position. */
  if (geo) cJSON_AddStringToObject(props, "geo_precision", "airport");
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);
  cJSON_Delete(data);

  char uid[160];
  snprintf(uid, sizeof uid, "%s|%s", callsign, icao ? icao : role);
  char title[288];
  snprintf(title, sizeof title, "%s %s: %s%s%s", callsign, role,
           icao ? icao : "", (icao && name) ? " " : "", name ? name : "");

  intel_item it = {0};
  it.remote_key      = uid;
  it.title           = title;
  it.summary         = jo_sv(ap, "municipality");
  it.body            = bj;
  it.lang            = "en";
  it.has_geo         = geo;
  it.lat             = geo ? lat : 0;
  it.lon             = geo ? lon : 0;
  it.record_type     = "flight-route-endpoint";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"aviation\",\"ADSBDB\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_callsign(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!adsbdb_key_ok(q, 3, 12)) return 0;
  cJSON *doc = adsbdb_get(ctx, "callsign", q, "ADSBDB_CALLSIGN");
  if (!doc) return 0;
  cJSON *resp = cJSON_GetObjectItem(doc, "response");
  cJSON *fr = resp ? cJSON_GetObjectItem(resp, "flightroute") : NULL;
  if (!cJSON_IsObject(fr)) { cJSON_Delete(doc); return 0; }

  const char *cs = jo_sv(fr, "callsign");
  if (!cs) cs = q;
  cJSON *airline = cJSON_GetObjectItem(fr, "airline");
  int n = 0;
  n += adsbdb_airport_row(sink, cJSON_GetObjectItem(fr, "origin"), "origin",
                          cs, airline);
  n += adsbdb_airport_row(sink, cJSON_GetObjectItem(fr, "destination"),
                          "destination", cs, airline);
  cJSON_Delete(doc);
  fprintf(stderr, "[ADSBDB_CALLSIGN] emitted %d (%s)\n", n, q);
  return 0;
}

/* -------------------------------------------------------- ADSBDB_AIRLINE -- */
static int run_airline(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!adsbdb_key_ok(q, 2, 4)) return 0;      /* ICAO(3) / IATA(2) operator */
  cJSON *doc = adsbdb_get(ctx, "airline", q, "ADSBDB_AIRLINE");
  if (!doc) return 0;
  cJSON *resp = cJSON_GetObjectItem(doc, "response");
  if (!cJSON_IsArray(resp)) { cJSON_Delete(doc); return 0; }

  int n = 0;
  cJSON *al;
  cJSON_ArrayForEach(al, resp) {
    const char *name = jo_sv(al, "name");
    const char *icao = jo_sv(al, "icao");
    if (!name) continue;

    cJSON *data = cJSON_CreateObject();
    av_copy(data, al, "name");
    av_copy(data, al, "icao");
    av_copy(data, al, "iata");
    av_copy(data, al, "country");
    av_copy(data, al, "country_iso");
    av_copy(data, al, "callsign");
    cJSON_AddStringToObject(data, "source", "adsbdb");
    char *bj = cJSON_PrintUnformatted(data);

    cJSON *props = cJSON_Duplicate(data, 1);
    cJSON_AddStringToObject(props, "service", "ADSBDB_AIRLINE");
    cJSON_AddStringToObject(props, "query", q);
    cJSON_AddBoolToObject(props, "success", 1);
    char *pj = cJSON_PrintUnformatted(props);
    cJSON_Delete(props);
    cJSON_Delete(data);

    char uid[96];
    snprintf(uid, sizeof uid, "%s", icao ? icao : name);
    char summary[256];
    snprintf(summary, sizeof summary, "%s%s%s",
             jo_sv(al, "country") ? jo_sv(al, "country") : "",
             jo_sv(al, "callsign") ? " · radio callsign " : "",
             jo_sv(al, "callsign") ? jo_sv(al, "callsign") : "");

    intel_item it = {0};
    it.remote_key      = uid;
    it.title           = name;
    it.summary         = summary[0] ? summary : NULL;
    it.body            = bj;
    it.lang            = "en";
    it.record_type     = "airline-operator";
    /* NO GEO — `country` is deliberately not geocoded (R2). */
    it.properties_json = pj;
    it.tags_json       = "[\"osint-search\",\"aviation\",\"ADSBDB\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(bj); free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[ADSBDB_AIRLINE] emitted %d (%s)\n", n, q);
  return 0;
}

static const source_def av_adsbdb_aircraft_def = {
  .id = "ADSBDB_AIRCRAFT", .collector = "osint",
  .name = "adsbdb Aircraft Registry (hex / tail)",
  .update_interval_sec = 0, .run = run_aircraft,
  .category = "transport", .type = "api",
  .url = "https://api.adsbdb.com/v0/aircraft/",
  .description = "Resolves a Mode-S hex OR a registration to type, ICAO type, manufacturer, registered owner and owner country, plus a photo URL. Registry data — no coordinates.",
  .license = ADSBDB_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_adsbdb_aircraft_def)

static const source_def av_adsbdb_callsign_def = {
  .id = "ADSBDB_CALLSIGN", .collector = "osint",
  .name = "adsbdb Callsign → Flight Route",
  .update_interval_sec = 0, .run = run_callsign,
  .category = "transport", .type = "api",
  .url = "https://api.adsbdb.com/v0/callsign/",
  .description = "Turns an observed ADS-B callsign into a route hypothesis: operating airline plus origin and destination airports with ICAO/IATA codes, municipality and the AIRPORT coordinates (never the aircraft position).",
  .license = ADSBDB_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_adsbdb_callsign_def)

static const source_def av_adsbdb_airline_def = {
  .id = "ADSBDB_AIRLINE", .collector = "osint",
  .name = "adsbdb Airline by Operator Code",
  .update_interval_sec = 0, .run = run_airline,
  .category = "transport", .type = "api",
  .url = "https://api.adsbdb.com/v0/airline/",
  .description = "Expands the ICAO/IATA operator code prefixing any observed callsign into the operating airline, its country and its radiotelephony callsign.",
  .license = ADSBDB_LICENSE, .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_adsbdb_airline_def)
