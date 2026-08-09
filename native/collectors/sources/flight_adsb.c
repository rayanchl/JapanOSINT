/* collectors/transport/sources/flight_adsb.c
 * FEED source — port of server/src/collectors/flightAdsb.js.
 * Fuses OpenSky live ADS-B + 4 adsb.lol bbox quadrants + per-airport
 * AeroDataBox scheduled flights (NRT/HND, RapidAPI key-gated).
 * Merge by ICAO24 (adsb.lol wins, property bags unioned), then dedupe
 * AeroDataBox into the live set by normalized callsign. Curated SEED /
 * _meta envelope intentionally dropped (live rows only). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

/* ---- small helpers ------------------------------------------------------ */

static int is_num(const cJSON *v) { return v && cJSON_IsNumber(v); }

static cJSON *dup_or_null(const cJSON *v) {
  return v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull();
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
  /* trim trailing ws */
  while (i > 0 && (up[i-1]==' '||up[i-1]=='\t'||up[i-1]=='\n'||up[i-1]=='\r')) up[--i]=0;
  for (const char **pf = CALLSIGN_PREFIXES; *pf; pf++) {
    size_t pl = strlen(*pf);
    if (strncmp(up, *pf, pl) == 0 && isdigit((unsigned char)up[pl])) return 1;
  }
  return 0;
}

/* Append {is_military, military_reason} to props (JS classifyMilitary). */
static void apply_military(cJSON *props) {
  cJSON *ic = cJSON_GetObjectItem(props, "icao24");
  cJSON *cs = cJSON_GetObjectItem(props, "callsign");
  cJSON *fn = cJSON_GetObjectItem(props, "flight_number");
  const char *icao24 = (ic && cJSON_IsString(ic)) ? ic->valuestring : NULL;
  const char *callsign = (cs && cJSON_IsString(cs)) ? cs->valuestring
                       : (fn && cJSON_IsString(fn)) ? fn->valuestring : NULL;
  int mil = 0; const char *reason = NULL;
  if (is_mil_by_icao24(icao24)) { mil = 1; reason = "icao_range"; }
  else if (is_mil_by_callsign(callsign)) { mil = 1; reason = "callsign_prefix"; }
  cJSON_AddBoolToObject(props, "is_military", mil);
  if (reason) cJSON_AddStringToObject(props, "military_reason", reason);
  else cJSON_AddItemToObject(props, "military_reason", cJSON_CreateNull());
}

/* ---- callsign normalization (port of normalizeCallsign) ----------------- */

static const struct { const char *iata, *icao; } IATA_TO_ICAO[] = {
  {"NH","ANA"},{"JL","JAL"},{"MM","APJ"},{"GK","JJP"},{"BC","SKY"},{"7G","SFJ"},
  {"HD","ADO"},{"6J","SNA"},{"KZ","NCA"},{"NQ","AJX"},{"JW","VNL"},{"IJ","SJO"},
  { NULL, NULL }
};

/* Returns malloc'd normalized string or NULL. */
static char *normalize_callsign(const char *raw) {
  if (!raw) return NULL;
  /* trim + upper + remove all whitespace */
  char buf[128];
  size_t j = 0;
  for (const char *p = raw; *p && j < sizeof buf - 1; p++) {
    if (isspace((unsigned char)*p)) continue;
    buf[j++] = (char)toupper((unsigned char)*p);
  }
  buf[j] = 0;
  if (j == 0) return NULL;
  /* match /^([A-Z0-9]{2,3})(\d+[A-Z]?)$/ */
  size_t L = j;
  /* try prefix length 3 then 2 (regex is greedy: {2,3} grabs 3 if it fits) */
  for (int plen = 3; plen >= 2; plen--) {
    if ((int)L <= plen) continue;
    int ok = 1;
    for (int k = 0; k < plen; k++) {
      char c = buf[k];
      if (!(isupper((unsigned char)c) || isdigit((unsigned char)c))) { ok = 0; break; }
    }
    if (!ok) continue;
    /* remainder must be \d+[A-Z]? */
    size_t r = (size_t)plen;
    if (!isdigit((unsigned char)buf[r])) continue;
    size_t d0 = r;
    while (r < L && isdigit((unsigned char)buf[r])) r++;
    if (r == d0) continue;
    if (r < L) {
      if (r != L - 1 || !isupper((unsigned char)buf[r])) continue;
    }
    /* full match: prefix=[0,plen), num=[plen,L) */
    char prefix[8]; memcpy(prefix, buf, (size_t)plen); prefix[plen] = 0;
    /* num = String(parseInt(numStr,10)) — strips leading zeros, drops
     * any trailing letter (parseInt stops at non-digit). */
    char numStr[64]; size_t nl = L - (size_t)plen;
    if (nl >= sizeof numStr) nl = sizeof numStr - 1;
    memcpy(numStr, buf + plen, nl); numStr[nl] = 0;
    long nv = strtol(numStr, NULL, 10);
    const char *icao = prefix;
    for (int m = 0; IATA_TO_ICAO[m].iata; m++)
      if (strcmp(IATA_TO_ICAO[m].iata, prefix) == 0) { icao = IATA_TO_ICAO[m].icao; break; }
    char *out = malloc(64);
    if (!out) return NULL;
    snprintf(out, 64, "%s%ld", icao, nv);
    return out;
  }
  /* no regex match → return the cleaned uppercase string */
  char *out = malloc(L + 1);
  if (!out) return NULL;
  memcpy(out, buf, L + 1);
  return out;
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

/* OAuth2 client-credentials token; NULL when creds absent or request fails. */
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

/* Returns cJSON array of Features, or NULL on failure (JS returns null). */
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
    /* (cap removed: every record of the fetched array is emitted —
     * docs/SOURCE_EXHAUSTIVENESS.md) */
    cJSON *s5 = cJSON_GetArrayItem(s, 5);
    cJSON *s6 = cJSON_GetArrayItem(s, 6);
    /* OpenSky sends null lon/lat for a state vector with no position fix.
     * The JS port substituted Tokyo (139.7, 35.6), which puts an INVENTED
     * pin on the map for an aircraft whose position is unknown. Drop the
     * state instead — a missing position is not a Tokyo position. */
    if (!is_num(s5) || !is_num(s6)) continue;
    double lon = s5->valuedouble;
    double lat = s6->valuedouble;

    cJSON *f = gj_point_feature(lon, lat);

    cJSON *p = cJSON_CreateObject();                  /* EXACT JS key order */
    char idbuf[32];
    snprintf(idbuf, sizeof idbuf, "ADSB_LIVE_%d", i);
    cJSON_AddStringToObject(p, "id", idbuf);
    cJSON_AddItemToObject(p, "icao24", dup_or_null(cJSON_GetArrayItem(s, 0)));  /* exhaustive-ok: fixed state-vector tuple */
    cJSON *s1 = cJSON_GetArrayItem(s, 1);
    char *cs = trim_or_null((s1 && cJSON_IsString(s1)) ? s1->valuestring : "");
    /* JS: (s[1]||'').trim() — empty string, not null */
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

static cJSON *round_or_null(const cJSON *v) {
  return is_num(v) ? cJSON_CreateNumber(round(v->valuedouble)) : cJSON_CreateNull();
}

/* Returns cJSON array of Features, or NULL on total blackout (JS null). */
static cJSON *try_adsblol(const source_ctx *ctx) {
  /* Collect raw aircraft from all quadrants; dedupe by lowercased hex. */
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

    cJSON *f = gj_point_feature(lonv->valuedouble, latv->valuedouble);

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

/* ---- AeroDataBox (scheduled flights, key-gated) ------------------------- */

typedef struct {
  const char *icao, *iata, *name; double lat, lon;
} adb_airport;
static const adb_airport AERODATABOX_AIRPORTS[] = {
  { "RJAA", "NRT", "Narita", 35.7720, 140.3929 },
  { "RJTT", "HND", "Haneda", 35.5494, 139.7798 },
};

static const char *str_or_null(cJSON *o, const char *k) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  return (v && cJSON_IsString(v) && *v->valuestring) ? v->valuestring : NULL;
}

/* Appends Features to `out` for one airport. Key-gated: 0 rows when unset. */
static void try_aerodatabox_airport(const source_ctx *ctx,
                                    const adb_airport *ap, cJSON *out) {
  const char *key = getenv("AERODATABOX_KEY");
  if (!key || !*key) return;                          /* RULE 9: 0 rows     */

  time_t now = time(NULL);
  struct tm tmv;
  gmtime_r(&now, &tmv);
  char start[32], end[32];
  strftime(start, sizeof start, "%Y-%m-%dT%H:%M", &tmv);
  time_t later = now + 11 * 3600;
  gmtime_r(&later, &tmv);
  strftime(end, sizeof end, "%Y-%m-%dT%H:%M", &tmv);

  char url[512];
  snprintf(url, sizeof url,
    "https://aerodatabox.p.rapidapi.com/flights/airports/icao/%s/%s/%s"
    "?withLeg=true&direction=Both&withCancelled=true&withCargo=false",
    ap->icao, start, end);
  char keyh[256];
  snprintf(keyh, sizeof keyh, "X-RapidAPI-Key: %s", key);
  const char *hdrs[] = { keyh,
    "X-RapidAPI-Host: aerodatabox.p.rapidapi.com", NULL };
  cJSON *data = feed_get_json_h(ctx->http, url, hdrs, 10000);
  if (!data) return;

  cJSON *arrivals = cJSON_GetObjectItem(data, "arrivals");
  cJSON *departures = cJSON_GetObjectItem(data, "departures");
  int i = 0;
  /* arrivals then departures, combined slice(0,150) */
  for (int pass = 0; pass < 2; pass++) {
    cJSON *list = pass == 0 ? arrivals : departures;
    int isArrival = pass == 0;
    if (!cJSON_IsArray(list)) continue;
    cJSON *fl;
    cJSON_ArrayForEach(fl, list) {
      /* (cap removed: every record of the fetched array is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
      char airportLabel[160];
      snprintf(airportLabel, sizeof airportLabel, "%s (%s/%s)",
               ap->name, ap->icao, ap->iata);
      cJSON *mv = cJSON_GetObjectItem(fl, "movement");
      const char *mvAirportIata = NULL;
      const char *schedUtc = NULL, *revUtc = NULL, *terminal = NULL;
      if (mv) {
        cJSON *mvAp = cJSON_GetObjectItem(mv, "airport");
        if (mvAp) mvAirportIata = str_or_null(mvAp, "iata");
        cJSON *st = cJSON_GetObjectItem(mv, "scheduledTime");
        if (st) schedUtc = str_or_null(st, "utc");
        cJSON *rt = cJSON_GetObjectItem(mv, "revisedTime");
        if (rt) revUtc = str_or_null(rt, "utc");
        terminal = str_or_null(mv, "terminal");
      }
      const char *number = str_or_null(fl, "number");
      cJSON *airlineO = cJSON_GetObjectItem(fl, "airline");
      const char *airlineName = airlineO ? str_or_null(airlineO, "name") : NULL;
      cJSON *aircraftO = cJSON_GetObjectItem(fl, "aircraft");
      const char *aircraftModel = aircraftO ? str_or_null(aircraftO, "model") : NULL;
      const char *status = str_or_null(fl, "status");

      cJSON *f = gj_point_feature(ap->lon, ap->lat);

      cJSON *p = cJSON_CreateObject();                /* EXACT JS key order */
      char idbuf[32];
      snprintf(idbuf, sizeof idbuf, "%s_ADB_%05d", ap->iata, i + 1);
      cJSON_AddStringToObject(p, "id", idbuf);
      cJSON_AddItemToObject(p, "callsign",
        number ? cJSON_CreateString(number) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "flight_number",
        number ? cJSON_CreateString(number) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "airline",
        airlineName ? cJSON_CreateString(airlineName) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "aircraft_type",
        aircraftModel ? cJSON_CreateString(aircraftModel) : cJSON_CreateNull());
      if (isArrival)
        cJSON_AddItemToObject(p, "origin",
          mvAirportIata ? cJSON_CreateString(mvAirportIata) : cJSON_CreateNull());
      else
        cJSON_AddStringToObject(p, "origin", ap->iata);
      if (isArrival)
        cJSON_AddStringToObject(p, "destination", ap->iata);
      else
        cJSON_AddItemToObject(p, "destination",
          mvAirportIata ? cJSON_CreateString(mvAirportIata) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "status",
        status ? cJSON_CreateString(status) : cJSON_CreateNull());
      cJSON_AddStringToObject(p, "type", isArrival ? "arrival" : "departure");
      cJSON_AddStringToObject(p, "airport", airportLabel);
      cJSON_AddItemToObject(p, "scheduled_time",
        schedUtc ? cJSON_CreateString(schedUtc) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "revised_time",
        revUtc ? cJSON_CreateString(revUtc) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "terminal",
        terminal ? cJSON_CreateString(terminal) : cJSON_CreateNull());
      cJSON_AddStringToObject(p, "source", "aerodatabox_api");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(out, f);
      i++;
    }
    /* (cap removed: every record the upstream returned is emitted —
     * docs/SOURCE_EXHAUSTIVENESS.md) */
  }
  cJSON_Delete(data);
}

/* ---- merge by ICAO24 (port of mergeLiveByIcao) -------------------------- */

/* Order-preserving merge: opensky first, then adsblol. Later sources
 * overwrite geometry/id/source; property bags unioned (prev keys keep
 * insertion slot, this-source values overwrite, new keys appended). */
static cJSON *merge_live_by_icao(cJSON *opensky, cJSON *adsblol) {
  cJSON *order = cJSON_CreateArray();        /* icao order list            */
  cJSON *map = cJSON_CreateObject();         /* icao -> merged Feature     */
  cJSON *lists[2] = { opensky, adsblol };
  for (int li = 0; li < 2; li++) {
    cJSON *list = lists[li];
    if (!list || !cJSON_IsArray(list)) continue;
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

/* ---- dedupe AeroDataBox into live by normalized callsign ---------------- */

/* Mutates `live` in place; returns a new array (live + non-dup aero). */
static cJSON *dedupe_features(cJSON *live, cJSON *aero) {
  cJSON *result = cJSON_CreateArray();
  cJSON *keyOrder = cJSON_CreateArray();    /* normalized-key list          */
  cJSON *byKey = cJSON_CreateObject();      /* key -> ptr index into result */
  int ri = 0;

  cJSON *f;
  cJSON_ArrayForEach(f, live) {
    cJSON *props = cJSON_GetObjectItem(f, "properties");
    cJSON *csv = props ? cJSON_GetObjectItem(props, "callsign") : NULL;
    char *key = normalize_callsign(
      (csv && cJSON_IsString(csv)) ? csv->valuestring : NULL);
    cJSON *dup = cJSON_Duplicate(f, 1);
    cJSON_AddItemToArray(result, dup);
    if (key) {
      cJSON_DeleteItemFromObject(byKey, key);
      cJSON_AddNumberToObject(byKey, key, ri);
      free(key);
    }
    ri++;
  }

  cJSON *af;
  cJSON_ArrayForEach(af, aero) {
    cJSON *aprops = cJSON_GetObjectItem(af, "properties");
    cJSON *fnv = aprops ? cJSON_GetObjectItem(aprops, "flight_number") : NULL;
    cJSON *acsv = aprops ? cJSON_GetObjectItem(aprops, "callsign") : NULL;
    const char *src = (fnv && cJSON_IsString(fnv) && *fnv->valuestring)
                        ? fnv->valuestring
                        : (acsv && cJSON_IsString(acsv)) ? acsv->valuestring : NULL;
    char *key = normalize_callsign(src);
    if (key) {
      cJSON *idxv = cJSON_GetObjectItem(byKey, key);
      if (idxv && cJSON_IsNumber(idxv)) {
        /* merge aero into the matched live feature's properties */
        cJSON *liveF = cJSON_GetArrayItem(result, idxv->valueint);
        cJSON *liveP = cJSON_GetObjectItem(liveF, "properties");
        /* { ...aero.props, ...live.props, <overrides> } */
        cJSON *merged = cJSON_CreateObject();
        cJSON *ak;
        cJSON_ArrayForEach(ak, aprops)
          cJSON_AddItemToObject(merged, ak->string, cJSON_Duplicate(ak, 1));
        cJSON *lk;
        cJSON_ArrayForEach(lk, liveP) {
          cJSON_DeleteItemFromObject(merged, lk->string);
          cJSON_AddItemToObject(merged, lk->string, cJSON_Duplicate(lk, 1));
        }
        /* explicit overrides (JS spread tail) */
        cJSON *liveAirline = cJSON_GetObjectItem(liveP, "airline");
        cJSON *aeroAirline = cJSON_GetObjectItem(aprops, "airline");
        int liveAirlineTruthy = liveAirline && cJSON_IsString(liveAirline)
                                && *liveAirline->valuestring;
        cJSON_DeleteItemFromObject(merged, "airline");
        cJSON_AddItemToObject(merged, "airline",
          cJSON_Duplicate(liveAirlineTruthy ? liveAirline
            : (aeroAirline ? aeroAirline : cJSON_CreateNull()), 1));
        cJSON_DeleteItemFromObject(merged, "scheduled_time");
        cJSON_AddItemToObject(merged, "scheduled_time",
          dup_or_null(cJSON_GetObjectItem(aprops, "scheduled_time")));
        cJSON_DeleteItemFromObject(merged, "revised_time");
        cJSON_AddItemToObject(merged, "revised_time",
          dup_or_null(cJSON_GetObjectItem(aprops, "revised_time")));
        cJSON_DeleteItemFromObject(merged, "terminal");
        cJSON_AddItemToObject(merged, "terminal",
          dup_or_null(cJSON_GetObjectItem(aprops, "terminal")));
        cJSON *liveStatus = cJSON_GetObjectItem(liveP, "status");
        cJSON *aeroStatus = cJSON_GetObjectItem(aprops, "status");
        int liveStatusTruthy = liveStatus && cJSON_IsString(liveStatus)
                               && *liveStatus->valuestring;
        cJSON_DeleteItemFromObject(merged, "status");
        cJSON_AddItemToObject(merged, "status",
          cJSON_Duplicate(liveStatusTruthy ? liveStatus
            : (aeroStatus ? aeroStatus : cJSON_CreateNull()), 1));
        cJSON *liveType = cJSON_GetObjectItem(liveP, "type");
        cJSON *aeroType = cJSON_GetObjectItem(aprops, "type");
        int liveTypeTruthy = liveType && cJSON_IsString(liveType)
                             && *liveType->valuestring;
        cJSON_DeleteItemFromObject(merged, "type");
        cJSON_AddItemToObject(merged, "type",
          cJSON_Duplicate(liveTypeTruthy ? liveType
            : (aeroType ? aeroType : cJSON_CreateNull()), 1));
        cJSON_DeleteItemFromObject(merged, "source");
        cJSON_AddStringToObject(merged, "source", "opensky+aerodatabox");
        cJSON_ReplaceItemInObject(liveF, "properties", merged);
        free(key);
        continue;                            /* JS: continue (no push)     */
      }
      cJSON_DeleteItemFromObject(byKey, key);
      cJSON_AddNumberToObject(byKey, key, ri);
      free(key);
    }
    cJSON_AddItemToArray(result, cJSON_Duplicate(af, 1));
    ri++;
  }
  cJSON_Delete(keyOrder);
  cJSON_Delete(byKey);
  return result;
}

/* ---- display title ------------------------------------------------------ */

/* lib/geojson.c takes intel_item.title from props title/name/name_ja/label.
 * The JS port set none of them, so every flight row reached the UI titleless.
 * Build one from what the feed actually carries — callsign, registration and
 * type — and fall back to the ICAO24 hex, which is always present. */
static void apply_title(cJSON *props) {
  if (cJSON_GetObjectItem(props, "title")) return;
  const char *cs = str_or_null(props, "callsign");
  if (!cs) cs = str_or_null(props, "flight_number");
  const char *reg = str_or_null(props, "registration");
  const char *ty = str_or_null(props, "aircraft_type");
  const char *hex = str_or_null(props, "icao24");
  char t[160];
  if (cs && reg && ty)      snprintf(t, sizeof t, "%s (%s, %s)", cs, reg, ty);
  else if (cs && reg)       snprintf(t, sizeof t, "%s (%s)", cs, reg);
  else if (cs && ty)        snprintf(t, sizeof t, "%s (%s)", cs, ty);
  else if (cs)              snprintf(t, sizeof t, "%s", cs);
  else if (reg && ty)       snprintf(t, sizeof t, "%s (%s)", reg, ty);
  else if (reg)             snprintf(t, sizeof t, "%s", reg);
  else if (hex)             snprintf(t, sizeof t, "ICAO24 %s", hex);
  else                      return;
  cJSON_AddStringToObject(props, "title", t);
}

/* ---- entrypoint --------------------------------------------------------- */

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *opensky = try_opensky(ctx);          /* array or NULL              */
  cJSON *adsblol = try_adsblol(ctx);          /* array or NULL              */
  cJSON *aero = cJSON_CreateArray();
  for (size_t a = 0; a < sizeof AERODATABOX_AIRPORTS / sizeof AERODATABOX_AIRPORTS[0]; a++)
    try_aerodatabox_airport(ctx, &AERODATABOX_AIRPORTS[a], aero);

  cJSON *live = merge_live_by_icao(opensky, adsblol);
  cJSON *features = dedupe_features(live, aero);

  /* classifyMilitary on every feature */
  cJSON *f;
  cJSON_ArrayForEach(f, features) {
    cJSON *props = cJSON_GetObjectItem(f, "properties");
    if (props) { apply_military(props); apply_title(props); }
  }

  int n = geojson_emit_features(sink, "flight-adsb", features);

  cJSON_Delete(features);
  cJSON_Delete(live);
  cJSON_Delete(aero);
  if (opensky) cJSON_Delete(opensky);
  if (adsblol) cJSON_Delete(adsblol);
  fprintf(stderr, "[flight-adsb] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def flight_adsb_def = {
  .id = "flight-adsb", .collector = "transport",
  .name = "ADS-B Flight Tracking", .name_ja = "ADS-B \xe3\x83\x95\xe3\x83\xa9\xe3\x82\xa4\xe3\x83\x88\xe8\xbf\xbd\xe8\xb7\xa1",
   .update_interval_sec = 60, .run = run };
REGISTER_SOURCE(flight_adsb_def)
