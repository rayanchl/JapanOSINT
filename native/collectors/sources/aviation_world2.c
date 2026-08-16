/* collectors/sources/aviation_world2.c
 * OSINT services — net-new keyless ADS-B / aircraft intelligence, pivoting on
 * ctx->entity (an ICAO24 hex, registration, or callsign). Shared run() switches
 * on ctx->source_id. Real fetch or honest-empty. No API keys.
 *
 *   ADSB_LOL         api.adsb.lol/v2/{hex|reg|callsign}/<e>   (JSON, live ADS-B)
 *   AIRPLANES_LIVE   api.airplanes.live/v2/{hex|reg|callsign}/<e> (JSON)
 *   OPENSKY_STATES   opensky-network.org/api/states/all?icao24=<hex> (JSON)
 *   PLANESPOTTERS    api.planespotters.net/pub/photos/{hex|reg}/<e> (JSON) */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static const char *AV_UA = "User-Agent: JapanOSINT/1.0 (osint@example.org)";

/* Heuristic: 6 hex chars → ICAO24 hex; contains digit+letter with '-' or len<=8 → reg;
 * else treat as callsign. Returns "hex" | "reg" | "callsign". */
static const char *av_kind(const char *e) {
  size_t n = strlen(e);
  if (n == 6) {
    int allhex = 1;
    for (size_t i = 0; i < n; i++) if (!isxdigit((unsigned char)e[i])) { allhex = 0; break; }
    if (allhex) return "hex";
  }
  if (strchr(e, '-')) return "reg";
  return "callsign";
}

/* ADS-B `flight` is a fixed-width 8-char field, space-padded ("APJ877  "), so
 * copying it straight into title/callsign left trailing blanks on every row.
 * Copy the trimmed value into `buf`; returns buf, or NULL if empty. */
static const char *av_trim(const char *s, char *buf, size_t n) {
  if (!s) return NULL;
  while (*s == ' ') s++;
  size_t L = strlen(s);
  while (L && s[L - 1] == ' ') L--;
  if (!L) return NULL;
  if (L >= n) L = n - 1;
  memcpy(buf, s, L);
  buf[L] = 0;
  return buf;
}

/* Emit one aircraft record from a re-api (adsb.lol / airplanes.live) "ac" obj. */
static int av_emit_ac(intel_sink *sink, const char *service, const cJSON *ac) {
  const char *hex = jo_sv(ac, "hex");
  char flbuf[32];
  const char *flight = av_trim(jo_sv(ac, "flight"), flbuf, sizeof flbuf);
  const char *reg = jo_sv(ac, "r");
  const char *type = jo_sv(ac, "t");
  const cJSON *lat = cJSON_GetObjectItem(ac, "lat");
  const cJSON *lon = cJSON_GetObjectItem(ac, "lon");
  const cJSON *alt = cJSON_GetObjectItem(ac, "alt_baro");
  if (!hex && !flight && !reg) return 0;
  cJSON *d = cJSON_CreateObject();
  if (hex) cJSON_AddStringToObject(d, "hex", hex);
  if (flight) cJSON_AddStringToObject(d, "callsign", flight);
  if (reg) cJSON_AddStringToObject(d, "registration", reg);
  if (type) cJSON_AddStringToObject(d, "type", type);
  if (alt && cJSON_IsNumber(alt)) cJSON_AddNumberToObject(d, "alt_baro_ft", alt->valuedouble);
  cJSON_AddStringToObject(d, "source", service);
  char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", service);
  if (hex) cJSON_AddStringToObject(p, "hex", hex);
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);

  char tags[96]; snprintf(tags, sizeof tags, "[\"osint-search\",\"%s\"]", service);
  char title[256];
  snprintf(title, sizeof title, "%s%s%s%s", flight ? flight : (reg ? reg : hex),
           type ? " · " : "", type ? type : "", "");
  char link[128] = {0};
  if (hex) snprintf(link, sizeof link, "https://globe.adsbexchange.com/?icao=%s", hex);
  int has_geo = (lat && cJSON_IsNumber(lat) && lon && cJSON_IsNumber(lon));
  intel_item it = {0};
  it.remote_key      = hex ? hex : (reg ? reg : flight);
  it.title           = title;
  it.summary         = reg ? reg : type;
  it.body            = bj;
  it.link            = link[0] ? link : NULL;
  it.lang            = "en";
  it.has_geo         = has_geo;
  it.lat             = has_geo ? lat->valuedouble : 0;
  it.lon             = has_geo ? lon->valuedouble : 0;
  it.record_type     = "aircraft-position";
  it.properties_json = pj;
  it.tags_json       = tags;
  int rc = sink->emit(sink, &it);
  free(bj); free(pj);
  return rc >= 0 ? 1 : 0;
}

static int run_reapi(const source_ctx *ctx, intel_sink *sink, const char *host,
                     const char *service, const char *enc, const char *kind) {
  char url[512];
  snprintf(url, sizeof url, "https://%s/v2/%s/%s", host, kind, enc);
  const char *hdrs[] = { "Accept: application/json", AV_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, service);
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *ac = cJSON_GetObjectItem(root, "ac");
  int emitted = 0;
  if (cJSON_IsArray(ac)) {
    const cJSON *a; cJSON_ArrayForEach(a, ac) { emitted += av_emit_ac(sink, service, a); }
  }
  cJSON_Delete(root);
  return emitted;
}

/* OpenSky states/all filtered by icao24 (hex only). state vector is a positional
 * array: [icao24,callsign,origin_country,time_position,last_contact,lon,lat,...]. */
static int run_opensky(const source_ctx *ctx, intel_sink *sink, const char *enc, const char *kind) {
  if (strcmp(kind, "hex") != 0) return 0;   /* OpenSky keyless filter is by icao24 */
  char url[512];
  snprintf(url, sizeof url, "https://opensky-network.org/api/states/all?icao24=%s", enc);
  const char *hdrs[] = { "Accept: application/json", AV_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "OPENSKY_STATES");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *states = cJSON_GetObjectItem(root, "states");
  int emitted = 0;
  if (cJSON_IsArray(states)) {
    const cJSON *s;
    cJSON_ArrayForEach(s, states) {
      if (!cJSON_IsArray(s)) continue;
      const cJSON *icao = cJSON_GetArrayItem(s, 0);  /* exhaustive-ok: OpenSky state vector is a fixed tuple */
      const cJSON *cs   = cJSON_GetArrayItem(s, 1);
      const cJSON *cc   = cJSON_GetArrayItem(s, 2);
      const cJSON *lon  = cJSON_GetArrayItem(s, 5);
      const cJSON *lat  = cJSON_GetArrayItem(s, 6);
      const char *hex = cJSON_IsString(icao) ? icao->valuestring : NULL;
      if (!hex) continue;
      /* OpenSky pads callsign to 8 chars too. */
      char csbuf[32];
      const char *csv = cJSON_IsString(cs)
                      ? av_trim(cs->valuestring, csbuf, sizeof csbuf) : NULL;
      cJSON *d = cJSON_CreateObject();
      cJSON_AddStringToObject(d, "hex", hex);
      if (csv) cJSON_AddStringToObject(d, "callsign", csv);
      if (cJSON_IsString(cc)) cJSON_AddStringToObject(d, "origin_country", cc->valuestring);
      cJSON_AddStringToObject(d, "source", "OpenSky Network");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "OPENSKY_STATES");
      cJSON_AddStringToObject(p, "hex", hex);
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      int has_geo = (cJSON_IsNumber(lat) && cJSON_IsNumber(lon));
      char link[128]; snprintf(link, sizeof link, "https://opensky-network.org/aircraft-profile?icao24=%s", hex);
      intel_item it = {0};
      it.remote_key = hex;
      it.title = csv ? csv : hex;
      it.summary = cJSON_IsString(cc) ? cc->valuestring : NULL;
      it.body = bj; it.link = link; it.lang = "en";
      it.has_geo = has_geo; it.lat = has_geo ? lat->valuedouble : 0; it.lon = has_geo ? lon->valuedouble : 0;
      it.record_type = "aircraft-position";
      it.properties_json = pj;
      it.tags_json = "[\"osint-search\",\"OPENSKY_STATES\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
      /* (cap removed: every record the upstream returned is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return emitted;
}

/* Planespotters photos by hex or registration. */
static int run_planespotters(const source_ctx *ctx, intel_sink *sink, const char *enc, const char *kind) {
  const char *seg = strcmp(kind, "reg") == 0 ? "reg" : "hex";
  char url[512];
  snprintf(url, sizeof url, "https://api.planespotters.net/pub/photos/%s/%s", seg, enc);
  const char *hdrs[] = { "Accept: application/json", AV_UA, NULL };
  char *body = jo_get(ctx, url, hdrs, "PLANESPOTTERS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  const cJSON *photos = cJSON_GetObjectItem(root, "photos");
  int emitted = 0;
  if (cJSON_IsArray(photos)) {
    const cJSON *ph;
    cJSON_ArrayForEach(ph, photos) {
      const char *id = jo_sv(ph, "id");
      const char *photographer = jo_sv(ph, "photographer");
      const cJSON *link = cJSON_GetObjectItem(ph, "link");
      const char *linkstr = cJSON_IsString(link) ? link->valuestring : NULL;
      const cJSON *thumb = cJSON_GetObjectItem(ph, "thumbnail_large");
      const char *thumbsrc = thumb ? jo_sv(thumb, "src") : NULL;
      if (!id && !linkstr) continue;
      cJSON *d = cJSON_CreateObject();
      if (photographer) cJSON_AddStringToObject(d, "photographer", photographer);
      if (thumbsrc) cJSON_AddStringToObject(d, "thumbnail", thumbsrc);
      cJSON_AddStringToObject(d, "source", "Planespotters");
      char *bj = cJSON_PrintUnformatted(d); cJSON_Delete(d);
      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "service", "PLANESPOTTERS");
      cJSON_AddBoolToObject(p, "success", 1);
      char *pj = cJSON_PrintUnformatted(p); cJSON_Delete(p);
      char title[128]; snprintf(title, sizeof title, "Aircraft photo %s", enc);
      intel_item it = {0};
      it.remote_key = id ? id : linkstr;
      it.title = title;
      it.summary = photographer;
      it.body = bj; it.link = linkstr; it.lang = "en";
      it.record_type = "aircraft-photo";
      it.properties_json = pj;
      it.tags_json = "[\"osint-search\",\"PLANESPOTTERS\"]";
      if (sink->emit(sink, &it) >= 0) emitted++;
      free(bj); free(pj);
      /* (cap removed: every record the upstream returned is emitted —
       * docs/SOURCE_EXHAUSTIVENESS.md) */
    }
  }
  cJSON_Delete(root);
  return emitted;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = ctx->entity;
  if (!q || !*q) return 0;
  const char *sid = ctx->source_id ? ctx->source_id : "";
  const char *kind = av_kind(q);
  char *enc = jo_urlencode(q);
  if (!enc) return 0;
  int n = 0;
  if      (!strcmp(sid, "ADSB_LOL"))       n = run_reapi(ctx, sink, "api.adsb.lol", "ADSB_LOL", enc, kind);
  else if (!strcmp(sid, "AIRPLANES_LIVE")) n = run_reapi(ctx, sink, "api.airplanes.live", "AIRPLANES_LIVE", enc, kind);
  else if (!strcmp(sid, "OPENSKY_STATES")) n = run_opensky(ctx, sink, enc, kind);
  else if (!strcmp(sid, "PLANESPOTTERS"))  n = run_planespotters(ctx, sink, enc, kind);
  free(enc);
  fprintf(stderr, "[%s] emitted %d\n", sid, n);
  return 0;
}

#define AV_DEF(sym, ID, NM, DESC) \
  static const source_def sym = { .id = ID, .collector = "osint", .name = NM, \
    .name_ja = NM, .update_interval_sec = 0, .run = run, .category = "infrastructure", \
    .type = "api", .url = "internal://osint/aviation", .description = DESC, \
    .layer = NULL, .free_tier = 1 }; \
  REGISTER_SOURCE(sym)

AV_DEF(av_adsblol_def, "ADSB_LOL", "ADSB.lol Aircraft",
  "adsb.lol keyless live ADS-B by hex/registration/callsign (position, type).");
AV_DEF(av_airplaneslive_def, "AIRPLANES_LIVE", "airplanes.live Aircraft",
  "airplanes.live keyless live ADS-B by hex/registration/callsign.");
AV_DEF(av_opensky_def, "OPENSKY_STATES", "OpenSky Network States",
  "OpenSky keyless state vectors filtered by ICAO24 hex (callsign, country, position).");
AV_DEF(av_planespotters_def, "PLANESPOTTERS", "Planespotters Photos",
  "Planespotters keyless aircraft photos by hex or registration.");
