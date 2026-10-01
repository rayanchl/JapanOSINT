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
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
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

    /* siteCode and variableCode are LISTS in WaterML-JSON (a site can be
     * registered in more than one network). One element is the norm — all 724
     * CO series carry exactly one — but the extra codes are what let a record
     * join to another agency's data, so every one of them is kept alongside
     * the display pick. */
    const char *site_code = NULL;
    cJSON *sc = cJSON_GetObjectItem(si, "siteCode"), *sc_all = NULL;
    if (cJSON_IsArray(sc) && cJSON_GetArraySize(sc) > 0) {
      sc_all = cJSON_CreateArray();
      cJSON *e;
      cJSON_ArrayForEach(e, sc) {
        const char *v = jo_sv(e, "value");
        if (!v) continue;
        if (!site_code) site_code = v;
        cJSON_AddItemToArray(sc_all, cJSON_Duplicate(e, 1));
      }
    }

    const char *pcode = NULL;
    cJSON *vc = cJSON_GetObjectItem(vr, "variableCode"), *vc_all = NULL;
    if (cJSON_IsArray(vc) && cJSON_GetArraySize(vc) > 0) {
      vc_all = cJSON_CreateArray();
      cJSON *e;
      cJSON_ArrayForEach(e, vc) {
        const char *v = jo_sv(e, "value");
        if (!v) continue;
        if (!pcode) pcode = v;
        cJSON_AddItemToArray(vc_all, cJSON_Duplicate(e, 1));
      }
    }
    if (!pcode) { cJSON_Delete(sc_all); cJSON_Delete(vc_all); continue; }

    cJSON *un = cJSON_GetObjectItem(vr, "unit");
    const char *unit = un ? jo_sv(un, "unitCode") : NULL;
    if (!unit) { cJSON_Delete(sc_all); cJSON_Delete(vc_all); continue; }
    const char *vname = jo_sv(vr, "variableName");

    /* values[] is a list of VALUE BLOCKS, one per measurement method, and only
     * block 0 was ever read. A gauge with a backup sensor publishes two blocks
     * — CAMP CREEK AT GARDEN OF THE GODS reports 00065 from both its primary
     * (methodID 280009) and its backup gage-height sensor (211192) — and the
     * second reading was silently discarded. Worse, the file's own header
     * already said rows should key on "site + parameterCd (+ method id)" while
     * the key never carried the method, so even reading both blocks would have
     * collapsed them onto one remote_key. Both are fixed here: every block is
     * emitted, and the method id is part of the key. */
    cJSON *vals = cJSON_GetObjectItem(s, "values");
    if (!cJSON_IsArray(vals) || cJSON_GetArraySize(vals) == 0) {
      cJSON_Delete(sc_all); cJSON_Delete(vc_all); continue;
    }

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

    cJSON *block;
    cJSON_ArrayForEach(block, vals) {
      cJSON *vlist = cJSON_GetObjectItem(block, "value");
      if (!cJSON_IsArray(vlist)) continue;
      int nv = cJSON_GetArraySize(vlist);
      if (nv == 0) continue;
      /* The iv service returns the reading(s) in the requested window, newest
       * last; with no window it returns one. Take the newest. */
      cJSON *last = cJSON_GetArrayItem(vlist, nv - 1);
      const char *vs = jo_sv(last, "value");
      const char *when = jo_sv(last, "dateTime");
      if (!vs || !when) continue;
      /* no-data sentinel — never emit -999999 as a measurement */
      if (strcmp(vs, "-999999") == 0 || strcmp(vs, "-999999.0") == 0) continue;
      char *end = NULL;
      double v = strtod(vs, &end);
      if (end == vs) continue;

      /* method: [{methodID, methodDescription}] — the block's identity */
      const char *method_id = NULL, *method_desc = NULL;
      cJSON *meth = cJSON_GetObjectItem(block, "method");
      char midbuf[32] = "";
      /* A value block carries exactly one method — the block IS the method,
       * which is why a two-method gauge answers with two blocks (both of which
       * are now emitted) rather than one block with two methods. */
      if (cJSON_IsArray(meth) && cJSON_GetArraySize(meth) > 0) {
        cJSON *m0 = cJSON_GetArrayItem(meth, 0);  /* exhaustive-ok: one method per value block; extra methods arrive as extra blocks, and every block is emitted */
        cJSON *mid = cJSON_GetObjectItem(m0, "methodID");
        if (cJSON_IsNumber(mid)) {
          snprintf(midbuf, sizeof midbuf, "%lld", (long long)mid->valuedouble);
          method_id = midbuf;
        } else {
          method_id = jo_sv(m0, "methodID");
        }
        method_desc = jo_sv(m0, "methodDescription");
      }

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "site_name", site);
      if (site_code) cJSON_AddStringToObject(p, "site_code", site_code);
      if (sc_all && cJSON_GetArraySize(sc_all) > 1)
        cJSON_AddItemToObject(p, "site_codes", cJSON_Duplicate(sc_all, 1));
      cJSON_AddStringToObject(p, "parameter_cd", pcode);
      if (vc_all && cJSON_GetArraySize(vc_all) > 1)
        cJSON_AddItemToObject(p, "variable_codes", cJSON_Duplicate(vc_all, 1));
      if (vname) cJSON_AddStringToObject(p, "parameter_name", vname);
      cJSON_AddNumberToObject(p, "value", v);
      cJSON_AddStringToObject(p, "unit", unit);
      cJSON_AddStringToObject(p, "observed_at", when);
      if (method_id) cJSON_AddStringToObject(p, "method_id", method_id);
      if (method_desc) cJSON_AddStringToObject(p, "method_description", method_desc);
      cJSON *quals = cJSON_GetObjectItem(last, "qualifiers");
      if (cJSON_IsArray(quals) && cJSON_GetArraySize(quals) > 0)
        cJSON_AddItemToObject(p, "qualifiers", cJSON_Duplicate(quals, 1));
      cJSON_AddStringToObject(p, "state", st);
      cJSON_AddStringToObject(p, "disclaimer", DISCLAIMER);
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      char key[224], title[384];
      snprintf(key, sizeof key, "%s|%s%s%s", site_code ? site_code : site,
               pcode, method_id ? "|" : "", method_id ? method_id : "");
      snprintf(title, sizeof title, "%s: %s %.3f %s%s%s", site,
               strcmp(pcode, "00060") == 0 ? "discharge" :
               strcmp(pcode, "00065") == 0 ? "gage height" : pcode, v, unit,
               (method_desc && method_desc[0]) ? " " : "",
               (method_desc && method_desc[0]) ? method_desc : "");

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
    cJSON_Delete(sc_all);
    cJSON_Delete(vc_all);
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
