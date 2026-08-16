/* collectors/sources/global_adsb.c
 * OSINT service — ADSB_GLOBAL. On-demand aircraft pivot against adsb.lol's
 * keyless global feed. ctx->entity is either a callsign/flight (letters+digits,
 * e.g. "JAL1") or an aircraft registration / tail number (e.g. "JA736J").
 * Fetch: GET https://api.adsb.lol/v2/callsign/<entity>  (callsign match) and,
 * when nothing matches, GET /v2/registration/<entity> (tail match). One
 * intel_item per live aircraft in the "ac" array — real hex, flight, position,
 * altitude and type; has_geo set when lat/lon are present. No live aircraft →
 * emit nothing (honest empty). Keyless LIVE. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* trim leading/trailing ASCII whitespace in place; returns s */
static char *adsb_trim(char *s) {
  if (!s) return s;
  while (*s && isspace((unsigned char)*s)) s++;
  char *e = s + strlen(s);
  while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
  return s;
}

/* one aircraft object → one intel_item. Returns 1 if emitted. */
static int emit_aircraft(intel_sink *sink, const cJSON *a, const char *entity) {
  if (!a || !cJSON_IsObject(a)) return 0;
  const char *hex  = jo_sv(a, "hex");
  const char *reg  = jo_sv(a, "r");     /* registration / tail */
  const char *type = jo_sv(a, "t");     /* aircraft type code (e.g. B77W) */

  char flight[32] = {0};
  const char *fl = jo_sv(a, "flight");
  if (fl) { snprintf(flight, sizeof flight, "%s", fl); adsb_trim(flight); }

  const cJSON *jlat = cJSON_GetObjectItem(a, "lat");
  const cJSON *jlon = cJSON_GetObjectItem(a, "lon");
  int has_geo = jlat && jlon && cJSON_IsNumber(jlat) && cJSON_IsNumber(jlon);
  double lat = has_geo ? jlat->valuedouble : 0;
  double lon = has_geo ? jlon->valuedouble : 0;

  const cJSON *jalt = cJSON_GetObjectItem(a, "alt_baro"); /* number or "ground" */

  if (!hex && !flight[0] && !reg) return 0;   /* nothing real to key on */

  cJSON *data = cJSON_CreateObject();
  if (hex)       cJSON_AddStringToObject(data, "hex", hex);
  if (flight[0]) cJSON_AddStringToObject(data, "flight", flight);
  if (reg)       cJSON_AddStringToObject(data, "registration", reg);
  if (type)      cJSON_AddStringToObject(data, "type", type);
  if (has_geo) {
    cJSON_AddNumberToObject(data, "latitude", lat);
    cJSON_AddNumberToObject(data, "longitude", lon);
  }
  if (jalt) {
    if (cJSON_IsNumber(jalt)) cJSON_AddNumberToObject(data, "alt_baro_ft", jalt->valuedouble);
    else if (cJSON_IsString(jalt)) cJSON_AddStringToObject(data, "alt_baro", jalt->valuestring);
  }
  const cJSON *gs = cJSON_GetObjectItem(a, "gs");
  if (gs && cJSON_IsNumber(gs)) cJSON_AddNumberToObject(data, "ground_speed_kt", gs->valuedouble);
  const cJSON *trk = cJSON_GetObjectItem(a, "track");
  if (trk && cJSON_IsNumber(trk)) cJSON_AddNumberToObject(data, "track", trk->valuedouble);
  const char *sq = jo_sv(a, "squawk");
  if (sq) cJSON_AddStringToObject(data, "squawk", sq);
  cJSON_AddStringToObject(data, "source", "adsb.lol");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "ADSB_GLOBAL");
  cJSON_AddStringToObject(props, "source", "adsb.lol");
  if (entity)    cJSON_AddStringToObject(props, "entity", entity);
  if (hex)       cJSON_AddStringToObject(props, "hex", hex);
  if (reg)       cJSON_AddStringToObject(props, "registration", reg);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char title[96];
  if (flight[0] && hex)      snprintf(title, sizeof title, "Flight %s (%s)", flight, hex);
  else if (flight[0])        snprintf(title, sizeof title, "Flight %s", flight);
  else if (reg && hex)       snprintf(title, sizeof title, "Aircraft %s (%s)", reg, hex);
  else if (hex)              snprintf(title, sizeof title, "Aircraft %s", hex);
  else                       snprintf(title, sizeof title, "Aircraft %s", reg);

  char rk[64];
  snprintf(rk, sizeof rk, "adsb:%s", hex ? hex : (reg ? reg : flight));

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.summary         = "live aircraft state";
  it.body            = bj;
  it.has_geo         = has_geo;
  it.lat             = lat;
  it.lon             = lon;
  it.record_type     = "osint_service_result";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"ADSB_GLOBAL\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Fetch one adsb.lol endpoint and emit every aircraft in "ac". Returns count. */
static int fetch_and_emit(const source_ctx *ctx, intel_sink *sink,
                          const char *url, const char *entity) {
  char *body = jo_get(ctx, url, NULL, "adsb_global");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  int emitted = 0;
  cJSON *ac = cJSON_GetObjectItem(root, "ac");
  if (cJSON_IsArray(ac)) {
    cJSON *a; cJSON_ArrayForEach(a, ac) emitted += emit_aircraft(sink, a, entity);
  }
  cJSON_Delete(root);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;

  char *enc = jo_urlencode(q);
  if (!enc) return -1;

  char url[512];
  snprintf(url, sizeof url, "https://api.adsb.lol/v2/callsign/%s", enc);
  int emitted = fetch_and_emit(ctx, sink, url, q);

  /* No callsign hit → try registration/tail lookup. */
  if (emitted == 0) {
    snprintf(url, sizeof url, "https://api.adsb.lol/v2/registration/%s", enc);
    emitted += fetch_and_emit(ctx, sink, url, q);
  }
  free(enc);

  fprintf(stderr, "[adsb_global] emitted %d\n", emitted);
  return 0;   /* honest empty is not an error */
}

static const source_def global_adsb_def = {
  .id = "ADSB_GLOBAL", .collector = "osint",
  .name = "Global ADS-B Aircraft", .name_ja = "グローバルADS-B航空機",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "api",
  .url = "internal://osint/adsb-global",
  .description = "adsb.lol global live aircraft by callsign or registration (keyless).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(global_adsb_def)
