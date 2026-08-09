/* collectors/sources/mar_rws_waterinfo_levels.c
 * Rijkswaterstaat Waterinfo — live water level at every RWS measuring point on
 * Dutch coastal waters, estuaries and inland waterways (surge, port access and
 * inland-shipping draft restrictions for the Rotterdam/Antwerp complex).
 *
 * Endpoint: https://waterinfo.rws.nl/api/point/latestmeasurement
 *           ?parameterid=waterhoogte
 * Emits (all fetched): name, locationCode, latestValue, dateTime, unitCode,
 *   qualityCode, possiblyFaulty, measurementLabel, locationLabel.
 * Keyless (undocumented internal API of the public Waterinfo site).
 * Licence: Rijkswaterstaat Waterinfo, Dutch public-sector open data.
 *
 * *** parse_notes GEO CAVEAT — "the reason has_geo is 0 here:
 * geometry.coordinates are NOT lat/lon. The response's crs is EPSG:25831 (UTM
 * zone 31N) and the values are eastings/northings in metres (e.g.
 * [656857.08, 5733842.52]). Feeding those to a map as degrees puts every
 * station in the Gulf of Guinea. Either implement a UTM31N->WGS84 conversion
 * before setting has_geo, or emit the station rows without geometry." ***
 * This collector takes the second option: NO row sets has_geo/lat/lon, and no
 * geometry_geojson is emitted. The raw easting/northing are carried in
 * properties, explicitly labelled with their CRS, so nothing fetched is lost
 * and nothing is mis-plotted (R2).
 *
 * parse_notes — "latestValue is in cm relative to NAP (unitCode/qualityCode
 * state this); possiblyFaulty=true rows should be flagged, not dropped
 * silently." Both honoured.
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

#define RWS_URL "https://waterinfo.rws.nl/api/point/latestmeasurement" \
                "?parameterid=waterhoogte"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, RWS_URL, 60000);
  if (!doc) {
    fprintf(stderr, "[rws-waterinfo-levels] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    const char *code = jo_sv(pr, "locationCode");
    const char *name = jo_sv(pr, "name");
    if (!code && !name) continue;
    const char *key = code ? code : name;

    /* first measurement entry carries the latest value for this point */
    const cJSON *ms = cJSON_GetObjectItem(pr, "measurements");
    const cJSON *m0 = cJSON_IsArray(ms) ? cJSON_GetArrayItem(ms, 0) : NULL;
    const cJSON *lv = m0 ? cJSON_GetObjectItem(m0, "latestValue") : NULL;
    const char *when = m0 ? jo_sv(m0, "dateTime") : NULL;
    const char *unit = m0 ? jo_sv(m0, "unitCode") : NULL;
    const char *qual = m0 ? jo_sv(m0, "qualityCode") : NULL;
    const cJSON *faulty = m0 ? cJSON_GetObjectItem(m0, "possiblyFaulty") : NULL;

    cJSON *p = cJSON_CreateObject();
    if (code) cJSON_AddStringToObject(p, "locationCode", code);
    if (name) cJSON_AddStringToObject(p, "name", name);
    if (cJSON_IsNumber(lv))
      cJSON_AddNumberToObject(p, "latestValue", lv->valuedouble);
    if (unit) cJSON_AddStringToObject(p, "unitCode", unit);
    if (qual) cJSON_AddStringToObject(p, "qualityCode", qual);
    if (when) cJSON_AddStringToObject(p, "dateTime", when);
    if (cJSON_IsBool(faulty))
      cJSON_AddBoolToObject(p, "possiblyFaulty", cJSON_IsTrue(faulty));
    const char *v;
    if (m0 && (v = jo_sv(m0, "measurementLabel")))
      cJSON_AddStringToObject(p, "measurementLabel", v);
    if ((v = jo_sv(pr, "locationLabel")))
      cJSON_AddStringToObject(p, "locationLabel", v);

    /* the response geometry is EPSG:25831 metres, NOT degrees — carried as
     * raw projected coordinates only, never as lat/lon (see the header) */
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    cJSON *co = g ? cJSON_GetObjectItem(g, "coordinates") : NULL;
    cJSON *ex = cJSON_GetArrayItem(co, 0), *ny = cJSON_GetArrayItem(co, 1);
    if (cJSON_IsNumber(ex) && cJSON_IsNumber(ny)) {
      cJSON_AddNumberToObject(p, "easting_epsg25831", ex->valuedouble);
      cJSON_AddNumberToObject(p, "northing_epsg25831", ny->valuedouble);
      cJSON_AddStringToObject(p, "geo_note",
        "upstream coordinates are EPSG:25831 (UTM 31N) metres, not degrees; no lat/lon is emitted because no reprojection is performed");
    }
    cJSON_AddStringToObject(p, "source", "Rijkswaterstaat Waterinfo");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[220], summary[240];
    snprintf(title, sizeof title, "%s", name ? name : key);
    if (cJSON_IsNumber(lv))
      snprintf(summary, sizeof summary, "%.1f %s %s%s%s%s",
               lv->valuedouble, unit ? unit : "cm", qual ? qual : "",
               when ? " at " : "", when ? when : "",
               (cJSON_IsBool(faulty) && cJSON_IsTrue(faulty))
                 ? " · possibly faulty" : "");
    else
      snprintf(summary, sizeof summary, "Rijkswaterstaat measuring point");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://waterinfo.rws.nl/";
    it.lang            = "nl";
    it.published_at    = when;
    it.record_type     = "water-level-gauge";
    /* has_geo deliberately 0 — see the GEO CAVEAT in the header */
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"water-level\",\"netherlands\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[rws-waterinfo-levels] emitted %d\n", n);
  return 0;
}

static const source_def mar_rws_waterinfo_levels_def = {
  .id = "rws-waterinfo-levels", .collector = "maritime",
  .name = "Rijkswaterstaat Waterinfo — Dutch water level measurements",
  .update_interval_sec = 1800, .run = run,
  .category = "transport", .type = "api", .url = RWS_URL,
  .description = "Live water level at every Rijkswaterstaat measuring point on Dutch coastal waters, estuaries and inland waterways. Emitted without coordinates: upstream geometry is EPSG:25831 metres and is not reprojected.",
  .license = "Rijkswaterstaat Waterinfo, Dutch public-sector open data",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_rws_waterinfo_levels_def)
