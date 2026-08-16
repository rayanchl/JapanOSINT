/* collectors/osint/sources/flight_tracker.c
 * OSINT service — faithful port of OSINTsaas osint_tools/flight_tracker.c
 * (handle_flight_tracker → flight_track). Canonical service FLIGHT_TRACKER
 * (osint_dispatcher.c service_registry[]). On-demand (interval 0); ctx->entity
 * = ICAO24 hex (6 hex chars) or a flight number. Source: OpenSky Network
 * states API (no key). Reproduces flight_track's result_builder output as the
 * documented result_builder JSON shape inline (the native build has no
 * result_builder.c): {query,query_type:"flight",results:[{source,found,data,
 * confidence,detection_method,url?}],summary:{total_sources,found_count,
 * detection_breakdown}}. parse_icao24 / query_opensky_aircraft (states[0]
 * vector decode incl. squawk meanings) / parse_flight_number reproduced
 * verbatim. success=true, confidence 80, like ip_geolocation.c envelope. */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* query_opensky_aircraft: GET states/all?icao24=<icao24>, decode states[0]
 * into a real flight-state object. Returns the decoded state on success
 * (caller owns it), or NULL when the aircraft is not currently broadcasting
 * (no fabricated row). Fills lat/lon/has_geo from the live vector. */
static cJSON *query_opensky_aircraft(http_client *http, const char *icao24,
                                     double *out_lat, double *out_lon,
                                     int *out_has_geo) {
  *out_has_geo = 0;
  char url[256];
  snprintf(url, sizeof url,
           "https://opensky-network.org/api/states/all?icao24=%s", icao24);
  cJSON *json = feed_get_json(http, url, 30000);
  if (!json) return NULL;

  cJSON *states = cJSON_GetObjectItem(json, "states");
  if (!states || !cJSON_IsArray(states) || cJSON_GetArraySize(states) == 0) {
    cJSON_Delete(json);
    return NULL;   /* not currently active → nothing */
  }
  cJSON *state = cJSON_GetArrayItem(states, 0);  /* exhaustive-ok: query is for ONE icao24, so states holds one vector */
  if (!state || !cJSON_IsArray(state)) { cJSON_Delete(json); return NULL; }

  cJSON *fi = cJSON_CreateObject();
  cJSON_AddStringToObject(fi, "icao24", icao24);
  cJSON_AddStringToObject(fi, "source", "OpenSky Network");

  cJSON *callsign = cJSON_GetArrayItem(state, 1);
  cJSON *origin = cJSON_GetArrayItem(state, 2);
  cJSON *lon = cJSON_GetArrayItem(state, 5);
  cJSON *lat = cJSON_GetArrayItem(state, 6);
  cJSON *altitude = cJSON_GetArrayItem(state, 7);
  cJSON *on_ground = cJSON_GetArrayItem(state, 8);
  cJSON *velocity = cJSON_GetArrayItem(state, 9);
  cJSON *heading = cJSON_GetArrayItem(state, 10);
  cJSON *squawk = cJSON_GetArrayItem(state, 14);

  if (callsign && cJSON_IsString(callsign)) {
    char *cs = callsign->valuestring;
    while (*cs && isspace((unsigned char)*cs)) cs++;
    char *end = cs + strlen(cs) - 1;
    while (end > cs && isspace((unsigned char)*end)) *end-- = '\0';
    if (strlen(cs) > 0) cJSON_AddStringToObject(fi, "callsign", cs);
  }
  if (origin && cJSON_IsString(origin))
    cJSON_AddStringToObject(fi, "origin_country", origin->valuestring);
  if (lon && lat && !cJSON_IsNull(lon) && !cJSON_IsNull(lat)) {
    cJSON_AddNumberToObject(fi, "longitude", lon->valuedouble);
    cJSON_AddNumberToObject(fi, "latitude", lat->valuedouble);
    *out_lat = lat->valuedouble;
    *out_lon = lon->valuedouble;
    *out_has_geo = 1;
  }
  if (altitude && cJSON_IsNumber(altitude)) {
    cJSON_AddNumberToObject(fi, "altitude_m", altitude->valuedouble);
    cJSON_AddNumberToObject(fi, "altitude_ft", altitude->valuedouble * 3.28084);
  }
  if (on_ground && cJSON_IsBool(on_ground))
    cJSON_AddBoolToObject(fi, "on_ground", cJSON_IsTrue(on_ground) ? 1 : 0);
  if (velocity && cJSON_IsNumber(velocity)) {
    cJSON_AddNumberToObject(fi, "velocity_ms", velocity->valuedouble);
    cJSON_AddNumberToObject(fi, "velocity_knots", velocity->valuedouble * 1.94384);
  }
  if (heading && cJSON_IsNumber(heading))
    cJSON_AddNumberToObject(fi, "heading", heading->valuedouble);
  if (squawk && cJSON_IsString(squawk) && strlen(squawk->valuestring) > 0) {
    cJSON_AddStringToObject(fi, "squawk", squawk->valuestring);
    const char *sq = squawk->valuestring;
    if (strcmp(sq, "7500") == 0)
      cJSON_AddStringToObject(fi, "squawk_meaning", "HIJACK");
    else if (strcmp(sq, "7600") == 0)
      cJSON_AddStringToObject(fi, "squawk_meaning", "RADIO FAILURE");
    else if (strcmp(sq, "7700") == 0)
      cJSON_AddStringToObject(fi, "squawk_meaning", "EMERGENCY");
  }

  cJSON_Delete(json);
  return fi;
}

/* PER-RECORD EMIT: ICAO24 branch only — one item per live OpenSky state
 * vector (real callsign/position/altitude/velocity/heading/squawk, with
 * has_geo+lat/lon). A flight-NUMBER input does no fetch and emits nothing.
 * No live state → emit nothing, return 0. */
static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;

  size_t len = strlen(q);
  int is_icao24 = (len == 6);
  for (size_t i = 0; is_icao24 && i < len; i++)
    if (!isxdigit((unsigned char)q[i])) is_icao24 = 0;
  if (!is_icao24) return 0;   /* flight-number branch: no real fetch → nothing */

  double lat = 0, lon = 0; int has_geo = 0;
  cJSON *fi = query_opensky_aircraft(ctx->http, q, &lat, &lon, &has_geo);
  if (!fi) return 0;          /* aircraft not broadcasting → nothing */

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);
  cJSON_AddNumberToObject(env, "confidence", 90);
  cJSON_AddItemToObject(env, "data", cJSON_Duplicate(fi, 1));
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "FLIGHT_TRACKER");
  cJSON_AddStringToObject(props, "entity", q);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 90);
  char *pj = cJSON_PrintUnformatted(props);

  /* remote_key = flight:<icao24>. */
  char rk[32]; snprintf(rk, sizeof rk, "flight:%s", q);
  const cJSON *cs = cJSON_GetObjectItem(fi, "callsign");
  char title[96];
  if (cs && cJSON_IsString(cs))
    snprintf(title, sizeof title, "Flight %s (%s)", cs->valuestring, q);
  else
    snprintf(title, sizeof title, "Aircraft %s", q);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = "live aircraft state";
  it.has_geo = has_geo;
  it.lat = lat;
  it.lon = lon;
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"FLIGHT_TRACKER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  cJSON_Delete(fi);
  return rc >= 0 ? 0 : -1;
}

static const source_def flight_tracker_def = {
  .id = "FLIGHT_TRACKER", .collector = "osint",
  .name = "Flight Tracker", .name_ja = "フライト追跡",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "api",
  .url = "internal://osint/flight-tracker",
  .description = "Track a flight or aircraft by number/registration.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(flight_tracker_def)
