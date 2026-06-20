/* collectors/transport/sources/plane_adsb.c — port of
 * server/src/utils/planeAdsbPoller.js (the OpenSky/adsb.lol live poller that
 * feeds server/src/collectors/unifiedFlights.js via getSnapshot()).
 *
 * The JS module is a STATEFUL 2-cadence background poller:
 *   • pollLive()  every 12 s — Promise.all([tryOpenSkyAPI, tryAdsbLol]) →
 *     mergeLiveByIcao(opensky, adsblol) → classifyMilitary → liveSnapshot map.
 *   • pollAeroDataBox() every 5 min — NRT/HND scheduled flights into a
 *     separate schedSnapshot, deduped against liveSnapshot by callsign.
 *   getSnapshot() = [...liveSnapshot, ...non-colliding schedSnapshot].
 *
 * The faithful C model of a request-response source is ONE pollLive()
 * snapshot per scheduler interval: a single OpenSky + 4-quadrant adsb.lol
 * fetch, ICAO24-merged (OpenSky first so adsb.lol's fresher position wins,
 * property bags unioned, source-tag concatenated), classifyMilitary stamped.
 * DROPPED (documented, not fabricated):
 *   - the AeroDataBox 5-min scheduled-flight cadence + its
 *     normalized-callsign dedupe against the live set (a distinct 2nd
 *     cadence; a per-interval snapshot cannot model it — and `flight-adsb`
 *     already covers AeroDataBox enrichment as its own registered source).
 *   - the in-memory liveSnapshot/schedSnapshot cross-poll state + stale-row
 *     GC + WS `live_vehicle` broadcast (no in-process map / WS in the C
 *     scheduler; each run re-emits the full current snapshot — equivalent
 *     for the `intel_items` master / unified-flights capture).
 * Curated SEED generator / `_meta` envelope intentionally not ported
 * (RULE 8 — live rows only).
 *
 * .id = "plane-adsb" so unified-flights' C port captures it via
 * registry_get("plane-adsb")->run() (mirrors unified_ais_ships.c's
 * unified_capture(ctx,"marine-traffic"/...,raw)). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ---- small helpers ------------------------------------------------------ */

static int is_num(const cJSON *v) { return v && cJSON_IsNumber(v); }

static cJSON *dup_or_null(const cJSON *v) {
  return v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull();
}

static cJSON *round_or_null(const cJSON *v) {
  return is_num(v) ? cJSON_CreateNumber(round(v->valuedouble)) : cJSON_CreateNull();
}

/* JS `(s || '').trim() || null` */
static char *trim_or_null(const char *s) {
  if (!s) return NULL;
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
  size_t len = strlen(s);
  while (len > 0 && (s[len-1]==' '||s[len-1]=='\t'||s[len-1]=='\n'||s[len-1]=='\r'))
    len--;
  if (len == 0) return NULL;
  char *out = malloc(len + 1);
  if (!out) return NULL;
  memcpy(out, s, len);
  out[len] = 0;
  return out;
}

static void to_lower(char *s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }

/* ---- military classification (port of _militaryIcao.js) ----------------- */

typedef struct { unsigned start, end; } mil_range;
static const mil_range MILITARY_RANGES[] = {
  { 0xae0000, 0xafffff }, /* USAF / USN / USA */
  { 0x43c000, 0x43cfff }, /* RAF */
  { 0xc00000, 0xc0ffff }, /* CAF (subset) */
  { 0x868000, 0x86ffff }, /* JASDF / JMSDF / JGSDF */
  { 0x7cf800, 0x7cffff }, /* RAAF */
  { 0x3ea000, 0x3ebfff }, /* Luftwaffe */
  { 0x3b7000, 0x3b7fff }, /* Armee de l Air */
  { 0x33ff00, 0x33ffff }, /* AMI */
  { 0x3443c0, 0x3443ff }, /* Ejercito del Aire */
  { 0x484800, 0x4848ff }, /* RNLAF */
  { 0x71be00, 0x71beff }, /* ROKAF */
};
static const char *CALLSIGN_PREFIXES[] = {
  "RCH","CNV","EVAC","SAM","JFR","JAPAN","PAT","REACH","DUKE","SHARK",
  "NAVY","RESCUE","CONVOY","HKY","VADER","RAID","PACK", NULL
};

static int is_mil_by_icao24(const char *icao24) {
  if (!icao24) return 0;
  char hex[16];
  size_t i = 0;
  const char *p = icao24;
  while (*p==' '||*p=='\t'||*p=='\n'||*p=='\r') p++;
  while (p[i] && i < sizeof hex - 1) { hex[i] = (char)tolower((unsigned char)p[i]); i++; }
  hex[i] = 0;
  size_t end = i;
  while (end > 0 && (hex[end-1]==' '||hex[end-1]=='\t'||hex[end-1]=='\n'||hex[end-1]=='\r'))
    hex[--end] = 0;
  if (strlen(hex) != 6) return 0;
  for (int k = 0; k < 6; k++)
    if (!isxdigit((unsigned char)hex[k])) return 0;
  unsigned n = (unsigned)strtoul(hex, NULL, 16);
  for (size_t r = 0; r < sizeof MILITARY_RANGES / sizeof MILITARY_RANGES[0]; r++)
    if (n >= MILITARY_RANGES[r].start && n <= MILITARY_RANGES[r].end) return 1;
  return 0;
}

/* CALLSIGN_RE: /^(PREFIX)\d/i — prefix immediately followed by a digit. */
static int is_mil_by_callsign(const char *cs) {
  if (!cs) return 0;
  while (*cs==' '||*cs=='\t'||*cs=='\n'||*cs=='\r') cs++;
  char up[64];
  size_t i = 0;
  for (; cs[i] && i < sizeof up - 1; i++) up[i] = (char)toupper((unsigned char)cs[i]);
  up[i] = 0;
  while (i > 0 && (up[i-1]==' '||up[i-1]=='\t'||up[i-1]=='\n'||up[i-1]=='\r')) up[--i]=0;
  for (const char **pf = CALLSIGN_PREFIXES; *pf; pf++) {
    size_t pl = strlen(*pf);
    if (strncmp(up, *pf, pl) == 0 && isdigit((unsigned char)up[pl])) return 1;
  }
  return 0;
}

/* Append {is_military, military_reason} to props (JS classifyMilitary).
 * Poller calls classifyMilitary({icao24, callsign: f.properties.callsign||null}).
 */
static void apply_military(cJSON *props) {
  cJSON *ic = cJSON_GetObjectItem(props, "icao24");
  cJSON *cs = cJSON_GetObjectItem(props, "callsign");
  const char *icao24 = (ic && cJSON_IsString(ic)) ? ic->valuestring : NULL;
  const char *callsign = (cs && cJSON_IsString(cs)) ? cs->valuestring : NULL;
  int mil = 0; const char *reason = NULL;
  if (is_mil_by_icao24(icao24)) { mil = 1; reason = "icao_range"; }
  else if (is_mil_by_callsign(callsign)) { mil = 1; reason = "callsign_prefix"; }
  cJSON_AddBoolToObject(props, "is_military", mil);
  if (reason) cJSON_AddStringToObject(props, "military_reason", reason);
  else cJSON_AddItemToObject(props, "military_reason", cJSON_CreateNull());
}

/* ---- OpenSky (live ADS-B) ----------------------------------------------- */

static const char *POSITION_SOURCE[] = { "ADS-B", "ASTERIX", "MLAT", "FLARM" };
static const char *CATEGORY_LABELS[] = {
  "No info", "No ADS-B category", "Light (<15500 lbs)",
  "Small (15500-75000 lbs)", "Large (75000-300000 lbs)",
  "High Vortex Large", "Heavy (>300000 lbs)", "High Performance",
  "Rotorcraft", "Glider/Sailplane", "Lighter-than-air",
  "Parachutist/Skydiver", "Ultralight/Paraglider", "Reserved", "UAV",
  "Space/Trans-atmospheric", "Emergency Vehicle", "Service Vehicle",
  "Point Obstacle", "Cluster Obstacle", "Line Obstacle",
};

/* OAuth2 client-credentials token (port of utils/openskyAuth.getOAuthToken);
 * NULL when creds absent or request fails. JS getOAuthToken() resolves null
 * without credentials and the fetch proceeds anonymously. */
static char *opensky_token(const source_ctx *ctx) {
  const char *cid = getenv("OPENSKY_CLIENT_ID");
  const char *csec = getenv("OPENSKY_CLIENT_SECRET");
  if (!cid || !*cid || !csec || !*csec) return NULL;
  char body[1024];
  snprintf(body, sizeof body,
    "grant_type=client_credentials&client_id=%s&client_secret=%s", cid, csec);
  const char *hdrs[] = { "Content-Type: application/x-www-form-urlencoded", NULL };
  cJSON *j = feed_post_json(ctx->http,
    "https://auth.opensky-network.org/auth/realms/opensky-network/protocol/openid-connect/token",
    body, hdrs, 15000);
  if (!j) return NULL;
  cJSON *at = cJSON_GetObjectItem(j, "access_token");
  char *tok = (at && cJSON_IsString(at)) ? strdup(at->valuestring) : NULL;
  cJSON_Delete(j);
  return tok;
}

/* Port of tryOpenSkyAPI(). Returns cJSON array of Features, or NULL on
 * failure (JS returns null → poller treats as source down). */
static cJSON *try_opensky(const source_ctx *ctx) {
  char *tok = opensky_token(ctx);
  cJSON *data = NULL;
  if (tok) {
    char auth[2048];
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", tok);
    const char *hdrs[] = { auth, NULL };
    data = feed_get_json_h(ctx->http,
      "https://opensky-network.org/api/states/all?lamin=24&lomin=122&lamax=46&lomax=154",
      hdrs, 10000);
    free(tok);
  } else {
    data = feed_get_json(ctx->http,
      "https://opensky-network.org/api/states/all?lamin=24&lomin=122&lamax=46&lomax=154",
      10000);
  }
  if (!data) return NULL;
  cJSON *states = cJSON_GetObjectItem(data, "states");
  if (!states || !cJSON_IsArray(states)) { cJSON_Delete(data); return NULL; }

  cJSON *out = cJSON_CreateArray();
  int i = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, states) {
    if (i >= 200) break;                              /* slice(0,200) */
    cJSON *s5 = cJSON_GetArrayItem(s, 5);
    cJSON *s6 = cJSON_GetArrayItem(s, 6);
    double lon = is_num(s5) ? s5->valuedouble : 139.7;
    double lat = is_num(s6) ? s6->valuedouble : 35.6;

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *p = cJSON_CreateObject();                  /* EXACT JS key order */
    char idbuf[32];
    snprintf(idbuf, sizeof idbuf, "ADSB_LIVE_%d", i);
    cJSON_AddStringToObject(p, "id", idbuf);
    cJSON_AddItemToObject(p, "icao24", dup_or_null(cJSON_GetArrayItem(s, 0)));
    cJSON *s1 = cJSON_GetArrayItem(s, 1);
    char *cs = trim_or_null((s1 && cJSON_IsString(s1)) ? s1->valuestring : "");
    /* JS: (s[1]||'').trim() — empty string when missing, not null */
    cJSON_AddStringToObject(p, "callsign", cs ? cs : "");
    free(cs);
    cJSON_AddItemToObject(p, "origin_country", dup_or_null(cJSON_GetArrayItem(s, 2)));
    cJSON_AddItemToObject(p, "time_position", dup_or_null(cJSON_GetArrayItem(s, 3)));
    cJSON_AddItemToObject(p, "last_contact", dup_or_null(cJSON_GetArrayItem(s, 4)));
    cJSON *s7 = cJSON_GetArrayItem(s, 7);
    cJSON_AddItemToObject(p, "baro_altitude_m", dup_or_null(s7));
    cJSON_AddItemToObject(p, "altitude_ft",
      is_num(s7) ? cJSON_CreateNumber(round(s7->valuedouble * 3.28084))
                 : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "on_ground", dup_or_null(cJSON_GetArrayItem(s, 8)));
    cJSON *s9 = cJSON_GetArrayItem(s, 9);
    cJSON_AddItemToObject(p, "velocity_mps", dup_or_null(s9));
    cJSON_AddItemToObject(p, "ground_speed_knots",
      is_num(s9) ? cJSON_CreateNumber(round(s9->valuedouble * 1.94384))
                 : cJSON_CreateNull());
    cJSON *s10 = cJSON_GetArrayItem(s, 10);
    cJSON_AddItemToObject(p, "heading",
      cJSON_CreateNumber(round(is_num(s10) ? s10->valuedouble : 0)));
    cJSON_AddItemToObject(p, "true_track",
      is_num(s10) ? cJSON_CreateNumber(round(s10->valuedouble))
                  : cJSON_CreateNull());
    cJSON *s11 = cJSON_GetArrayItem(s, 11);
    cJSON_AddItemToObject(p, "vertical_rate_fpm",
      is_num(s11) ? cJSON_CreateNumber(round(s11->valuedouble * 196.85))
                  : cJSON_CreateNull());
    cJSON *s13 = cJSON_GetArrayItem(s, 13);
    cJSON_AddItemToObject(p, "geo_altitude_m", dup_or_null(s13));
    cJSON_AddItemToObject(p, "geo_altitude_ft",
      is_num(s13) ? cJSON_CreateNumber(round(s13->valuedouble * 3.28084))
                  : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "squawk", dup_or_null(cJSON_GetArrayItem(s, 14)));
    cJSON_AddItemToObject(p, "spi", dup_or_null(cJSON_GetArrayItem(s, 15)));
    cJSON *s16 = cJSON_GetArrayItem(s, 16);
    if (is_num(s16) && s16->valueint >= 0 &&
        s16->valueint < (int)(sizeof POSITION_SOURCE / sizeof POSITION_SOURCE[0]))
      cJSON_AddStringToObject(p, "position_source", POSITION_SOURCE[s16->valueint]);
    else
      cJSON_AddItemToObject(p, "position_source", dup_or_null(s16));
    cJSON *s17 = cJSON_GetArrayItem(s, 17);
    if (is_num(s17)) {
      int ci = s17->valueint;
      if (ci >= 0 && ci < (int)(sizeof CATEGORY_LABELS / sizeof CATEGORY_LABELS[0]))
        cJSON_AddStringToObject(p, "category", CATEGORY_LABELS[ci]);
      else
        cJSON_AddItemToObject(p, "category", cJSON_Duplicate(s17, 1));
    } else {
      cJSON_AddItemToObject(p, "category", cJSON_CreateNull());
    }
    cJSON_AddStringToObject(p, "source", "opensky_api");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(out, f);
    i++;
  }
  cJSON_Delete(data);
  return out;
}

/* ---- adsb.lol (live ADS-B, 4 quadrants) --------------------------------- */

typedef struct { int lat, lon, dist; } quadrant;
static const quadrant ADSBLOL_QUADRANTS[] = {
  { 43, 142, 250 }, /* Hokkaido + northern Tohoku */
  { 36, 138, 250 }, /* central Honshu */
  { 32, 131, 250 }, /* Kyushu / Shikoku */
  { 26, 128, 250 }, /* Okinawa / Sakishima */
};
#define NQUAD ((int)(sizeof ADSBLOL_QUADRANTS / sizeof ADSBLOL_QUADRANTS[0]))

/* Port of tryAdsbLol(). Returns cJSON array of Features, or NULL on total
 * blackout (JS null when all quadrants fail). */
static cJSON *try_adsblol(const source_ctx *ctx) {
  cJSON *byIcaoKeys = cJSON_CreateArray();   /* parallel order list of hex   */
  cJSON *byIcaoVals = cJSON_CreateObject();  /* hex -> raw ac (last wins)    */
  int failed = 0;
  for (int q = 0; q < NQUAD; q++) {
    char url[128];
    snprintf(url, sizeof url, "https://api.adsb.lol/v2/lat/%d/lon/%d/dist/%d",
             ADSBLOL_QUADRANTS[q].lat, ADSBLOL_QUADRANTS[q].lon,
             ADSBLOL_QUADRANTS[q].dist);
    const char *hdrs[] = { "accept: application/json", NULL };
    cJSON *data = feed_get_json_h(ctx->http, url, hdrs, 8000);
    if (!data) { failed++; continue; }
    cJSON *ac = cJSON_GetObjectItem(data, "ac");
    if (cJSON_IsArray(ac)) {
      cJSON *a;
      cJSON_ArrayForEach(a, ac) {
        cJSON *hexv = cJSON_GetObjectItem(a, "hex");
        if (!hexv || !cJSON_IsString(hexv) || !*hexv->valuestring) continue;
        cJSON *latv = cJSON_GetObjectItem(a, "lat");
        cJSON *lonv = cJSON_GetObjectItem(a, "lon");
        if (!is_num(latv) || !is_num(lonv)) continue;
        char hex[32];
        snprintf(hex, sizeof hex, "%s", hexv->valuestring);
        to_lower(hex);
        if (!cJSON_GetObjectItem(byIcaoVals, hex))
          cJSON_AddItemToArray(byIcaoKeys, cJSON_CreateString(hex));
        cJSON *dupAc = cJSON_Duplicate(a, 1);
        cJSON_DeleteItemFromObject(byIcaoVals, hex);
        cJSON_AddItemToObject(byIcaoVals, hex, dupAc);
      }
    }
    cJSON_Delete(data);
  }
  if (failed == NQUAD) {
    cJSON_Delete(byIcaoKeys);
    cJSON_Delete(byIcaoVals);
    return NULL;
  }

  cJSON *out = cJSON_CreateArray();
  cJSON *k;
  cJSON_ArrayForEach(k, byIcaoKeys) {
    cJSON *ac = cJSON_GetObjectItem(byIcaoVals, k->valuestring);
    if (!ac) continue;
    cJSON *hexv = cJSON_GetObjectItem(ac, "hex");
    cJSON *latv = cJSON_GetObjectItem(ac, "lat");
    cJSON *lonv = cJSON_GetObjectItem(ac, "lon");
    const char *hex = hexv->valuestring;

    cJSON *flightv = cJSON_GetObjectItem(ac, "flight");
    char *callsign = trim_or_null((flightv && cJSON_IsString(flightv))
                                    ? flightv->valuestring : "");

    cJSON *trackv = cJSON_GetObjectItem(ac, "track");
    cJSON *thv = cJSON_GetObjectItem(ac, "true_heading");
    double heading;
    if (is_num(trackv)) heading = round(trackv->valuedouble);
    else if (is_num(thv)) heading = round(thv->valuedouble);
    else heading = 0;

    cJSON *abv = cJSON_GetObjectItem(ac, "alt_baro");
    int onGround = abv && cJSON_IsString(abv) &&
                   strcmp(abv->valuestring, "ground") == 0;

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *co = cJSON_CreateArray();
    cJSON_AddItemToArray(co, cJSON_CreateNumber(lonv->valuedouble));
    cJSON_AddItemToArray(co, cJSON_CreateNumber(latv->valuedouble));
    cJSON_AddItemToObject(g, "coordinates", co);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *p = cJSON_CreateObject();                  /* EXACT JS key order */
    char idbuf[64];
    snprintf(idbuf, sizeof idbuf, "ADSBLOL_%s", hex);
    cJSON_AddStringToObject(p, "id", idbuf);
    char lhex[32];
    snprintf(lhex, sizeof lhex, "%s", hex);
    to_lower(lhex);
    cJSON_AddStringToObject(p, "icao24", lhex);
    if (callsign) cJSON_AddStringToObject(p, "callsign", callsign);
    else cJSON_AddItemToObject(p, "callsign", cJSON_CreateNull());
    free(callsign);
    cJSON *rv = cJSON_GetObjectItem(ac, "r");
    cJSON_AddItemToObject(p, "registration",
      (rv && cJSON_IsString(rv) && *rv->valuestring) ? cJSON_Duplicate(rv,1)
                                                     : cJSON_CreateNull());
    cJSON *tv = cJSON_GetObjectItem(ac, "t");
    cJSON_AddItemToObject(p, "aircraft_type",
      (tv && cJSON_IsString(tv) && *tv->valuestring) ? cJSON_Duplicate(tv,1)
                                                     : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "altitude_ft",
      is_num(abv) ? cJSON_CreateNumber(abv->valuedouble) : cJSON_CreateNull());
    cJSON *agv = cJSON_GetObjectItem(ac, "alt_geom");
    cJSON_AddItemToObject(p, "geo_altitude_ft",
      is_num(agv) ? cJSON_CreateNumber(agv->valuedouble) : cJSON_CreateNull());
    cJSON *gsv = cJSON_GetObjectItem(ac, "gs");
    cJSON_AddItemToObject(p, "ground_speed_knots", round_or_null(gsv));
    cJSON *tasv = cJSON_GetObjectItem(ac, "tas");
    cJSON_AddItemToObject(p, "true_airspeed_knots", round_or_null(tasv));
    cJSON *iasv = cJSON_GetObjectItem(ac, "ias");
    cJSON_AddItemToObject(p, "indicated_airspeed_knots", round_or_null(iasv));
    cJSON *machv = cJSON_GetObjectItem(ac, "mach");
    cJSON_AddItemToObject(p, "mach",
      is_num(machv) ? cJSON_CreateNumber(machv->valuedouble) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "heading", cJSON_CreateNumber(heading));
    cJSON_AddItemToObject(p, "true_track", round_or_null(trackv));
    cJSON *mhv = cJSON_GetObjectItem(ac, "mag_heading");
    cJSON_AddItemToObject(p, "magnetic_heading", round_or_null(mhv));
    cJSON *brv = cJSON_GetObjectItem(ac, "baro_rate");
    cJSON *grv = cJSON_GetObjectItem(ac, "geom_rate");
    if (is_num(brv))
      cJSON_AddItemToObject(p, "vertical_rate_fpm", cJSON_CreateNumber(brv->valuedouble));
    else if (is_num(grv))
      cJSON_AddItemToObject(p, "vertical_rate_fpm", cJSON_CreateNumber(grv->valuedouble));
    else
      cJSON_AddItemToObject(p, "vertical_rate_fpm", cJSON_CreateNull());
    cJSON *sqv = cJSON_GetObjectItem(ac, "squawk");
    cJSON_AddItemToObject(p, "squawk",
      (sqv && cJSON_IsString(sqv) && *sqv->valuestring) ? cJSON_Duplicate(sqv,1)
                                                        : cJSON_CreateNull());
    cJSON *catv = cJSON_GetObjectItem(ac, "category");
    cJSON_AddItemToObject(p, "category",
      (catv && cJSON_IsString(catv) && *catv->valuestring) ? cJSON_Duplicate(catv,1)
                                                           : cJSON_CreateNull());
    cJSON *emv = cJSON_GetObjectItem(ac, "emergency");
    cJSON_AddItemToObject(p, "emergency",
      (emv && cJSON_IsString(emv) && *emv->valuestring) ? cJSON_Duplicate(emv,1)
                                                        : cJSON_CreateNull());
    cJSON_AddBoolToObject(p, "on_ground", onGround);
    cJSON *seenv = cJSON_GetObjectItem(ac, "seen");
    cJSON_AddItemToObject(p, "last_seen_s",
      is_num(seenv) ? cJSON_CreateNumber(seenv->valuedouble) : cJSON_CreateNull());
    cJSON *rssiv = cJSON_GetObjectItem(ac, "rssi");
    cJSON_AddItemToObject(p, "rssi",
      is_num(rssiv) ? cJSON_CreateNumber(rssiv->valuedouble) : cJSON_CreateNull());
    cJSON_AddStringToObject(p, "source", "adsblol_api");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(out, f);
  }
  cJSON_Delete(byIcaoKeys);
  cJSON_Delete(byIcaoVals);
  return out;
}

/* ---- merge by ICAO24 (port of mergeLiveByIcao) -------------------------- */

/* Order-preserving merge: opensky first, then adsblol (poller passes
 * mergeLiveByIcao(openskyFeatures, adsbLolFeatures)). Later sources overwrite
 * geometry/id/source; property bags unioned (prev keys keep their insertion
 * slot, this-source values overwrite, new keys appended); source-tag is
 * `a+b` when both present & differ else this||prev. */
static cJSON *merge_live_by_icao(cJSON *opensky, cJSON *adsblol) {
  cJSON *order = cJSON_CreateArray();        /* icao order list            */
  cJSON *map = cJSON_CreateObject();         /* icao -> merged Feature     */
  cJSON *lists[2] = { opensky, adsblol };
  for (int li = 0; li < 2; li++) {
    cJSON *list = lists[li];
    if (!list || !cJSON_IsArray(list)) continue;     /* JS: !Array → skip  */
    cJSON *f;
    cJSON_ArrayForEach(f, list) {
      cJSON *props = cJSON_GetObjectItem(f, "properties");
      cJSON *icv = props ? cJSON_GetObjectItem(props, "icao24") : NULL;
      if (!icv || !cJSON_IsString(icv) || !*icv->valuestring) continue;
      char icao[32];
      snprintf(icao, sizeof icao, "%s", icv->valuestring);
      to_lower(icao);
      cJSON_SetValuestring(icv, icao);       /* f.properties.icao24 = icao */

      cJSON *prev = cJSON_GetObjectItem(map, icao);
      cJSON *cur = cJSON_Duplicate(f, 1);
      cJSON *curProps = cJSON_GetObjectItem(cur, "properties");
      if (prev) {
        cJSON *prevProps = cJSON_GetObjectItem(prev, "properties");
        cJSON *pv = cJSON_GetObjectItem(prevProps, "source");
        cJSON *tv = cJSON_GetObjectItem(curProps, "source");
        const char *prevSrc = (pv && cJSON_IsString(pv)) ? pv->valuestring : NULL;
        const char *thisSrc = (tv && cJSON_IsString(tv)) ? tv->valuestring : NULL;
        /* { ...prev.properties, ...cur.properties }: start from prev order,
         * overwrite with cur values, then append cur-only keys. */
        cJSON *merged = cJSON_CreateObject();
        cJSON *pk;
        cJSON_ArrayForEach(pk, prevProps) {
          cJSON *override = cJSON_GetObjectItem(curProps, pk->string);
          cJSON_AddItemToObject(merged, pk->string,
            cJSON_Duplicate(override ? override : pk, 1));
        }
        cJSON *ck;
        cJSON_ArrayForEach(ck, curProps) {
          if (!cJSON_GetObjectItem(merged, ck->string))
            cJSON_AddItemToObject(merged, ck->string, cJSON_Duplicate(ck, 1));
        }
        /* source: prevSrc&&thisSrc&&prevSrc!==thisSrc ? a+b : this||prev */
        cJSON_DeleteItemFromObject(merged, "source");
        if (prevSrc && thisSrc && strcmp(prevSrc, thisSrc) != 0) {
          char buf[128];
          snprintf(buf, sizeof buf, "%s+%s", prevSrc, thisSrc);
          cJSON_AddStringToObject(merged, "source", buf);
        } else if (thisSrc) {
          cJSON_AddStringToObject(merged, "source", thisSrc);
        } else if (prevSrc) {
          cJSON_AddStringToObject(merged, "source", prevSrc);
        } else {
          cJSON_AddItemToObject(merged, "source", cJSON_CreateNull());
        }
        cJSON_ReplaceItemInObject(cur, "properties", merged);
      }
      if (cJSON_GetObjectItem(map, icao))
        cJSON_DeleteItemFromObject(map, icao);
      else
        cJSON_AddItemToArray(order, cJSON_CreateString(icao));
      cJSON_AddItemToObject(map, icao, cur);
    }
  }
  cJSON *out = cJSON_CreateArray();
  cJSON *k;
  cJSON_ArrayForEach(k, order) {
    cJSON *v = cJSON_GetObjectItem(map, k->valuestring);
    if (v) cJSON_AddItemToArray(out, cJSON_Duplicate(v, 1));
  }
  cJSON_Delete(order);
  cJSON_Delete(map);
  return out;
}

/* ---- entrypoint --------------------------------------------------------- */

static int run(const source_ctx *ctx, intel_sink *sink) {
  /* JS pollLive(): Promise.all([tryOpenSkyAPI(), tryAdsbLol()]). Both helpers
   * swallow their own errors and return null on failure; if BOTH fail the
   * poller marks all_sources_failed and updates nothing — here a 0-feature
   * snapshot (correctness-neutral; live rows only). */
  cJSON *opensky = try_opensky(ctx);          /* array or NULL              */
  cJSON *adsblol = try_adsblol(ctx);          /* array or NULL              */

  /* OpenSky first so adsb.lol's fresher position wins on the union. */
  cJSON *merged = merge_live_by_icao(opensky, adsblol);

  /* JS: for each merged feature with an icao24, stamp classifyMilitary into
   * properties (the fresh map; rows without icao24 are dropped — but
   * merge_live_by_icao already only retains rows with a non-empty icao24). */
  cJSON *features = cJSON_CreateArray();
  cJSON *f;
  cJSON_ArrayForEach(f, merged) {
    cJSON *props = cJSON_GetObjectItem(f, "properties");
    cJSON *icv = props ? cJSON_GetObjectItem(props, "icao24") : NULL;
    if (!icv || !cJSON_IsString(icv) || !*icv->valuestring) continue;
    cJSON *dup = cJSON_Duplicate(f, 1);
    cJSON *dp = cJSON_GetObjectItem(dup, "properties");
    if (dp) apply_military(dp);
    cJSON_AddItemToArray(features, dup);
  }

  int n = geojson_emit_features(sink, "plane-adsb", features);

  cJSON_Delete(features);
  cJSON_Delete(merged);
  if (opensky) cJSON_Delete(opensky);
  if (adsblol) cJSON_Delete(adsblol);
  fprintf(stderr, "[plane-adsb] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def plane_adsb_def = {
  .id = "plane-adsb", .collector = "transport",
  .name = "Live Aircraft (ADS-B snapshot)",
  .name_ja = "\xe8\x88\xaa\xe7\xa9\xba\xe6\xa9\x9f\xe3\x83\xa9\xe3\x82\xa4\xe3\x83\x96 (ADS-B)",
   .update_interval_sec = 60, .run = run };
REGISTER_SOURCE(plane_adsb_def)
