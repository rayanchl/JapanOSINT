/* USGS NWIS instantaneous values — US river discharge and gage height.
 * Endpoint: https://waterservices.usgs.gov/nwis/iv/?format=json
 *           &stateCd=<st>&parameterCd=00060,00065&siteStatus=active  (keyless)
 * Emits one row per (site, parameter) time series with a real latest reading:
 *   value (UNIT taken verbatim from variable.unit.unitCode — "ft3/s" for
 *   discharge 00060, "ft" for gage height 00065), parameter code, site name,
 *   site code, reading timestamp, and the provisional-data disclaimer.
 *
 * SENTINEL TRAP: the magic no-data value is the STRING "-999999" — it is
 * rejected, never emitted as a measurement. Values are STRINGS throughout.
 * KEYING: a site appears once per parameter/method, so rows key on
 * site + parameterCd (+ method id), never site alone.
 * GEO (R2): coordinates come from sourceInfo.geoLocation.geogLocation
 * (NAD83) and are emitted only when the upstream supplied them.
 * Licence: US Geological Survey, public domain. Data are PROVISIONAL. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "usgs-nwis-instantaneous"
#define DISCLAIMER "USGS provisional data subject to revision."

static const char *STATES[] = { "co", "ca", "tx", "wa", NULL };

static int collect_state(const source_ctx *ctx, intel_sink *sink,
                         const char *st, int *fetched) {
  char url[256];
  snprintf(url, sizeof url,
    "https://waterservices.usgs.gov/nwis/iv/?format=json&stateCd=%s"
    "&parameterCd=00060,00065&siteStatus=active", st);
  cJSON *doc = feed_get_json(ctx->http, url, 45000);
  if (!doc) return 0;
  *fetched = 1;

  cJSON *val = cJSON_GetObjectItem(doc, "value");
  cJSON *ts = val ? cJSON_GetObjectItem(val, "timeSeries") : NULL;
  if (!cJSON_IsArray(ts)) { cJSON_Delete(doc); return 0; }

  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, ts) {
    cJSON *si = cJSON_GetObjectItem(s, "sourceInfo");
    cJSON *vr = cJSON_GetObjectItem(s, "variable");
    if (!si || !vr) continue;
    const char *site = jo_sv(si, "siteName");
    if (!site) continue;

    const char *site_code = NULL;
    cJSON *sc = cJSON_GetObjectItem(si, "siteCode");
    if (cJSON_IsArray(sc) && cJSON_GetArraySize(sc) > 0)
      site_code = jo_sv(cJSON_GetArrayItem(sc, 0), "value");

    const char *pcode = NULL;
    cJSON *vc = cJSON_GetObjectItem(vr, "variableCode");
    if (cJSON_IsArray(vc) && cJSON_GetArraySize(vc) > 0)
      pcode = jo_sv(cJSON_GetArrayItem(vc, 0), "value");
    if (!pcode) continue;

    cJSON *un = cJSON_GetObjectItem(vr, "unit");
    const char *unit = un ? jo_sv(un, "unitCode") : NULL;
    if (!unit) continue;                        /* no unit -> no measurement */
    const char *vname = jo_sv(vr, "variableName");

    /* values[0].value[] — last element is the latest reading */
    cJSON *vals = cJSON_GetObjectItem(s, "values");
    if (!cJSON_IsArray(vals) || cJSON_GetArraySize(vals) == 0) continue;
    cJSON *vlist = cJSON_GetObjectItem(cJSON_GetArrayItem(vals, 0), "value");
    if (!cJSON_IsArray(vlist)) continue;
    int nv = cJSON_GetArraySize(vlist);
    if (nv == 0) continue;
    cJSON *last = cJSON_GetArrayItem(vlist, nv - 1);
    const char *vs = jo_sv(last, "value");
    const char *when = jo_sv(last, "dateTime");
    if (!vs || !when) continue;
    /* no-data sentinel — never emit -999999 as a measurement */
    if (strcmp(vs, "-999999") == 0 || strcmp(vs, "-999999.0") == 0) continue;
    char *end = NULL;
    double v = strtod(vs, &end);
    if (end == vs) continue;

    int has_geo = 0; double lat = 0, lon = 0;
    cJSON *gl = cJSON_GetObjectItem(si, "geoLocation");
    cJSON *gg = gl ? cJSON_GetObjectItem(gl, "geogLocation") : NULL;
    if (gg) {
      cJSON *la = cJSON_GetObjectItem(gg, "latitude");
      cJSON *lo = cJSON_GetObjectItem(gg, "longitude");
      if (cJSON_IsNumber(la) && cJSON_IsNumber(lo)) {
        lat = la->valuedouble; lon = lo->valuedouble;
        if (lat != 0 || lon != 0) has_geo = 1;
      }
    }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "site_name", site);
    if (site_code) cJSON_AddStringToObject(p, "site_code", site_code);
    cJSON_AddStringToObject(p, "parameter_cd", pcode);
    if (vname) cJSON_AddStringToObject(p, "parameter_name", vname);
    cJSON_AddNumberToObject(p, "value", v);
    cJSON_AddStringToObject(p, "unit", unit);
    cJSON_AddStringToObject(p, "observed_at", when);
    cJSON_AddStringToObject(p, "state", st);
    cJSON_AddStringToObject(p, "disclaimer", DISCLAIMER);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[192], title[320];
    snprintf(key, sizeof key, "%s|%s", site_code ? site_code : site, pcode);
    snprintf(title, sizeof title, "%s: %s %.3f %s", site,
             strcmp(pcode, "00060") == 0 ? "discharge" :
             strcmp(pcode, "00065") == 0 ? "gage height" : pcode, v, unit);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.published_at    = when;
    row.lang            = "en";
    row.link            = "https://waterdata.usgs.gov/nwis";
    row.record_type     = "river-gauge";
    row.has_geo         = has_geo;
    row.lat = lat; row.lon = lon;
    row.properties_json = pj;
    row.tags_json       = "[\"environment\",\"hydrology\",\"usgs\",\"river\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
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

static const source_def eni_usgs_nwis_iv_def = {
  .id = SRC, .collector = "environment",
  .name = "USGS NWIS instantaneous river discharge and gage height",
  .update_interval_sec = 900, .run = run,
  .category = "environment", .type = "api",
  .url = "https://waterservices.usgs.gov/nwis/iv/",
  .description = "Real-time US river gauge network: discharge (ft3/s) and gage height (ft) with station coordinates. Provisional data.",
  .license = "US Geological Survey, public domain; provisional-data disclaimer reproduced per row.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_usgs_nwis_iv_def)
