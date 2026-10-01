/* collectors/sources/mar_emodnet_port_locations.c
 * EMODnet Human Activities — European port locations (946 ports).
 *
 * Endpoint: https://ows.emodnet-humanactivities.eu/wfs?service=WFS
 *   &version=2.0.0&request=GetFeature&outputFormat=application/json
 *   &typeName=emodnet:portlocations
 * Emits (all fetched): port_id, port, portname, country, source, traffic flag
 *   and the port reference point from features[].geometry.coordinates.
 * Keyless WFS. Licence: EMODnet Human Activities (EU / DG MARE), free reuse
 *   with attribution (CC-BY style).
 *
 * parse_notes — "Coordinate is features[].geometry.coordinates = [lon, lat] in
 * EPSG:4326 (crs is stated in the response) ... It is the port reference point
 * — correct for a port entity." record_type is "port-location": a place, never
 * a vessel position.
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

#define WFS_URL "https://ows.emodnet-humanactivities.eu/wfs?service=WFS" \
                "&version=2.0.0&request=GetFeature" \
                "&outputFormat=application/json&typeName=emodnet:portlocations"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, WFS_URL, 60000);
  if (!doc) {
    fprintf(stderr, "[emodnet-port-locations] fetch/parse failed\n");
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
    const char *pname = jo_sv(pr, "portname");
    if (!pid) continue;

    int geo = 0; double lat = 0, lon = 0;
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    if (g && !cJSON_IsNull(g)) {
      cJSON *co = cJSON_GetObjectItem(g, "coordinates");
      cJSON *x = cJSON_GetArrayItem(co, 0), *y = cJSON_GetArrayItem(co, 1);
      if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
        geo = 1; lon = x->valuedouble; lat = y->valuedouble;
      }
    }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "port_id", pid);
    if (port)  cJSON_AddStringToObject(p, "port", port);
    if (pname) cJSON_AddStringToObject(p, "portname", pname);
    const char *v;
    if ((v = jo_sv(pr, "country"))) cJSON_AddStringToObject(p, "country", v);
    if ((v = jo_sv(pr, "source")))  cJSON_AddStringToObject(p, "source_dataset", v);
    if ((v = jo_sv(pr, "traffic"))) cJSON_AddStringToObject(p, "traffic", v);
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "port-reference-point");
    }
    cJSON_AddStringToObject(p, "source", "EMODnet Human Activities");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *cc = jo_sv(pr, "country");
    char title[220], summary[200];
    snprintf(title, sizeof title, "%s (%s)",
             port ? port : (pname ? pname : pid), pid);
    snprintf(summary, sizeof summary, "European port%s%s",
             cc ? " · " : "", cc ? cc : "");

    intel_item it = {0};
    it.remote_key      = pid;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://emodnet.ec.europa.eu/en/human-activities";
    it.record_type     = "port-location";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"port\",\"emodnet\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[emodnet-port-locations] emitted %d\n", n);
  return 0;
}

static const source_def mar_emodnet_port_locations_def = {
  .id = "emodnet-port-locations", .collector = "maritime",
  .name = "EMODnet Human Activities — European port locations",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset", .url = WFS_URL,
  .description = "946 European ports with harmonised port IDs, country, coordinates and provenance — the join key for the EMODnet port traffic, waste and vessel-call statistics layers.",
  .license = "EMODnet Human Activities (EU / DG MARE), free reuse with attribution",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_emodnet_port_locations_def)
