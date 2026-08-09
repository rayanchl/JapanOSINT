/* collectors/sources/mar_emodnet_port_vessel_traffic.c
 * EMODnet Human Activities — annual vessel calls by port, ship type and
 * tonnage band (Eurostat-derived).
 *
 * Endpoint: https://ows.emodnet-humanactivities.eu/wfs?service=WFS
 *   &version=2.0.0&request=GetFeature&outputFormat=application/json
 *   &typeName=emodnet:portvessels&count=5000
 *   (the layer has >1.7M rows; &count= pages it — see parse_notes)
 * Emits (all fetched): port, port_id, portcode, country, year, vesseltype,
 *   tonnage band + tonnagesize, nofvessels, gross_tonnage__gt_thousand, and
 *   the PORT point the statistic describes.
 * Keyless WFS. Licence: EMODnet Human Activities (EU), derived from Eurostat
 *   maritime transport statistics; free reuse with attribution.
 *
 * *** parse_notes EXPLICIT WARNING — "the geometry on every feature is the
 * PORT point, repeated for each statistic row — it is the location of the port
 * the statistic describes, and must never be read as a vessel position. Many
 * rows share identical coordinates by design; label the record type clearly
 * (port statistic, not vessel)." ***
 * Accordingly: record_type is "port-vessel-statistic" (never a vessel row),
 * geo_precision is "port-reference-point", and properties carry an explicit
 * geo_note saying the point is the port, not a ship.
 *
 * parse_notes — "'tonnage':'TOTAL' rows are the all-bands aggregate — filter
 * to avoid double counting": the band is carried in properties and an
 * is_aggregate flag marks the TOTAL rows so a consumer can exclude them.
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define WFS_URL "https://ows.emodnet-humanactivities.eu/wfs?service=WFS" \
                "&version=2.0.0&request=GetFeature" \
                "&outputFormat=application/json&typeName=emodnet:portvessels" \
                "&count=5000"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, WFS_URL, 90000);
  if (!doc) {
    fprintf(stderr, "[emodnet-port-vessel-traffic] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    const char *pid = jo_sv(pr, "port_id");
    const char *port = jo_sv(pr, "port");
    const char *vtype = jo_sv(pr, "vesseltype");
    const char *band = jo_sv(pr, "tonnage");
    const cJSON *yr = cJSON_GetObjectItem(pr, "year");
    if (!pid || !cJSON_IsNumber(yr)) continue;

    int geo = 0; double lat = 0, lon = 0;
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    if (g && !cJSON_IsNull(g)) {
      cJSON *co = cJSON_GetObjectItem(g, "coordinates");
      cJSON *x = cJSON_GetArrayItem(co, 0), *y = cJSON_GetArrayItem(co, 1);
      if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
        geo = 1; lon = x->valuedouble; lat = y->valuedouble;
      }
    }

    char key[160];
    snprintf(key, sizeof key, "%s|%d|%s|%s", pid, (int)yr->valuedouble,
             vtype ? vtype : "?", band ? band : "?");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "port_id", pid);
    if (port)  cJSON_AddStringToObject(p, "port", port);
    if (vtype) cJSON_AddStringToObject(p, "vesseltype", vtype);
    if (band)  cJSON_AddStringToObject(p, "tonnage_band", band);
    const char *v;
    if ((v = jo_sv(pr, "portcode")))    cJSON_AddStringToObject(p, "portcode", v);
    if ((v = jo_sv(pr, "country")))     cJSON_AddStringToObject(p, "country", v);
    if ((v = jo_sv(pr, "tonnagesize"))) cJSON_AddStringToObject(p, "tonnagesize", v);
    jo_copy_num(p, pr, "year");
    jo_copy_num(p, pr, "nofvessels");
    jo_copy_num(p, pr, "gross_tonnage__gt_thousand");
    cJSON_AddBoolToObject(p, "is_aggregate",
                          band && strcmp(band, "TOTAL") == 0);
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "port-reference-point");
      cJSON_AddStringToObject(p, "geo_note",
        "this is the PORT the statistic describes — NOT a vessel position");
    }
    cJSON_AddStringToObject(p, "source",
      "EMODnet Human Activities (Eurostat maritime transport statistics)");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const cJSON *nv = cJSON_GetObjectItem(pr, "nofvessels");
    char title[240], summary[240];
    snprintf(title, sizeof title, "%s %d — %s (%s)",
             port ? port : pid, (int)yr->valuedouble,
             vtype ? vtype : "all types", band ? band : "all bands");
    if (cJSON_IsNumber(nv))
      snprintf(summary, sizeof summary, "%lld vessel calls (port statistic)",
               (long long)nv->valuedouble);
    else
      snprintf(summary, sizeof summary, "port vessel-call statistic");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://emodnet.ec.europa.eu/en/human-activities";
    it.record_type     = "port-vessel-statistic";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"port\",\"statistics\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[emodnet-port-vessel-traffic] emitted %d\n", n);
  return 0;
}

static const source_def mar_emodnet_port_vessel_traffic_def = {
  .id = "emodnet-port-vessel-traffic", .collector = "maritime",
  .name = "EMODnet Human Activities — vessel calls by port, type and tonnage",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset", .url = WFS_URL,
  .description = "Annual Eurostat-derived vessel-call counts and gross tonnage per European port, broken down by ship type and tonnage band. Each row is a PORT statistic plotted at the port point — not a vessel position.",
  .license = "EMODnet Human Activities (EU) / Eurostat, free reuse with attribution",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_emodnet_port_vessel_traffic_def)
