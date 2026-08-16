/* collectors/sources/mar_digitraffic_port_locations.c
 * Fintraffic Digitraffic — SafeSeaNet UN/LOCODE port reference points.
 *
 * Endpoint: https://meri.digitraffic.fi/api/port-call/v1/ports
 * Emits (all fetched): locode, locationName, country and the published port
 *   point from ssnLocations.features[].geometry.coordinates = [lon, lat].
 * Keyless. Licence: Fintraffic Digitraffic, CC BY 4.0.
 *
 * parse_notes — "this is a PORT reference point (correct for the entity being
 * described — a port)". record_type is "port-location": it is a place, never a
 * vessel position.
 * parse_notes trap — "Many features have \"geometry\": null (e.g. DEHEN
 * Heilbronn) — those rows must be emitted with has_geo=0, never backfilled
 * from the country." Handled below (R2).
 * parse_notes trap — Accept-Encoding: gzip is required; the platform HTTP
 * layer already sends it, so no custom header is added.
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

#define PORTS_URL "https://meri.digitraffic.fi/api/port-call/v1/ports"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, PORTS_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[digitraffic-port-locations] fetch/parse failed\n");
    return -1;
  }
  cJSON *ssn = cJSON_GetObjectItem(doc, "ssnLocations");
  cJSON *feats = ssn ? cJSON_GetObjectItem(ssn, "features")
                     : cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    if (!cJSON_IsObject(f)) continue;
    cJSON *props = cJSON_GetObjectItem(f, "properties");
    const char *locode = jo_sv(f, "locode");
    if (!locode && props) locode = jo_sv(props, "locode");
    if (!locode) continue;
    const char *place = props ? jo_sv(props, "locationName") : NULL;
    const char *country = props ? jo_sv(props, "country") : NULL;

    /* geometry may legitimately be null — emit the row without a coordinate */
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
    cJSON_AddStringToObject(p, "locode", locode);
    if (place)   cJSON_AddStringToObject(p, "locationName", place);
    if (country) cJSON_AddStringToObject(p, "country", country);
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "port-reference-point");
    }
    cJSON_AddStringToObject(p, "source", "Fintraffic Digitraffic / SafeSeaNet");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[200], summary[160];
    snprintf(title, sizeof title, "%s (%s)", place ? place : locode, locode);
    snprintf(summary, sizeof summary, "SafeSeaNet port location%s%s",
             country ? " · " : "", country ? country : "");

    intel_item it = {0};
    it.remote_key      = locode;
    it.title           = title;
    it.summary         = summary;
    it.link            = PORTS_URL;
    it.record_type     = "port-location";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"port\",\"locode\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[digitraffic-port-locations] emitted %d\n", n);
  return 0;
}

static const source_def mar_digitraffic_port_locations_def = {
  .id = "digitraffic-port-locations", .collector = "maritime",
  .name = "Fintraffic Digitraffic — SafeSeaNet port locations (LOCODE)",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "api", .url = PORTS_URL,
  .description = "UN/LOCODE port and terminal reference points used by SafeSeaNet: LOCODE, place name, country and the published point location of each port.",
  .license = "Fintraffic Digitraffic, CC BY 4.0",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_digitraffic_port_locations_def)
