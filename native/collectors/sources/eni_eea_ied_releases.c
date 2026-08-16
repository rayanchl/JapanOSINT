/* EEA Industrial Emissions (IED / E-PRTR) pollutant releases.
 * Endpoint: https://discodata.eea.europa.eu/sql?query=<url-encoded T-SQL>
 * DiscoData is a read-only SQL-over-HTTP gateway: the whole T-SQL query goes in
 * ?query= percent-encoded, and TOP n must be INSIDE the SQL (p/nrOfHits only
 * page the already-computed result). Response is {"results":[...]}; a bad
 * column name answers HTTP 200 with {"errors":[...]}, which is checked for.
 * Keyless.
 *
 * Emits one row per (facility, pollutant, medium): total_pollutant_quantity
 * (UNIT: KILOGRAMS per reporting year) + mediumCode (AIR/WATER/LAND).
 * Numbers arrive in SCIENTIFIC NOTATION as JSON numbers (4.4e+002 = 440) —
 * fine as doubles, and never string-formatted back out.
 * COORDINATE TRAP, quoted from the source survey: "use x_4326 (LONGITUDE) and
 * y_4326 (LATITUDE) - the x/y naming is backwards from lat/lon convention and
 * swapping them puts every European factory in the Sahara." x_3857/y_3857 are
 * Web Mercator METRES and are not requested at all.
 * Licence: EEA standard re-use policy — free reuse with attribution. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "eea-ied-pollutant-release"

static const char *URL =
  "https://discodata.eea.europa.eu/sql?query="
  "SELECT%20TOP%201000%20f.facilityName%2Cf.countryCode%2Cf.x_4326%2Cf.y_4326"
  "%2Cr.pollutant%2Cr.totalPollutantQuantityKg%2Cr.mediumCode"
  "%20FROM%20%5BIED%5D.%5Blatest%5D.%5BPollutantRelease%5D%20r"
  "%20JOIN%20%5BIED%5D.%5Blatest%5D.%5BProductionFacility%5D%20f"
  "%20ON%20f.id%3Dr.facilityReportId&p=1&nrOfHits=1000";

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, URL, 60000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  /* HTTP 200 + {"errors":[...]} is how a bad column name comes back */
  if (cJSON_GetObjectItem(doc, "errors")) {
    cJSON_Delete(doc);
    fprintf(stderr, "[" SRC "] upstream returned errors[]\n");
    return -1;
  }
  cJSON *res = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(res)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no results[]\n"); return -1; }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, res) {
    const char *name = jo_sv(r, "facilityName");
    const char *poll = jo_sv(r, "pollutant");
    cJSON *q = cJSON_GetObjectItem(r, "totalPollutantQuantityKg");
    if (!name || !poll || !cJSON_IsNumber(q)) continue;  /* withheld -> skip */

    /* x_4326 is LONGITUDE, y_4326 is LATITUDE (backwards from convention) */
    int has_geo = 0; double lat = 0, lon = 0;
    cJSON *x = cJSON_GetObjectItem(r, "x_4326");
    cJSON *y = cJSON_GetObjectItem(r, "y_4326");
    if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
      lon = x->valuedouble; lat = y->valuedouble;
      if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180 &&
          (lat != 0 || lon != 0)) has_geo = 1;
    }

    const char *cc = jo_sv(r, "countryCode");
    const char *med = jo_sv(r, "mediumCode");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "facility_name", name);
    if (cc) cJSON_AddStringToObject(p, "country", cc);
    cJSON_AddStringToObject(p, "pollutant", poll);
    cJSON_AddNumberToObject(p, "total_pollutant_quantity", q->valuedouble);
    cJSON_AddStringToObject(p, "unit", "kg per reporting year");
    if (med) cJSON_AddStringToObject(p, "medium", med);
    cJSON *acc = cJSON_GetObjectItem(r, "accidentalPollutantQuantityKg");
    if (cJSON_IsNumber(acc))
      cJSON_AddNumberToObject(p, "accidental_quantity_kg", acc->valuedouble);
    const char *mc = jo_sv(r, "methodCode");
    if (mc) cJSON_AddStringToObject(p, "method_code", mc);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[320], title[352];
    snprintf(key, sizeof key, "%s|%s|%s|%s", cc ? cc : "", name, poll,
             med ? med : "");
    snprintf(title, sizeof title, "%s (%s): %s to %s = %.0f kg/yr",
             name, cc ? cc : "?", poll, med ? med : "?", q->valuedouble);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = "https://industry.eea.europa.eu/";
    row.record_type     = "industrial-release";
    row.has_geo         = has_geo;
    row.lat = lat; row.lon = lon;
    row.properties_json = pj;
    row.tags_json       = "[\"industry\",\"emissions\",\"europe\",\"eea\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_eea_ied_releases_def = {
  .id = SRC, .collector = "industry",
  .name = "EEA Industrial Emissions (IED/E-PRTR) pollutant releases",
  .update_interval_sec = 604800, .run = run,
  .category = "industry", .type = "api",
  .url = "https://discodata.eea.europa.eu/sql",
  .description = "European industrial pollutant release register: reported annual releases (kg/yr) of each pollutant to air, water and land for large industrial installations, with coordinates.",
  .license = "EEA standard re-use policy — free reuse with attribution (DiscoData SQL endpoint, keyless).",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_eea_ied_releases_def)
