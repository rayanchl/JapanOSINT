/* collectors/sources/mar_emodnet_fishing_effort.c
 * EMODnet Human Activities — EU fishing effort (VMS-derived fishing days)
 * aggregated to 0.5° C-squares, split by gear technology and vessel length.
 *
 * Endpoint: https://ows.emodnet-humanactivities.eu/wfs?service=WFS
 *   &version=2.0.0&request=GetFeature&outputFormat=application/json
 *   &typeName=emodnet:csquareeffort&count=5000
 *   (parse_notes: "Very large layer: filter by year and fishing_tech with
 *    CQL_FILTER and page with &count=/&startIndex=" — one page per run.)
 * Emits (all fetched): cscode, eu, fishing_tech + fishing_tech_desc,
 *   vessel_length + vessel_length_desc, year, total_fishing_days and the
 *   C-square cell as a MultiPolygon in geometry_geojson.
 * Keyless WFS. Licence: EMODnet Human Activities (EU), derived from ICES/STECF
 *   VMS data; free reuse with attribution.
 *
 * *** parse_notes trap — "Geometry is a 0.5°×0.5° C-square MultiPolygon, i.e.
 * deliberately coarse — emit the polygon and set geo_precision 'grid-cell'; a
 * centroid point would imply a precision the data does not have." ***
 * So the polygon is emitted and has_geo/lat/lon are deliberately left at 0.
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
                "&version=2.0.0&request=GetFeature&outputFormat=application/json" \
                "&typeName=emodnet:csquareeffort&count=5000"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, WFS_URL, 120000);
  if (!doc) {
    fprintf(stderr, "[emodnet-fishing-effort] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    const char *cs = jo_sv(pr, "cscode");
    const cJSON *yr = cJSON_GetObjectItem(pr, "year");
    if (!cs || !cJSON_IsNumber(yr)) continue;

    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    char *gj = (g && !cJSON_IsNull(g)) ? cJSON_PrintUnformatted(g) : NULL;
    if (!gj) continue;

    const char *tech = jo_sv(pr, "fishing_tech");
    const char *vl = jo_sv(pr, "vessel_length");
    char key[160];
    snprintf(key, sizeof key, "%s|%d|%s|%s", cs, (int)yr->valuedouble,
             tech ? tech : "?", vl ? vl : "?");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "cscode", cs);
    jo_copy_str(p, pr, "eu");
    jo_copy_str(p, pr, "fishing_tech");
    jo_copy_str(p, pr, "fishing_tech_desc");
    jo_copy_str(p, pr, "vessel_length");
    jo_copy_str(p, pr, "vessel_length_desc");
    jo_copy_num(p, pr, "year");
    jo_copy_num(p, pr, "total_fishing_days");
    cJSON_AddStringToObject(p, "geo_precision", "grid-cell");
    cJSON_AddStringToObject(p, "geo_note",
      "0.5 degree C-square cell emitted as a polygon; no centroid point is implied");
    cJSON_AddStringToObject(p, "source",
      "EMODnet Human Activities (ICES/STECF VMS)");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const cJSON *days = cJSON_GetObjectItem(pr, "total_fishing_days");
    char title[240], summary[240];
    snprintf(title, sizeof title, "Fishing effort %s %d — %s",
             cs, (int)yr->valuedouble, tech ? tech : "all gears");
    if (cJSON_IsNumber(days))
      snprintf(summary, sizeof summary, "%.0f fishing days%s%s",
               days->valuedouble, vl ? " · " : "", vl ? vl : "");
    else
      snprintf(summary, sizeof summary, "C-square fishing effort record");

    intel_item it = {0};
    it.remote_key       = key;
    it.title            = title;
    it.summary          = summary;
    it.link             = "https://emodnet.ec.europa.eu/en/human-activities";
    it.record_type      = "fishing-effort-cell";
    it.geometry_geojson = gj;
    /* has_geo/lat/lon intentionally 0: see the parse_notes trap above */
    it.properties_json  = pj ? pj : "{}";
    it.tags_json        = "[\"maritime\",\"fishing\",\"vms\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
    free(gj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[emodnet-fishing-effort] emitted %d\n", n);
  return 0;
}

static const source_def mar_emodnet_fishing_effort_def = {
  .id = "emodnet-fishing-effort", .collector = "maritime",
  .name = "EMODnet Human Activities — EU fishing effort by C-square",
  .update_interval_sec = 2592000, .run = run,
  .category = "industry", .type = "dataset", .url = WFS_URL,
  .description = "VMS-derived EU fishing effort in fishing days aggregated to 0.5 degree C-squares, split by gear technology and vessel length class — where EU fleets actually fish.",
  .license = "EMODnet Human Activities (EU) / ICES-STECF, free reuse with attribution",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_emodnet_fishing_effort_def)
