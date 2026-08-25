/* EEA Industrial Emissions (IED / E-PRTR) pollutant releases.
 * Endpoint: https://discodata.eea.europa.eu/sql?query=<url-encoded T-SQL>
 * DiscoData is a read-only SQL-over-HTTP gateway: the whole T-SQL query goes in
 * ?query= percent-encoded, and p/nrOfHits page the computed result. So a `TOP n`
 * inside the SQL is not a page size, it is a HARD CAP on the result set that
 * paging can never get past — see the pagination note further down, which is
 * why there is no TOP in the query any more. Response is {"results":[...]}; a
 * bad column name answers HTTP 200 with {"errors":[...]}, which is checked for.
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

/* PAGINATION — and why `TOP n` had to go.
 *
 * The query used to open with `SELECT TOP 1000` and the request carried
 * `&p=1&nrOfHits=1000`. Those two do NOT compose: TOP truncates the RESULT SET
 * before p/nrOfHits page it, so page 2 of a TOP-1000 query comes back
 * `{"results":[]}` and the walk can never leave page 1. The join behind this
 * query has 140,743 rows (SELECT COUNT(*) against the same two tables); the
 * collector emitted the first 1,000 of them — 0.7% — with no error, no log and
 * no notice, which is precisely the failure docs/SOURCE_EXHAUSTIVENESS.md is
 * about.
 *
 * With TOP dropped, p/nrOfHits page the whole join: p=141 returns the last 743
 * rows and p=142 returns []. The page order is stable (fetching p=2 twice is
 * byte-identical, and p=1/p=2 do not overlap), which matters because DiscoData
 * REFUSES an ORDER BY — `...&query=...ORDER BY...` answers HTTP 200 with
 * {"errors":[{"errorcode":10002,"error":"Your query is not allowed
 * execution..."}]} — so the walk has to rely on the server's own ordering.
 *
 * The page ceiling below is a runaway guard, and a run that ends on it says so
 * as a collector-truncation-notice. */
/* The SELECT also grew: accidentalPollutantQuantityKg and methodCode were
 * already being read out of every row here, but were never in the column list,
 * so both were absent on every record ever emitted. parentCompanyName, city,
 * reportingYear and the E-PRTR main-activity code come from the same join at
 * no extra cost and are what makes a release attributable. */
#define EEA_QUERY                                                             \
  "https://discodata.eea.europa.eu/sql?query="                                \
  "SELECT%20f.facilityName%2Cf.parentCompanyName%2Cf.countryCode%2Cf.city"    \
  "%2Cf.reportingYear%2Cf.EPRTRAnnexIMainActivity%2Cf.x_4326%2Cf.y_4326"      \
  "%2Cr.pollutant%2Cr.totalPollutantQuantityKg"                               \
  "%2Cr.accidentalPollutantQuantityKg%2Cr.mediumCode%2Cr.methodCode"          \
  "%20FROM%20%5BIED%5D.%5Blatest%5D.%5BPollutantRelease%5D%20r"               \
  "%20JOIN%20%5BIED%5D.%5Blatest%5D.%5BProductionFacility%5D%20f"             \
  "%20ON%20f.id%3Dr.facilityReportId"
#define EEA_PAGE_SIZE 1000
#define EEA_MAX_PAGES 400   /* exhaustive-ok: page-walk runaway guard (the join is 141 pages today); an early stop emits a collector-truncation-notice */

static const char *URL = EEA_QUERY;

/* One page of the join. Returns rows emitted; *seen is the rows the page
 * carried, *hard_fail is set when the request itself failed or DiscoData
 * answered with errors[]. */
static int collect_page(const source_ctx *ctx, intel_sink *sink, int page,
                        int *seen, int *hard_fail) {
  char url[768];
  snprintf(url, sizeof url, "%s&p=%d&nrOfHits=%d", EEA_QUERY, page,
           EEA_PAGE_SIZE);
  cJSON *doc = feed_get_json(ctx->http, url, 90000);
  if (!doc) { *hard_fail = 1; return 0; }
  /* HTTP 200 + {"errors":[...]} is how a bad column name comes back */
  if (cJSON_GetObjectItem(doc, "errors")) {
    cJSON_Delete(doc);
    fprintf(stderr, "[" SRC "] upstream returned errors[] on page %d\n", page);
    *hard_fail = 1;
    return 0;
  }
  cJSON *res = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(res)) {
    cJSON_Delete(doc);
    fprintf(stderr, "[" SRC "] no results[] on page %d\n", page);
    *hard_fail = 1;
    return 0;
  }
  *seen = cJSON_GetArraySize(res);

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
    const char *parent = jo_sv(r, "parentCompanyName");
    if (parent) cJSON_AddStringToObject(p, "parent_company", parent);
    const char *city = jo_sv(r, "city");
    if (city) cJSON_AddStringToObject(p, "city", city);
    cJSON *ry = cJSON_GetObjectItem(r, "reportingYear");
    if (cJSON_IsNumber(ry))
      cJSON_AddNumberToObject(p, "reporting_year", ry->valuedouble);
    const char *act = jo_sv(r, "EPRTRAnnexIMainActivity");
    if (act) cJSON_AddStringToObject(p, "eprtr_main_activity", act);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    /* The reporting YEAR belongs in the key. Without it every year of the same
     * facility/pollutant/medium hashed to one remote_key, so the sink upserted
     * a 17-year series down to whichever row happened to arrive last — 140,743
     * fetched rows collapsing into a fraction of that in storage, which is the
     * same discard as never fetching them (rule 5: never discard at a seam). */
    char key[352], title[384];
    long year = cJSON_IsNumber(ry) ? (long)ry->valuedouble : 0;
    snprintf(key, sizeof key, "%s|%s|%s|%s|%ld", cc ? cc : "", name, poll,
             med ? med : "", year);
    snprintf(title, sizeof title, "%s (%s) %ld: %s to %s = %.0f kg/yr",
             name, cc ? cc : "?", year, poll, med ? med : "?", q->valuedouble);

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
  /* House rule 2: the DiscoData query carries SELECT TOP 1000 and &p=1, so a
   * result set that comes back exactly full was clipped upstream and no later
   * page is requested. DiscoData does not report the unclipped total. */
  if (cJSON_GetArraySize(res) >= 1000)
    jo_truncation_notice(sink, SRC, "IED PollutantRelease", n, -1,
                         "the SQL carries SELECT TOP 1000 and the request pins "
                         "p=1; the result came back exactly full, so releases "
                         "beyond it were never fetched",
                         "raise the TOP/nrOfHits bound and walk p=2,3,… in URL "
                         "in collectors/sources/eni_eea_ied_releases.c");
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int max_pages = EEA_MAX_PAGES;
  const char *penv = getenv("JO_EEA_IED_PAGES");
  if (penv && *penv) { int v = atoi(penv); if (v > 0) max_pages = v; }

  int n = 0, pages = 0, more_pending = 0;
  long rows_seen = 0;
  for (int page = 1; page <= max_pages; page++) {
    int seen = 0, hard_fail = 0;
    n += collect_page(ctx, sink, page, &seen, &hard_fail);
    if (hard_fail) {
      if (page == 1) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
      /* A page that failed mid-walk cost us every row after it. */
      jo_trunc_notice(sink, SRC, URL, n, -1,
                      "the DiscoData page walk stopped when a page failed; "
                      "later rows of the IED join were not read",
                      "re-run the collector; the walk restarts from page 1");
      break;
    }
    if (seen == 0) break;                     /* past the end of the join    */
    pages++;
    rows_seen += seen;
    if (seen < EEA_PAGE_SIZE) break;          /* short page = last page      */
    if (page == max_pages) more_pending = 1;
  }
  fprintf(stderr, "[" SRC "] emitted %d row(s) from %ld join row(s) over %d "
                  "page(s)\n", n, rows_seen, pages);
  if (more_pending)
    jo_trunc_notice(sink, SRC, URL, n, -1,
                    "the page-walk ceiling stopped the run while DiscoData was "
                    "still returning full pages of the IED join",
                    "raise $JO_EEA_IED_PAGES");
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
