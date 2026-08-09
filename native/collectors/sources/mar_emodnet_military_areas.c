/* collectors/sources/mar_emodnet_military_areas.c
 * EMODnet Human Activities — declared maritime military areas in European
 * waters: exercise areas, firing ranges and restricted defence zones.
 *
 * Endpoint: https://ows.emodnet-humanactivities.eu/wfs?service=WFS
 *   &version=2.0.0&request=GetFeature&outputFormat=application/json
 *   &typeName=emodnet:militaryareaspoly&count=1000
 * Emits (all fetched): country (+country_2/3), status, type_1/2/3, resource
 *   (the national source document), areakm, coast_dist and the declared area
 *   as a MultiPolygon in geometry_geojson.
 * Keyless WFS. Licence: EMODnet Human Activities (EU), compiled from national
 *   maritime spatial plans; free reuse with attribution.
 *
 * *** parse_notes trap — "Geometry is a MultiPolygon — emit as
 * geometry_geojson, do NOT collapse to a centroid point (a 461 km² firing
 * range reduced to one dot is a misleading map)." ***
 * So the full polygon is emitted and has_geo/lat/lon are deliberately left at
 * 0: no representative point is manufactured for these areas.
 * parse_notes — "A companion point layer emodnet:militaryareas exists for
 * sites published as points only"; it is not merged in here.
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
                "&version=2.0.0&request=GetFeature&outputFormat=application/json" \
                "&typeName=emodnet:militaryareaspoly&count=1000"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, WFS_URL, 120000);
  if (!doc) {
    fprintf(stderr, "[emodnet-military-areas] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    const char *country = jo_sv(pr, "country");
    const char *t1 = jo_sv(pr, "type_1");
    if (!country && !t1) continue;

    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    char *gj = (g && !cJSON_IsNull(g)) ? cJSON_PrintUnformatted(g) : NULL;
    if (!gj) continue;                   /* no fetched geometry -> no row */

    const char *fid = jo_sv(f, "id");
    char key[200];
    snprintf(key, sizeof key, "%s", fid ? fid : (t1 ? t1 : country));

    cJSON *p = cJSON_CreateObject();
    jo_copy_str(p, pr, "country");
    jo_copy_str(p, pr, "country_2");
    jo_copy_str(p, pr, "country_3");
    jo_copy_str(p, pr, "status");
    jo_copy_str(p, pr, "type_1");
    jo_copy_str(p, pr, "type_2");
    jo_copy_str(p, pr, "type_3");
    jo_copy_str(p, pr, "resource");
    jo_copy_num(p, pr, "areakm");
    jo_copy_num(p, pr, "coast_dist");
    cJSON_AddStringToObject(p, "geo_precision", "declared-area-polygon");
    cJSON_AddStringToObject(p, "geo_note",
      "declared area emitted as a polygon; deliberately NOT reduced to a centroid point");
    cJSON_AddStringToObject(p, "source", "EMODnet Human Activities");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const cJSON *ak = cJSON_GetObjectItem(pr, "areakm");
    const char *st = jo_sv(pr, "status");
    char title[240], summary[220];
    snprintf(title, sizeof title, "%s%s%s", t1 ? t1 : "Military area",
             country ? " — " : "", country ? country : "");
    if (cJSON_IsNumber(ak))
      snprintf(summary, sizeof summary, "%.1f km2%s%s", ak->valuedouble,
               st ? " · " : "", st ? st : "");
    else
      snprintf(summary, sizeof summary, "%s", st ? st : "declared military area");

    intel_item it = {0};
    it.remote_key       = key;
    it.title            = title;
    it.summary          = summary;
    it.link             = "https://emodnet.ec.europa.eu/en/human-activities";
    it.record_type      = "military-area";
    it.geometry_geojson = gj;
    /* has_geo/lat/lon intentionally 0: see the parse_notes trap above */
    it.properties_json  = pj ? pj : "{}";
    it.tags_json        = "[\"maritime\",\"military\",\"restricted-area\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[emodnet-military-areas] emitted %d\n", n);
  return 0;
}

static const source_def mar_emodnet_military_areas_def = {
  .id = "emodnet-military-areas", .collector = "maritime",
  .name = "EMODnet Human Activities — declared maritime military areas",
  .update_interval_sec = 2592000, .run = run,
  .category = "transport", .type = "dataset", .url = WFS_URL,
  .description = "Officially declared military and defence areas at sea in European waters — exercise areas, firing ranges and restricted defence zones as polygons, with the national source document for each.",
  .license = "EMODnet Human Activities (EU), free reuse with attribution",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_emodnet_military_areas_def)
