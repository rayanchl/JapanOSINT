/* UK Power Networks live power cuts (Opendatasoft Explore v2.1).
 * Endpoint: https://ukpowernetworks.opendatasoft.com/api/explore/v2.1/catalog/
 *           datasets/ukpn-live-faults/records?limit=100                (keyless)
 * Envelope is {total_count, results[]}.
 * Emits one row per live/recent distribution fault: incident reference,
 * powercuttype (Planned vs Unplanned — the difference an analyst cares about),
 * customers affected (an integer COUNT, and 0 is legitimate on a just-raised
 * incident, so it is emitted rather than dropped), calls reported, affected
 * postcode districts, creation/restoration times.
 *
 * GEO (R2): geopoint is an OBJECT {lon, lat} (lon first in the raw JSON) and is
 * used directly. A location is NEVER derived from the postcode strings.
 * Licence: UK Power Networks Open Data Portal open data licence (OGL-compatible). */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "ukpn-live-faults"

static void addstr(cJSON *p, const cJSON *o, const char *k, const char *out) {
  const char *s = jo_sv(o, k);
  if (s) cJSON_AddStringToObject(p, out, s);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = "https://ukpowernetworks.opendatasoft.com/api/explore/v2.1/"
                    "catalog/datasets/ukpn-live-faults/records?limit=100";
  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  cJSON *res = cJSON_GetObjectItem(doc, "results");
  if (!cJSON_IsArray(res)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no results[]\n"); return -1; }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, res) {
    const char *ref = jo_sv(r, "incidentreference");
    if (!ref) continue;
    const char *type = jo_sv(r, "powercuttype");
    const char *created = jo_sv(r, "creationdatetime");

    /* geopoint is {lon, lat}; postcodes are NEVER geocoded here (R2) */
    int has_geo = 0; double lat = 0, lon = 0;
    cJSON *gp = cJSON_GetObjectItem(r, "geopoint");
    if (cJSON_IsObject(gp)) {
      cJSON *la = cJSON_GetObjectItem(gp, "lat");
      cJSON *lo = cJSON_GetObjectItem(gp, "lon");
      if (cJSON_IsNumber(la) && cJSON_IsNumber(lo)) {
        lat = la->valuedouble; lon = lo->valuedouble;
        if (lat >= -90 && lat <= 90 && lon >= -180 && lon <= 180 &&
            (lat != 0 || lon != 0)) has_geo = 1;
      }
    }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "incident_reference", ref);
    if (type) cJSON_AddStringToObject(p, "power_cut_type", type);
    cJSON *cust = cJSON_GetObjectItem(r, "nocustomeraffected");
    if (cJSON_IsNumber(cust)) {          /* 0 is a legitimate value */
      cJSON_AddNumberToObject(p, "customers_affected", cust->valuedouble);
      cJSON_AddStringToObject(p, "customers_affected_unit", "count of customers");
    }
    cJSON *calls = cJSON_GetObjectItem(r, "nocallsreported");
    if (cJSON_IsNumber(calls))
      cJSON_AddNumberToObject(p, "calls_reported", calls->valuedouble);
    addstr(p, r, "creationdatetime", "created_at");
    addstr(p, r, "restoreddatetime", "restored_at");
    addstr(p, r, "estimatedrestorationdate", "estimated_restoration");
    addstr(p, r, "postcodesaffected", "postcodes_affected");
    addstr(p, r, "incidenttypename", "incident_type");
    addstr(p, r, "statusid", "status_id");
    cJSON_AddStringToObject(p, "operator", "UK Power Networks");
    cJSON_AddStringToObject(p, "country", "GB");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[288];
    if (cJSON_IsNumber(cust))
      snprintf(title, sizeof title, "UKPN %s (%s): %.0f customers affected",
               ref, type ? type : "power cut", cust->valuedouble);
    else
      snprintf(title, sizeof title, "UKPN %s (%s)", ref, type ? type : "power cut");

    intel_item row = {0};
    row.remote_key      = ref;
    row.title           = title;
    row.summary         = title;
    row.published_at    = created;
    row.lang            = "en";
    row.link            = "https://ukpowernetworks.opendatasoft.com/explore/dataset/ukpn-live-faults/";
    row.record_type     = "power-outage";
    row.has_geo         = has_geo;
    row.lat = lat; row.lon = lon;
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"outage\",\"uk\",\"distribution\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_ukpn_live_faults_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "UK Power Networks live power cuts",
  .update_interval_sec = 300, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://ukpowernetworks.opendatasoft.com/api/explore/v2.1/catalog/datasets/ukpn-live-faults/records",
  .description = "Live and recent electricity distribution faults across UK Power Networks licence areas with customers affected and incident location.",
  .license = "UK Power Networks Open Data Portal open data licence (OGL-compatible), keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_ukpn_live_faults_def)
