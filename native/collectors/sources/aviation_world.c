/* collectors/sources/aviation_world.c
 * OSINT services — aviation, worldwide, KEYLESS + LIVE. Three sources, one run()
 * switching on ctx->source_id (all pivot on ctx->entity):
 *
 *   OURAIRPORTS     — davidmegginson.github.io/ourairports-data/airports.csv.
 *                     Streams the global airport dataset and emits one intel_item
 *                     per airport whose ICAO/IATA/GPS/local code, ident, or name
 *                     matches the entity. Real rows only (name + lat/lon from CSV).
 *
 *   HEXDB_AIRCRAFT  — hexdb.io/api/v1/aircraft/<24-bit-hex>. Reverse Mode-S/ICAO
 *                     hex → airframe (registration, manufacturer, type, owner).
 *                     hexdb v1 keys strictly on the 6-hex ModeS address; when the
 *                     entity isn't a bare hex we first resolve it (registration or
 *                     callsign) to a live hex via adsb.fi, then look that up.
 *
 *   ADSB_FI         — opendata.adsb.fi/api/v2 (keyless). Live aircraft state by
 *                     hex / registration / callsign; adsb.lol is queried as a
 *                     supplementary mirror. One item per live aircraft in "ac".
 *
 * HONESTY: every branch REAL-fetches and emits only parsed fields, or returns 0
 * (honest empty). Nothing is fabricated; no constructed URL is emitted as a
 * record. Missing/idle aircraft simply yield nothing. */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- small utils ------------------------------------------------------- */

/* case-insensitive equality */
static int ci_eq(const char *a, const char *b) {
  if (!a || !b) return 0;
  while (*a && *b) {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    a++; b++;
  }
  return *a == *b;
}

/* is s exactly 6 hex digits (a 24-bit ICAO/Mode-S address)? */
static int is_hex6(const char *s) {
  int n = 0;
  for (const char *p = s; *p; p++, n++)
    if (!isxdigit((unsigned char)*p)) return 0;
  return n == 6;
}

static const char *cj_num_or_null(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (v && cJSON_IsNumber(v)) { *out = v->valuedouble; return k; }
  return NULL;
}

/* ---- OURAIRPORTS ------------------------------------------------------- */

/* Parse ONE CSV line (RFC4180-ish: double-quoted fields, "" escapes) into up to
 * `maxf` field pointers within a mutable copy. Returns field count. */
static int csv_split(char *line, char **f, int maxf) {
  int n = 0; char *w = line; char *r = line;
  int inq = 0;
  f[n] = w;
  while (*r) {
    char c = *r;
    if (inq) {
      if (c == '"') {
        if (r[1] == '"') { *w++ = '"'; r += 2; continue; }
        inq = 0; r++; continue;
      }
      *w++ = c; r++;
    } else {
      if (c == '"') { inq = 1; r++; continue; }
      if (c == ',') {
        *w = 0; r++; w = r;
        if (++n >= maxf) return n;
        f[n] = w;
        /* w may lag r after unescaping; realign start */
        continue;
      }
      if (c == '\r' || c == '\n') { break; }
      *w++ = c; r++;
    }
  }
  *w = 0;
  return n + 1;
}

/* field indices in the ourairports CSV header */
enum { OA_ID, OA_IDENT, OA_TYPE, OA_NAME, OA_LAT, OA_LON, OA_ELEV, OA_CONT,
       OA_COUNTRY, OA_REGION, OA_MUNI, OA_SCHED, OA_ICAO, OA_IATA, OA_GPS,
       OA_LOCAL, OA_HOME, OA_WIKI, OA_KEYWORDS, OA_MAX };

static int oa_match(char **f, int nf, const char *q) {
  /* code-style exact match (case-insensitive) on identifiers */
  if (nf > OA_IDENT && ci_eq(f[OA_IDENT], q)) return 1;
  if (nf > OA_ICAO  && f[OA_ICAO][0]  && ci_eq(f[OA_ICAO], q))  return 1;
  if (nf > OA_IATA  && f[OA_IATA][0]  && ci_eq(f[OA_IATA], q))  return 1;
  if (nf > OA_GPS   && f[OA_GPS][0]   && ci_eq(f[OA_GPS], q))   return 1;
  if (nf > OA_LOCAL && f[OA_LOCAL][0] && ci_eq(f[OA_LOCAL], q)) return 1;
  /* substring match on the name (only for queries long enough to be meaningful) */
  if (strlen(q) >= 4 && nf > OA_NAME && f[OA_NAME][0]) {
    /* case-insensitive substr */
    const char *hay = f[OA_NAME];
    size_t ql = strlen(q);
    for (const char *h = hay; *h; h++) {
      size_t i = 0;
      while (i < ql && h[i] &&
             tolower((unsigned char)h[i]) == tolower((unsigned char)q[i])) i++;
      if (i == ql) return 1;
    }
  }
  return 0;
}

static int oa_emit(intel_sink *sink, char **f, int nf) {
  const char *ident = (nf > OA_IDENT) ? f[OA_IDENT] : NULL;
  const char *name  = (nf > OA_NAME)  ? f[OA_NAME]  : NULL;
  if (!name || !name[0]) return 0;
  const char *typ   = (nf > OA_TYPE)  ? f[OA_TYPE]  : NULL;
  const char *muni  = (nf > OA_MUNI)  ? f[OA_MUNI]  : NULL;
  const char *ctry  = (nf > OA_COUNTRY)? f[OA_COUNTRY] : NULL;
  const char *icao  = (nf > OA_ICAO && f[OA_ICAO][0]) ? f[OA_ICAO] : NULL;
  const char *iata  = (nf > OA_IATA && f[OA_IATA][0]) ? f[OA_IATA] : NULL;
  const char *wiki  = (nf > OA_WIKI && f[OA_WIKI][0]) ? f[OA_WIKI] : NULL;
  double lat = 0, lon = 0; int hasgeo = 0;
  if (nf > OA_LAT && f[OA_LAT][0] && nf > OA_LON && f[OA_LON][0]) {
    lat = atof(f[OA_LAT]); lon = atof(f[OA_LON]);
    if (lat != 0.0 || lon != 0.0) hasgeo = 1;
  }

  cJSON *data = cJSON_CreateObject();
  cJSON_AddStringToObject(data, "name", name);
  if (ident) cJSON_AddStringToObject(data, "ident", ident);
  if (icao)  cJSON_AddStringToObject(data, "icao_code", icao);
  if (iata)  cJSON_AddStringToObject(data, "iata_code", iata);
  if (typ)   cJSON_AddStringToObject(data, "type", typ);
  if (muni)  cJSON_AddStringToObject(data, "municipality", muni);
  if (ctry)  cJSON_AddStringToObject(data, "iso_country", ctry);
  if (hasgeo) { cJSON_AddNumberToObject(data, "lat", lat);
                cJSON_AddNumberToObject(data, "lon", lon); }
  cJSON_AddStringToObject(data, "source", "OurAirports");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "OURAIRPORTS");
  cJSON_AddStringToObject(props, "source", "OurAirports");
  if (icao) cJSON_AddStringToObject(props, "icao_code", icao);
  if (iata) cJSON_AddStringToObject(props, "iata_code", iata);
  if (ctry) cJSON_AddStringToObject(props, "iso_country", ctry);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char summary[256];
  snprintf(summary, sizeof summary, "%s%s%s%s%s",
           typ ? typ : "airport",
           muni ? " · " : "", muni ? muni : "",
           ctry ? ", " : "", ctry ? ctry : "");

  intel_item it = {0};
  it.remote_key      = (icao && icao[0]) ? icao : (ident ? ident : name);
  it.title           = name;
  it.summary         = summary;
  it.body            = bj;
  it.lang            = "en";
  it.link            = wiki;
  it.has_geo         = hasgeo; it.lat = lat; it.lon = lon;
  it.record_type     = "airport";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"OURAIRPORTS\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_ourairports(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *csv = jo_get(ctx,
      "https://davidmegginson.github.io/ourairports-data/airports.csv",
      NULL, "ourairports");
  if (!csv) return 0;

  int emitted = 0, max = 40;
  char *save = NULL;
  char *line = strtok_r(csv, "\n", &save);
  int first = 1;
  while (line && emitted < max) {
    if (first) { first = 0; line = strtok_r(NULL, "\n", &save); continue; }
    /* work on a copy so csv_split's in-place edits don't corrupt strtok state */
    char buf[2048];
    snprintf(buf, sizeof buf, "%s", line);
    char *f[OA_MAX];
    int nf = csv_split(buf, f, OA_MAX);
    if (nf >= OA_NAME && oa_match(f, nf, q)) emitted += oa_emit(sink, f, nf);
    line = strtok_r(NULL, "\n", &save);
  }
  free(csv);
  fprintf(stderr, "[ourairports] emitted %d\n", emitted);
  return emitted;
}

/* ---- HEXDB_AIRCRAFT ---------------------------------------------------- */

static int hexdb_emit(intel_sink *sink, const cJSON *r, const char *hex) {
  const char *modes = jo_sv(r, "ModeS");
  const char *reg   = jo_sv(r, "Registration");
  const char *manu  = jo_sv(r, "Manufacturer");
  const char *ictc  = jo_sv(r, "ICAOTypeCode");
  const char *type  = jo_sv(r, "Type");
  const char *owner = jo_sv(r, "RegisteredOwners");
  const char *ofc   = jo_sv(r, "OperatorFlagCode");
  if (!reg && !type && !manu) return 0;

  cJSON *data = cJSON_CreateObject();
  if (modes) cJSON_AddStringToObject(data, "mode_s", modes);
  else if (hex) cJSON_AddStringToObject(data, "mode_s", hex);
  if (reg)   cJSON_AddStringToObject(data, "registration", reg);
  if (manu)  cJSON_AddStringToObject(data, "manufacturer", manu);
  if (type)  cJSON_AddStringToObject(data, "type", type);
  if (ictc)  cJSON_AddStringToObject(data, "icao_type_code", ictc);
  if (owner) cJSON_AddStringToObject(data, "registered_owner", owner);
  if (ofc)   cJSON_AddStringToObject(data, "operator_flag_code", ofc);
  cJSON_AddStringToObject(data, "source", "hexdb.io");
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "HEXDB_AIRCRAFT");
  cJSON_AddStringToObject(props, "source", "hexdb.io");
  if (modes) cJSON_AddStringToObject(props, "mode_s", modes);
  if (reg)   cJSON_AddStringToObject(props, "registration", reg);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char title[192];
  snprintf(title, sizeof title, "%s%s%s",
           reg ? reg : (modes ? modes : "aircraft"),
           type ? " — " : "", type ? type : "");
  char link[128];
  snprintf(link, sizeof link, "https://hexdb.io/registration?reg=%s",
           reg ? reg : (modes ? modes : ""));

  intel_item it = {0};
  it.remote_key      = modes ? modes : (reg ? reg : hex);
  it.title           = title;
  it.summary         = owner ? owner : manu;
  it.body            = bj;
  it.lang            = "en";
  it.link            = (reg || modes) ? link : NULL;
  it.record_type     = "aircraft";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"HEXDB_AIRCRAFT\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* Query adsb.fi to resolve a registration/callsign to a currently-live hex.
 * Writes lowercase 6-hex into hexout (>=7 bytes). Returns 1 on success. */
static int adsbfi_resolve_hex(const source_ctx *ctx, const char *kind,
                              const char *enc, char *hexout) {
  char url[256];
  snprintf(url, sizeof url, "https://opendata.adsb.fi/api/v2/%s/%s", kind, enc);
  char *body = jo_get(ctx, url, NULL, "hexdb");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  int ok = 0;
  cJSON *ac = cJSON_GetObjectItem(root, "ac");
  if (cJSON_IsArray(ac) && cJSON_GetArraySize(ac) > 0) {
    const char *h = jo_sv(cJSON_GetArrayItem(ac, 0), "hex");  /* exhaustive-ok: registration->hex resolution step */
    if (h && is_hex6(h)) { snprintf(hexout, 7, "%s", h); ok = 1; }
  }
  cJSON_Delete(root);
  return ok;
}

static int run_hexdb(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char hex[8] = {0};
  if (is_hex6(q)) {
    snprintf(hex, sizeof hex, "%s", q);
  } else {
    /* entity is a registration or callsign — resolve to a live hex first */
    char *enc = jo_urlencode(q);
    if (!enc) return 0;
    int got = adsbfi_resolve_hex(ctx, "registration", enc, hex)
           || adsbfi_resolve_hex(ctx, "callsign", enc, hex);
    free(enc);
    if (!got) { fprintf(stderr, "[hexdb] no live hex for '%s'\n", q); return 0; }
  }

  char url[128];
  snprintf(url, sizeof url, "https://hexdb.io/api/v1/aircraft/%s", hex);
  const char *hdrs[] = { "Accept: application/json", NULL };
  char *body = jo_get(ctx, url, hdrs, "hexdb");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  int emitted = 0;
  /* hexdb returns {"status":"404",...} for unknown hex; only emit real records */
  if (!cJSON_GetObjectItem(root, "status"))
    emitted = hexdb_emit(sink, root, hex);
  cJSON_Delete(root);
  fprintf(stderr, "[hexdb] emitted %d\n", emitted);
  return emitted;
}

/* ---- ADSB_FI (+ adsb.lol mirror) -------------------------------------- */

static int adsbfi_emit(intel_sink *sink, const cJSON *a, const char *src) {
  const char *hex   = jo_sv(a, "hex");
  const char *flt   = jo_sv(a, "flight");
  const char *reg   = jo_sv(a, "r");
  const char *typ   = jo_sv(a, "t");
  if (!hex && !flt && !reg) return 0;

  double lat = 0, lon = 0; int hasgeo = 0;
  if (cj_num_or_null(a, "lat", &lat) && cj_num_or_null(a, "lon", &lon))
    hasgeo = 1;
  double v; char alt[32] = {0};
  const cJSON *ab = cJSON_GetObjectItem(a, "alt_baro");
  if (ab && cJSON_IsNumber(ab)) snprintf(alt, sizeof alt, "%d", (int)ab->valuedouble);
  else if (ab && cJSON_IsString(ab)) snprintf(alt, sizeof alt, "%s", ab->valuestring);

  cJSON *data = cJSON_CreateObject();
  if (hex) cJSON_AddStringToObject(data, "hex", hex);
  if (flt) { char *t = strdup(flt); if (t){ for(char*p=t;*p;p++) if(*p==' ')*p=0;
             cJSON_AddStringToObject(data, "callsign", t); free(t);} }
  if (reg) cJSON_AddStringToObject(data, "registration", reg);
  if (typ) cJSON_AddStringToObject(data, "type", typ);
  if (hasgeo) { cJSON_AddNumberToObject(data, "lat", lat);
                cJSON_AddNumberToObject(data, "lon", lon); }
  if (alt[0]) cJSON_AddStringToObject(data, "alt_baro", alt);
  if (cj_num_or_null(a, "gs", &v))   cJSON_AddNumberToObject(data, "ground_speed", v);
  if (cj_num_or_null(a, "track", &v))cJSON_AddNumberToObject(data, "track", v);
  cJSON_AddStringToObject(data, "source", src);
  char *bj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);

  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", "ADSB_FI");
  cJSON_AddStringToObject(props, "source", src);
  if (hex) cJSON_AddStringToObject(props, "hex", hex);
  if (reg) cJSON_AddStringToObject(props, "registration", reg);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  char title[128];
  snprintf(title, sizeof title, "%s%s%s",
           flt && flt[0] ? flt : (reg ? reg : (hex ? hex : "aircraft")),
           typ ? " · " : "", typ ? typ : "");
  /* strip trailing spaces the ADS-B flight field pads with */
  for (int i = (int)strlen(title) - 1; i >= 0 && title[i] == ' '; i--) title[i] = 0;

  intel_item it = {0};
  it.remote_key      = hex ? hex : (reg ? reg : flt);
  it.title           = title;
  it.summary         = reg;
  it.body            = bj;
  it.lang            = "en";
  it.has_geo         = hasgeo; it.lat = lat; it.lon = lon;
  it.record_type     = "aircraft-live";
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"ADSB_FI\"]";
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

/* fetch one {ac:[...]} endpoint, emit each aircraft. Returns count. */
static int adsbfi_fetch(const source_ctx *ctx, intel_sink *sink,
                        const char *url, const char *src) {
  char *body = jo_get(ctx, url, NULL, "adsb_fi");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  int emitted = 0;
  cJSON *ac = cJSON_GetObjectItem(root, "ac");
  if (cJSON_IsArray(ac)) {
    cJSON *a; cJSON_ArrayForEach(a, ac) emitted += adsbfi_emit(sink, a, src);
  }
  cJSON_Delete(root);
  return emitted;
}

static int run_adsbfi(const source_ctx *ctx, intel_sink *sink, const char *q) {
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  const char *kind = is_hex6(q) ? "hex"
                   : (strchr(q, ' ') ? "callsign"
                   : (strpbrk(q, "-") || strlen(q) <= 8 ? "registration" : "callsign"));
  char url[256];
  int emitted = 0;

  /* adsb.fi (primary) */
  snprintf(url, sizeof url, "https://opendata.adsb.fi/api/v2/%s/%s", kind, enc);
  emitted += adsbfi_fetch(ctx, sink, url, "adsb.fi");

  /* adsb.lol (supplementary mirror; same v2 schema). Only try registration/
   * callsign/hex it supports; hex uses /icao on adsb.lol → use /hex compat. */
  const char *lol_kind = is_hex6(q) ? "hex" : kind;
  snprintf(url, sizeof url, "https://api.adsb.lol/v2/%s/%s", lol_kind, enc);
  emitted += adsbfi_fetch(ctx, sink, url, "adsb.lol");

  free(enc);
  fprintf(stderr, "[adsb_fi] emitted %d\n", emitted);
  return emitted;
}

/* ---- dispatch ---------------------------------------------------------- */

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return -1;
  const char *sid = ctx->source_id ? ctx->source_id : "";

  if (strcmp(sid, "OURAIRPORTS") == 0)    { run_ourairports(ctx, sink, q); return 0; }
  if (strcmp(sid, "HEXDB_AIRCRAFT") == 0) { run_hexdb(ctx, sink, q);       return 0; }
  if (strcmp(sid, "ADSB_FI") == 0)        { run_adsbfi(ctx, sink, q);      return 0; }
  return 0;
}

static const source_def ourairports_def = {
  .id = "OURAIRPORTS", .collector = "osint",
  .name = "OurAirports Directory", .name_ja = "OurAirports 空港検索",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "dataset",
  .url = "https://davidmegginson.github.io/ourairports-data/airports.csv",
  .description = "OurAirports global airport dataset (keyless) — match by ICAO/IATA code or name; lat/lon.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(ourairports_def)

static const source_def hexdb_aircraft_def = {
  .id = "HEXDB_AIRCRAFT", .collector = "osint",
  .name = "hexdb Aircraft Lookup", .name_ja = "hexdb 機体検索",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "api",
  .url = "https://hexdb.io/api/v1/aircraft",
  .description = "hexdb.io Mode-S/ICAO hex → airframe (registration, type, owner); reg/callsign resolved live.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(hexdb_aircraft_def)

static const source_def adsb_fi_def = {
  .id = "ADSB_FI", .collector = "osint",
  .name = "adsb.fi Live Aircraft", .name_ja = "adsb.fi ライブ航空機",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "api",
  .url = "https://opendata.adsb.fi/api/v2",
  .description = "adsb.fi keyless live aircraft by hex/registration/callsign (+ adsb.lol mirror).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(adsb_fi_def)
