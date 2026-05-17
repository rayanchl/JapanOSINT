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
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
#include "../../../core/httpclient.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static const char *AIRLINE_CODES[][2] = {
  {"AAL", "American Airlines"}, {"UAL", "United Airlines"},
  {"DAL", "Delta Air Lines"}, {"SWA", "Southwest Airlines"},
  {"BAW", "British Airways"}, {"DLH", "Lufthansa"},
  {"AFR", "Air France"}, {"KLM", "KLM Royal Dutch"},
  {"UAE", "Emirates"}, {"QTR", "Qatar Airways"},
  {"SIA", "Singapore Airlines"}, {"CPA", "Cathay Pacific"},
  {"ANA", "All Nippon Airways"}, {"JAL", "Japan Airlines"},
  {"QFA", "Qantas"}, {NULL, NULL}
};

static const char *get_airline_name(const char *icao) {
  for (int i = 0; AIRLINE_CODES[i][0]; i++)
    if (strcasecmp(AIRLINE_CODES[i][0], icao) == 0) return AIRLINE_CODES[i][1];
  return NULL;
}

static cJSON *parse_icao24(const char *icao24) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "icao24", icao24);
  if (strlen(icao24) != 6) {
    cJSON_AddStringToObject(result, "note", "Invalid ICAO24 format");
    return result;
  }
  unsigned int addr = 0;
  sscanf(icao24, "%x", &addr);
  cJSON_AddNumberToObject(result, "address_decimal", addr);
  const char *country = "Unknown";
  if (addr >= 0xA00000 && addr <= 0xAFFFFF) country = "United States";
  else if (addr >= 0x400000 && addr <= 0x43FFFF) country = "United Kingdom";
  else if (addr >= 0x3C0000 && addr <= 0x3FFFFF) country = "Germany";
  else if (addr >= 0x380000 && addr <= 0x3BFFFF) country = "France";
  else if (addr >= 0x300000 && addr <= 0x33FFFF) country = "Italy";
  else if (addr >= 0x340000 && addr <= 0x37FFFF) country = "Spain";
  else if (addr >= 0x480000 && addr <= 0x4BFFFF) country = "Netherlands";
  else if (addr >= 0x500000 && addr <= 0x5FFFFF) country = "Australia";
  else if (addr >= 0x600000 && addr <= 0x6FFFFF) country = "China";
  else if (addr >= 0x780000 && addr <= 0x7BFFFF) country = "Japan";
  else if (addr >= 0x700000 && addr <= 0x70FFFF) country = "India";
  else if (addr >= 0x800000 && addr <= 0x83FFFF) country = "Russia";
  else if (addr >= 0x8A0000 && addr <= 0x8AFFFF) country = "Brazil";
  else if (addr >= 0xC00000 && addr <= 0xC3FFFF) country = "Canada";
  else if (addr >= 0x880000 && addr <= 0x887FFF) country = "South Korea";
  cJSON_AddStringToObject(result, "registration_country", country);
  return result;
}

/* query_opensky_aircraft: GET states/all?icao24=<icao24>, decode states[0]. */
static cJSON *query_opensky_aircraft(http_client *http, const char *icao24,
                                     int *out_has_data) {
  *out_has_data = 0;
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "source", "OpenSky Network");

  char url[256];
  snprintf(url, sizeof url,
           "https://opensky-network.org/api/states/all?icao24=%s", icao24);
  cJSON *json = feed_get_json(http, url, 30000);
  if (!json) {
    cJSON_AddStringToObject(result, "status", "not_found");
    cJSON_AddStringToObject(result, "note",
      "Aircraft not currently broadcasting position");
    return result;
  }

  cJSON *states = cJSON_GetObjectItem(json, "states");
  if (states && cJSON_IsArray(states) && cJSON_GetArraySize(states) > 0) {
    cJSON *state = cJSON_GetArrayItem(states, 0);
    if (state && cJSON_IsArray(state)) {
      cJSON *fi = cJSON_CreateObject();
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
      }
      if (altitude && cJSON_IsNumber(altitude)) {
        cJSON_AddNumberToObject(fi, "altitude_m", altitude->valuedouble);
        cJSON_AddNumberToObject(fi, "altitude_ft",
          altitude->valuedouble * 3.28084);
      }
      if (on_ground && cJSON_IsBool(on_ground))
        cJSON_AddBoolToObject(fi, "on_ground", cJSON_IsTrue(on_ground) ? 1 : 0);
      if (velocity && cJSON_IsNumber(velocity)) {
        cJSON_AddNumberToObject(fi, "velocity_ms", velocity->valuedouble);
        cJSON_AddNumberToObject(fi, "velocity_knots",
          velocity->valuedouble * 1.94384);
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
      cJSON_AddItemToObject(result, "current_state", fi);
      cJSON_AddStringToObject(result, "status", "tracking");
      *out_has_data = 1;
    }
  } else {
    cJSON_AddStringToObject(result, "status", "not_active");
  }

  cJSON_Delete(json);
  return result;
}

static cJSON *parse_flight_number(const char *fn) {
  cJSON *result = cJSON_CreateObject();
  cJSON_AddStringToObject(result, "flight_number", fn);
  char airline[4] = {0}, number[5] = {0};
  int lc = 0;
  while (fn[lc] && isalpha((unsigned char)fn[lc])) {
    if (lc < 3) airline[lc] = (char)toupper((unsigned char)fn[lc]);
    lc++;
  }
  if (lc >= 2 && lc <= 3) {
    strncpy(number, fn + lc, 4);
    cJSON_AddStringToObject(result, "airline_code", airline);
    cJSON_AddStringToObject(result, "flight_digits", number);
    if (lc == 2) {
      cJSON_AddStringToObject(result, "code_type", "IATA");
    } else {
      cJSON_AddStringToObject(result, "code_type", "ICAO");
      const char *name = get_airline_name(airline);
      if (name) cJSON_AddStringToObject(result, "airline_name", name);
    }
  } else {
    cJSON_AddStringToObject(result, "parse_status", "invalid_format");
  }
  return result;
}

/* Reproduce result_builder: append one {source,found,data,confidence,
 * detection_method,url?} item; tally found/total. */
static void rb_add(cJSON *results, const char *source, int found,
                   int confidence, cJSON *data /*owned*/, const char *url,
                   int *total, int *fc) {
  cJSON *it = cJSON_CreateObject();
  cJSON_AddStringToObject(it, "source", source);
  cJSON_AddBoolToObject(it, "found", found);
  if (data) cJSON_AddItemToObject(it, "data", data);
  else cJSON_AddNullToObject(it, "data");
  cJSON_AddNumberToObject(it, "confidence", confidence);
  cJSON_AddStringToObject(it, "detection_method", "direct");
  if (url) cJSON_AddStringToObject(it, "url", url);
  cJSON_AddItemToArray(results, it);
  (*total)++;
  if (found) (*fc)++;
}

static cJSON *flight_track(http_client *http, const char *query) {
  cJSON *results = cJSON_CreateArray();
  int total = 0, fc = 0;

  size_t len = strlen(query);
  int is_icao24 = (len == 6);
  for (size_t i = 0; is_icao24 && i < len; i++)
    if (!isxdigit((unsigned char)query[i])) is_icao24 = 0;

  if (is_icao24) {
    cJSON *icao = parse_icao24(query);
    rb_add(results, "ICAO24 Analysis", 1, 90 /*HIGH*/, icao, NULL, &total, &fc);
    int has_data = 0;
    cJSON *opensky = query_opensky_aircraft(http, query, &has_data);
    rb_add(results, "OpenSky Network", has_data, has_data ? 90 : 0,
           opensky, "https://opensky-network.org/", &total, &fc);
  } else {
    cJSON *fi = parse_flight_number(query);
    rb_add(results, "Flight Number Analysis", 1, 70 /*MEDIUM*/, fi, NULL,
           &total, &fc);
    cJSON *resources = cJSON_CreateObject();
    cJSON_AddStringToObject(resources, "source", "Tracking Resources");
    cJSON *links = cJSON_CreateArray();
    cJSON_AddItemToArray(links, cJSON_CreateString("https://www.flightradar24.com/"));
    cJSON_AddItemToArray(links, cJSON_CreateString("https://flightaware.com/"));
    cJSON_AddItemToArray(links, cJSON_CreateString("https://opensky-network.org/"));
    cJSON_AddItemToObject(resources, "links", links);
    rb_add(results, "FlightRadar24", 1, 70, resources,
           "https://www.flightradar24.com/", &total, &fc);
  }

  cJSON *root = cJSON_CreateObject();
  cJSON_AddStringToObject(root, "query", query);
  cJSON_AddStringToObject(root, "query_type", "flight");
  cJSON_AddItemToObject(root, "results", results);
  cJSON *summary = cJSON_CreateObject();
  cJSON_AddNumberToObject(summary, "total_sources", total);
  cJSON_AddNumberToObject(summary, "found_count", fc);
  cJSON *bd = cJSON_CreateObject();
  cJSON_AddNumberToObject(bd, "direct", total);
  cJSON_AddNumberToObject(bd, "llama", 0);
  cJSON_AddItemToObject(summary, "detection_breakdown", bd);
  cJSON_AddItemToObject(root, "summary", summary);
  return root;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;

  cJSON *data = flight_track(ctx->http, q);

  cJSON *env = cJSON_CreateObject();
  cJSON_AddBoolToObject(env, "success", 1);          /* OSINTsaas: always true */
  cJSON_AddNumberToObject(env, "confidence", 80);
  cJSON_AddItemToObject(env, "data", data);
  char *bj = cJSON_PrintUnformatted(env);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "FLIGHT_TRACKER");
  cJSON_AddStringToObject(props, "entity", q);
  cJSON_AddBoolToObject(props, "success", 1);
  cJSON_AddNumberToObject(props, "confidence", 80);
  char *pj = cJSON_PrintUnformatted(props);

  char rk[300];
  snprintf(rk, sizeof rk, "flight:%s", q);
  char title[320];
  snprintf(title, sizeof title, "FLIGHT_TRACKER — %s", q);

  intel_item it = {0};
  it.remote_key = rk;
  it.title = title;
  it.body = bj;
  it.summary = "flight lookup";
  it.record_type = "osint_service_result";
  it.properties_json = pj;
  it.tags_json = "[\"osint-search\",\"FLIGHT_TRACKER\"]";
  int rc = sink->emit(sink, &it);

  free(bj); free(pj);
  cJSON_Delete(env);
  return rc >= 0 ? 0 : -1;
}

static const source_def flight_tracker_def = {
  .id = "FLIGHT_TRACKER", .collector = "osint",
  .name = "Flight Tracker", .name_ja = "フライト追跡",
  .update_interval_sec = 0, .run = run,
};
REGISTER_SOURCE(flight_tracker_def)
