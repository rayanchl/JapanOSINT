/* collectors/sources/av_awc_taf.c
 * NOAA/NWS Aviation Weather Center — TAF terminal aerodrome forecasts.
 * Endpoint: https://aviationweather.gov/api/data/taf?ids={icao_list}&format=json
 * (a bare top-level JSON array; up to 50 ICAO identifiers per call, verified).
 *
 * Emits, all parsed from the reply: icaoId, name, issueTime, bulletinTime,
 * validTimeFrom, validTimeTo, remarks, elev, prior/mostRecent, the untouched
 * rawTAF bulletin, and the decoded `fcsts[]` forecast periods copied verbatim.
 *
 * GEOMETRY (R2): `lat`/`lon` are the AERODROME's coordinates as held in AWC's
 * own station table — real, not derived — so rows are tagged
 * geo_precision:"station". Stations with no table entry return no lat/lon;
 * those rows are emitted with has_geo=0 rather than guessed from the ICAO
 * prefix.
 *
 * The `ids` list below is a QUERY PARAMETER, not emitted content: a fixed
 * watch list of major aerodromes worldwide (all Japanese majors plus the main
 * hubs of every region), swept in chunks of 50. Nothing about a station is
 * emitted unless AWC returned a TAF for it, so a station that is silent simply
 * produces no row.
 *
 * Distinct endpoint from the pre-existing AVIATION_METAR source, which only
 * calls /api/data/metar.
 * R3: a chunk that returns an empty array is an honest empty; -1 only when the
 * first chunk's fetch fails.
 *
 * Licence: US Government work (NOAA/NWS Aviation Weather Center), public
 * domain, keyless.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "av_common.inc"

/* Watch list — query input only. Japan first, then the rest of the world. */
static const char *TAF_IDS[] = {
  "RJTT","RJAA","RJBB","RJOO","RJCC","RJFF","RJGG","RJNK","RJSS","RJOA",
  "RJSN","RJCH","RJFT","RJOM","RJDT","RJOT","RJSK","RJTF","RJTY","ROAH",
  "RODN","RJBE","RJOB","RJEC","RJSA","RJSF","RJSM","RJTH","RJOK","RJFK",
  "EGLL","EGKK","EGSS","EGCC","EGPH","EIDW","LFPG","LFPO","LFML","LFLL",
  "EDDF","EDDM","EDDB","EDDL","EDDH","EHAM","EBBR","ELLX","LSZH","LSGG",
  "LOWW","LKPR","EPWA","LHBP","LROP","LBSF","LDZA","LJLJ","LZIB","EETN",
  "EVRA","EYVI","EFHK","ESSA","ENGM","EKCH","BIKF","LPPT","LEMD","LEBL",
  "LIRF","LIMC","LGAV","LCLK","LMML","LTFM","LTAI","UUEE","UUDD","ULLI",
  "UKBB","UMMS","UBBB","UDYZ","OMDB","OMAA","OTHH","OERK","OEJN","OKBK",
  "OBBI","OOMS","OJAI","OLBA","LLBG","OIIE","OPKC","OPIS","VIDP","VABB",
  "VOMM","VOBL","VECC","VGHS","VCBI","VNKT","VRMM","VTBS","VTSP","VDPP",
  "VLVT","VVNB","VVTS","WMKK","WSSS","WIII","WADD","WBSB","RPLL","RPVM",
  "ZBAA","ZBAD","ZSPD","ZSSS","ZGGG","ZGSZ","ZUUU","ZPPP","ZLXY","ZYTX",
  "VHHH","VMMC","RCTP","RCKH","RKSI","RKPC","RKPK","ZKPY","UAAA","UTTT",
  "YSSY","YMML","YBBN","YPPH","YBCS","NZAA","NZCH","NZWN","NFFN","NVVV",
  "PHNL","PANC","KJFK","KEWR","KLGA","KBOS","KIAD","KDCA","KATL","KMIA",
  "KMCO","KORD","KMDW","KDFW","KIAH","KDEN","KPHX","KLAS","KLAX","KSFO",
  "KSEA","KSAN","KSLC","KMSP","KDTW","KPHL","KCLT","KTPA","KBWI","KSTL",
  "CYYZ","CYUL","CYVR","CYYC","CYOW","CYWG","MMMX","MMUN","MMGL","MMTJ",
  "SBGR","SBGL","SBBR","SAEZ","SCEL","SKBO","SPJC","SVMI","MPTO","MDSD",
  "MKJP","TJSJ","FAOR","FACT","HKJK","HAAB","DNMM","GOBD","DGAA","HECA",
  "DTTA","DAAG","GMMN","FIMP","FMEE","FQMA","FZAA","HTDA","HUEN","FLKK",
  NULL };
#define TAF_CHUNK 50

static const char *TAF_KEYS[] = {
  "icaoId","name","dbPopTime","bulletinTime","issueTime","rawTAF","remarks",
  "elev","prior","mostRecent", NULL };

static int taf_chunk(const source_ctx *ctx, intel_sink *sink, int start,
                     int *emitted) {
  char ids[TAF_CHUNK * 6 + 8];
  size_t j = 0;
  int n = 0;
  for (int i = start; TAF_IDS[i] && n < TAF_CHUNK; i++, n++) {
    int w = snprintf(ids + j, sizeof ids - j, "%s%s", n ? "," : "", TAF_IDS[i]);
    if (w < 0 || (size_t)w >= sizeof ids - j) break;
    j += (size_t)w;
  }
  if (n == 0) return 0;
  char url[512];
  snprintf(url, sizeof url,
           "https://aviationweather.gov/api/data/taf?ids=%s&format=json", ids);
  const char *hdrs[] = { "Accept: application/json", AV_UA_HDR, NULL };
  char *body = jo_get(ctx, url, hdrs, "awc-taf");
  if (!body) return -1;
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc || !cJSON_IsArray(doc)) { cJSON_Delete(doc); return -1; }

  cJSON *feats = cJSON_CreateArray();
  cJSON *r;
  cJSON_ArrayForEach(r, doc) {
    if (!cJSON_IsObject(r)) continue;
    const char *icao = jo_sv(r, "icaoId");
    const char *raw  = jo_sv(r, "rawTAF");
    if (!icao || !raw) continue;               /* no bulletin → no row (R1) */
    const char *name = jo_sv(r, "name");
    const char *issue = jo_sv(r, "issueTime");

    cJSON *geom = NULL;
    double lat, lon;
    if (av_dbl(r, "lat", &lat) && av_dbl(r, "lon", &lon) &&
        lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) {
      geom = cJSON_CreateObject();
      cJSON_AddStringToObject(geom, "type", "Point");
      cJSON *c = cJSON_CreateArray();
      cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
      cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
      cJSON_AddItemToObject(geom, "coordinates", c);
    }

    char uid[96];
    snprintf(uid, sizeof uid, "%s|%s", icao, issue ? issue : "");
    char title[288];
    snprintf(title, sizeof title, "TAF %s%s%s", icao, name ? " — " : "",
             name ? name : "");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "uid", uid);
    cJSON_AddStringToObject(p, "title", title);
    for (int k = 0; TAF_KEYS[k]; k++) av_copy(p, r, TAF_KEYS[k]);
    if (issue) cJSON_AddStringToObject(p, "published_at", issue);
    double vf = 0, vt = 0;
    char iso[32];
    if (av_dbl(r, "validTimeFrom", &vf)) {
      const char *s = av_epoch_iso(vf, iso, sizeof iso);
      if (s) cJSON_AddStringToObject(p, "valid_from", s);
    }
    char iso2[32];
    if (av_dbl(r, "validTimeTo", &vt)) {
      const char *s = av_epoch_iso(vt, iso2, sizeof iso2);
      if (s) cJSON_AddStringToObject(p, "valid_until", s);
    }
    const cJSON *fc = cJSON_GetObjectItem(r, "fcsts");
    if (cJSON_IsArray(fc))
      cJSON_AddItemToObject(p, "fcsts", cJSON_Duplicate(fc, 1));
    if (geom) cJSON_AddStringToObject(p, "geo_precision", "station");
    cJSON_AddStringToObject(p, "record_type", "taf");
    cJSON_AddStringToObject(p, "source", "NWS Aviation Weather Center");
    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("aviation"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("taf"));
    cJSON_AddItemToObject(p, "tags", tags);
    cJSON_AddItemToArray(feats, av_feature(geom, p));
  }
  *emitted += geojson_emit_features(sink, ctx->source_id, feats);
  cJSON_Delete(feats);
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int emitted = 0, start = 0, first = 1;
  for (;;) {
    int n = taf_chunk(ctx, sink, start, &emitted);
    if (n < 0) {
      if (first) { fprintf(stderr, "[awc-taf] fetch failed\n"); return -1; }
      break;                       /* later chunk failed → keep what we have */
    }
    first = 0;
    if (n == 0) break;
    start += n;
  }
  fprintf(stderr, "[awc-taf] emitted %d\n", emitted);
  return 0;
}

static const source_def av_awc_taf_def = {
  .id = "awc-taf", .collector = "transport",
  .name = "AWC TAF Terminal Forecasts",
  .update_interval_sec = 1800, .run = run,
  .category = "transport", .type = "api",
  .url = "https://aviationweather.gov/api/data/taf?format=json",
  .description = "Terminal Aerodrome Forecasts for a watch list of major aerodromes worldwide (all Japanese majors plus regional hubs) — raw bulletin, decoded forecast periods, validity window and the aerodrome's own coordinates. Pairs with the existing METAR source for current + forecast conditions.",
  .license = "US Government work (NOAA/NWS Aviation Weather Center) — public domain, keyless.",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(av_awc_taf_def)
