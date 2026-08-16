/* US EPA ECHO enforcement and compliance facility search.
 * Endpoints (keyless, TWO-STEP and chained in one run because QueryIDs are
 * server-side and short-lived):
 *   1. https://echodata.epa.gov/echo/echo_rest_services.get_facility_info
 *        ?output=JSON&p_st=<ST>&p_act=Y&responseset=3   -> Results.QueryID
 *   2. https://echodata.epa.gov/echo/echo_rest_services.get_qid
 *        ?output=JSON&qid=<QueryID>&pageno=1            -> Results.Facilities[]
 * Emits one row per facility returned by step 2: name, compliance status,
 * inspection count, last inspection date, NAICS codes, penalties.
 *
 * CENTROID TRAP (R2), quoted from the source survey: get_facility_info returns
 * "a ClusterOutput block whose ClusterLatitude is a STATE CENTROID - never emit
 * that as a facility location". Step 1's response is therefore used ONLY for
 * the QueryID; every coordinate comes from step 2's per-facility
 * FacLat/FacLong. If step 2 fails we emit nothing rather than fall back.
 * STRING TRAP: all numeric-looking ECHO fields are strings; TotalPenalties is a
 * formatted currency string and is kept verbatim as a string.
 * Licence: US EPA ECHO, public domain, keyless. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "epa-echo-facilities"

static const char *STATES[] = { "NJ", "TX", "CA", NULL };

static void addstr(cJSON *p, const cJSON *o, const char *k, const char *out) {
  const char *s = jo_sv(o, k);
  if (s) cJSON_AddStringToObject(p, out, s);
}

static int collect_state(const source_ctx *ctx, intel_sink *sink,
                         const char *st, int *fetched) {
  char url[320];
  snprintf(url, sizeof url,
    "https://echodata.epa.gov/echo/echo_rest_services.get_facility_info"
    "?output=JSON&p_st=%s&p_act=Y&responseset=3", st);
  cJSON *step1 = feed_get_json(ctx->http, url, 45000);
  if (!step1) return 0;
  *fetched = 1;

  cJSON *r1 = cJSON_GetObjectItem(step1, "Results");
  /* copy the QueryID out before step1 is freed — the string lives inside it */
  char qid[64] = {0};
  if (r1) {
    const char *qs = jo_sv(r1, "QueryID");
    if (qs) snprintf(qid, sizeof qid, "%s", qs);
    else {
      cJSON *q = cJSON_GetObjectItem(r1, "QueryID");
      if (cJSON_IsNumber(q))
        snprintf(qid, sizeof qid, "%lld", (long long)q->valuedouble);
    }
  }
  cJSON_Delete(step1);          /* step 1 gives ONLY the QueryID — the
                                 * ClusterLatitude in it is a state centroid */
  if (!qid[0]) return 0;

  snprintf(url, sizeof url,
    "https://echodata.epa.gov/echo/echo_rest_services.get_qid"
    "?output=JSON&qid=%s&pageno=1", qid);
  cJSON *step2 = feed_get_json(ctx->http, url, 45000);
  if (!step2) return 0;

  cJSON *r2 = cJSON_GetObjectItem(step2, "Results");
  cJSON *facs = r2 ? cJSON_GetObjectItem(r2, "Facilities") : NULL;
  if (!cJSON_IsArray(facs)) { cJSON_Delete(step2); return 0; }

  int n = 0;
  cJSON *fac;
  cJSON_ArrayForEach(fac, facs) {
    const char *name = jo_sv(fac, "FacName");
    if (!name) continue;
    const char *reg = jo_sv(fac, "RegistryID");

    /* per-facility coordinates ONLY (R2) — strings in ECHO */
    int has_geo = 0; double lat = 0, lon = 0;
    const char *las = jo_sv(fac, "FacLat"), *los = jo_sv(fac, "FacLong");
    if (las && los) {
      char *e1 = NULL, *e2 = NULL;
      lat = strtod(las, &e1); lon = strtod(los, &e2);
      if (e1 != las && e2 != los && (lat != 0 || lon != 0) &&
          lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180) has_geo = 1;
    }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "facility_name", name);
    cJSON_AddStringToObject(p, "state", st);
    if (reg) cJSON_AddStringToObject(p, "registry_id", reg);
    addstr(p, fac, "FacCity", "city");
    addstr(p, fac, "FacNAICSCodes", "naics_codes");
    addstr(p, fac, "FacComplianceStatus", "compliance_status");
    addstr(p, fac, "CAAComplianceStatus", "caa_compliance_status");
    addstr(p, fac, "FacInspectionCount", "inspection_count");
    addstr(p, fac, "FacDateLastInspection", "last_inspection_date");
    /* TotalPenalties is a formatted currency STRING ($ and commas) — kept as
     * text rather than silently mis-parsed into a number */
    addstr(p, fac, "FacPenaltyCount", "penalty_count");
    addstr(p, fac, "FacTotalPenalties", "total_penalties_usd_text");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *cs = jo_sv(fac, "FacComplianceStatus");
    char key[128], title[320];
    snprintf(key, sizeof key, "%s|%s", st, reg ? reg : name);
    snprintf(title, sizeof title, "%s (%s): %s", name, st,
             cs ? cs : "regulated facility");

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = "https://echo.epa.gov/";
    row.record_type     = "regulated-facility";
    row.has_geo         = has_geo;
    row.lat = lat; row.lon = lon;
    row.properties_json = pj;
    row.tags_json       = "[\"industry\",\"compliance\",\"epa\",\"usa\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(step2);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0, fetched = 0;
  for (int i = 0; STATES[i]; i++)
    total += collect_state(ctx, sink, STATES[i], &fetched);
  if (!fetched) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  fprintf(stderr, "[" SRC "] emitted %d\n", total);
  return 0;
}

static const source_def eni_epa_echo_facilities_def = {
  .id = SRC, .collector = "industry",
  .name = "US EPA ECHO enforcement and compliance facilities",
  .update_interval_sec = 86400, .run = run,
  .category = "industry", .type = "api",
  .url = "https://echodata.epa.gov/echo/echo_rest_services.get_facility_info",
  .description = "Clean Air Act / Clean Water Act / RCRA regulated industrial facilities with current compliance status, inspection counts and penalties, with per-facility coordinates.",
  .license = "US EPA ECHO, public domain, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_epa_echo_facilities_def)
