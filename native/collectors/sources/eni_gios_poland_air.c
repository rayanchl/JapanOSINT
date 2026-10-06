/* GIOS Poland air quality measurements (PJP API v1).
 * Endpoints (keyless; the legacy /pjp-api/rest/... paths are gone, HTTP 410 —
 * only the /v1/ paths work). THREE-STEP:
 *   1 https://api.gios.gov.pl/pjp-api/v1/rest/station/findAll?size=500&page=<0..>
 *       -> "Lista stacji pomiarowych"[] with "Identyfikator stacji",
 *          "Nazwa stacji", "WGS84 φ N" / "WGS84 λ E" (STRINGS), and
 *          `totalPages` — walked to the end (page is 0-based)
 *   2 .../v1/rest/station/sensors/{stationId}
 *       -> "Lista stanowisk pomiarowych dla podanej stacji"[] with
 *          "Identyfikator stanowiska" and "Wskaźnik - kod"
 *   3 .../v1/rest/data/getData/{sensorId}?size=24
 *       -> "Lista danych pomiarowych"[] with "Data" and "Wartość"
 * Emits one row per (station, pollutant): value (UNIT: ug/m3 per the API's own
 * meta description), pollutant code, measurement hour, station coordinates.
 *
 * NULL TRAP: the most recent hour routinely has a null "Wartość" —
 * nulls are skipped and the newest NON-NULL sample is taken; a null is never
 * coerced to 0.
 * MANUAL-SENSOR TRAP: manual stanowiska answer HTTP 200 with an
 * {"error_result": ...} envelope; that key is checked before parsing.
 * JSON KEYS ARE POLISH with diacritics, and the document carries TWO "@context"
 * keys — key uniqueness is never assumed, only the Polish keys are read.
 * Station budget: JO_GIOS_STATIONS (default 15) bounds the request fan-out.
 * It is a bound on how many STATIONS get their sensors read, so when it bites
 * the shortfall is emitted as a collector-truncation-notice (scope
 * "stations") against the full station list, never silent.
 *
 * The station list itself used to be ONE page (size=100&page=0) of a list the
 * upstream pages: measured 2026-10-02 it holds 288 stations, totalPages=3 at
 * size=100, so 188 stations could never be reached whatever the budget said.
 * It is now read at size=500 and walked page by page to `totalPages`.
 * Licence: GIOS public API, keyless. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "gios-poland-air"

static const char *K_STATIONS = "Lista stacji pomiarowych";
static const char *K_STID     = "Identyfikator stacji";
static const char *K_STNAME   = "Nazwa stacji";
static const char *K_LAT      = "WGS84 \xcf\x86 N";      /* WGS84 φ N */
static const char *K_LON      = "WGS84 \xce\xbb E";      /* WGS84 λ E */
static const char *K_SENSORS  = "Lista stanowisk pomiarowych dla podanej stacji";
static const char *K_SENSID   = "Identyfikator stanowiska";
static const char *K_CODE     = "Wska\xc5\xbanik - kod";  /* Wskaźnik - kod */
static const char *K_DATA     = "Lista danych pomiarowych";
static const char *K_DATE     = "Data";
static const char *K_VALUE    = "Warto\xc5\x9b\xc4\x87"; /* Wartość */

static const char *WANT[] = { "PM10", "PM2.5", "NO2", "O3", "SO2", NULL };

static long long iv(const cJSON *o, const char *k, long long dflt) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (cJSON_IsNumber(v)) return (long long)v->valuedouble;
  if (cJSON_IsString(v) && v->valuestring[0]) return atoll(v->valuestring);
  return dflt;
}
static int wanted(const char *code) {
  for (int i = 0; WANT[i]; i++) if (strcmp(WANT[i], code) == 0) return 1;
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int budget = 15;
  const char *env = getenv("JO_GIOS_STATIONS");
  if (env && *env) { int b = atoi(env); if (b > 0) budget = b; }

  /* The whole station list, every page of it, gathered into one array. */
  cJSON *sdoc = cJSON_CreateArray();
  if (!sdoc) return -1;
  int total_pages = 1;
  for (int page = 0; page < total_pages; page++) {
    char surl[160];
    snprintf(surl, sizeof surl, "https://api.gios.gov.pl/pjp-api/v1/rest/"
             "station/findAll?size=500&page=%d", page);
    cJSON *pdoc = feed_get_json(ctx->http, surl, 30000);
    if (!pdoc) {
      if (page == 0) {
        cJSON_Delete(sdoc);
        fprintf(stderr, "[" SRC "] station fetch failed\n");
        return -1;
      }
      jo_trunc_notice_scoped(sink, SRC, "station-list", surl,
        cJSON_GetArraySize(sdoc), -1, "a later page of the station list failed",
        "re-run; the station list is walked from page 0 every run");
      break;
    }
    const cJSON *tp = cJSON_GetObjectItem(pdoc, "totalPages");
    if (cJSON_IsNumber(tp) && tp->valuedouble > total_pages)
      total_pages = (int)tp->valuedouble;
    cJSON *list = cJSON_GetObjectItem(pdoc, K_STATIONS);
    int got = 0;
    if (cJSON_IsArray(list)) {
      cJSON *it;
      while ((it = cJSON_DetachItemFromArray(list, 0)) != NULL) {
        cJSON_AddItemToArray(sdoc, it);
        got++;
      }
    }
    cJSON_Delete(pdoc);
    if (!got) break;                       /* past the last page */
  }
  cJSON *stations = sdoc;
  if (cJSON_GetArraySize(stations) == 0) {
    cJSON_Delete(sdoc);
    fprintf(stderr, "[" SRC "] no station list\n");
    return -1;
  }
  const int nstations = cJSON_GetArraySize(stations);
  if (nstations > budget)
    jo_trunc_notice_scoped(sink, SRC, "stations",
      "https://api.gios.gov.pl/pjp-api/v1/rest/station/findAll", budget,
      nstations, "JO_GIOS_STATIONS bounds how many stations have their "
      "sensors read per run (request fan-out)",
      "raise JO_GIOS_STATIONS (each station costs ~4 requests)");

  int n = 0, used = 0;
  cJSON *st;
  cJSON_ArrayForEach(st, stations) {
    if (used >= budget) break;
    long long stid = iv(st, K_STID, -1);
    const char *stname = jo_sv(st, K_STNAME);
    if (stid < 0 || !stname) continue;
    /* coordinates are STRINGS in this API */
    const char *las = jo_sv(st, K_LAT), *los = jo_sv(st, K_LON);
    int has_geo = 0; double lat = 0, lon = 0;
    if (las && los) {
      char *e1 = NULL, *e2 = NULL;
      lat = strtod(las, &e1); lon = strtod(los, &e2);
      if (e1 != las && e2 != los && lat >= -90 && lat <= 90 &&
          lon >= -180 && lon <= 180 && (lat != 0 || lon != 0)) has_geo = 1;
    }
    used++;

    char url[192];
    snprintf(url, sizeof url,
      "https://api.gios.gov.pl/pjp-api/v1/rest/station/sensors/%lld", stid);
    cJSON *sensdoc = feed_get_json(ctx->http, url, 20000);
    if (!sensdoc) continue;
    cJSON *sensors = cJSON_GetObjectItem(sensdoc, K_SENSORS);
    if (!cJSON_IsArray(sensors)) { cJSON_Delete(sensdoc); continue; }

    cJSON *sen;
    cJSON_ArrayForEach(sen, sensors) {
      const char *code = jo_sv(sen, K_CODE);
      long long sid = iv(sen, K_SENSID, -1);
      if (!code || sid < 0 || !wanted(code)) continue;

      snprintf(url, sizeof url,
        "https://api.gios.gov.pl/pjp-api/v1/rest/data/getData/%lld?size=24", sid);
      cJSON *ddoc = feed_get_json(ctx->http, url, 20000);
      if (!ddoc) continue;
      /* manual stanowisko -> HTTP 200 with an error envelope */
      if (cJSON_GetObjectItem(ddoc, "error_result")) { cJSON_Delete(ddoc); continue; }
      cJSON *list = cJSON_GetObjectItem(ddoc, K_DATA);
      if (!cJSON_IsArray(list)) { cJSON_Delete(ddoc); continue; }

      /* newest NON-NULL sample; nulls are skipped, never read as zero */
      cJSON *best = NULL; const char *best_when = NULL;
      cJSON *d;
      cJSON_ArrayForEach(d, list) {
        cJSON *val = cJSON_GetObjectItem(d, K_VALUE);
        const char *when = jo_sv(d, K_DATE);
        if (!cJSON_IsNumber(val) || !when) continue;
        if (!best_when || strcmp(when, best_when) > 0) { best = d; best_when = when; }
      }
      if (!best) { cJSON_Delete(ddoc); continue; }
      double v = cJSON_GetObjectItem(best, K_VALUE)->valuedouble;

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "station_name", stname);
      cJSON_AddNumberToObject(p, "station_id", (double)stid);
      cJSON_AddStringToObject(p, "pollutant", code);
      cJSON_AddNumberToObject(p, "value", v);
      cJSON_AddStringToObject(p, "unit", "ug/m3");
      cJSON_AddStringToObject(p, "measured_at", best_when);
      cJSON_AddStringToObject(p, "country", "PL");
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      char key[128], title[288];
      snprintf(key, sizeof key, "%lld|%s|%s", stid, code, best_when);
      snprintf(title, sizeof title, "%s: %s = %.2f ug/m3", stname, code, v);

      intel_item row = {0};
      row.remote_key      = key;
      row.title           = title;
      row.summary         = title;
      row.published_at    = best_when;
      row.lang            = "pl";
      row.link            = "https://powietrze.gios.gov.pl/pjp/current";
      row.record_type     = "air-quality-measurement";
      row.has_geo         = has_geo;
      row.lat = lat; row.lon = lon;
      row.properties_json = pj;
      row.tags_json       = "[\"environment\",\"air-quality\",\"poland\"]";
      if (sink->emit(sink, &row) >= 0) n++;
      free(pj);
      cJSON_Delete(ddoc);
    }
    cJSON_Delete(sensdoc);
  }
  cJSON_Delete(sdoc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_gios_poland_air_def = {
  .id = SRC, .collector = "environment",
  .name = "GIOS Poland air quality measurements",
  .update_interval_sec = 3600, .run = run,
  .category = "environment", .type = "api",
  .url = "https://api.gios.gov.pl/pjp-api/v1/rest/data/getData/",
  .description = "Polish Chief Inspectorate of Environmental Protection hourly PM10/PM2.5/NO2/O3/SO2 measurements (ug/m3) with station coordinates.",
  .license = "GIOS PJP public API, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_gios_poland_air_def)
