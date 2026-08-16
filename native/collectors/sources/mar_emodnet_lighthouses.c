/* collectors/sources/mar_emodnet_lighthouses.c
 * EMODnet Human Activities — European lighthouses (4,202 structures with ARLHS
 * identifiers). Coastal navigation infrastructure and a stable set of visual
 * landmarks for imagery work.
 *
 * Endpoint: https://ows.emodnet-humanactivities.eu/wfs?service=WFS
 *   &version=2.0.0&request=GetFeature&outputFormat=application/json
 *   &typeName=emodnet:lighthouses
 * Emits (all fetched): arlhs_number, name, country, status (existence),
 *   distance_to_shore_m, position_coastline, comments and the structure
 *   position from features[].geometry.coordinates.
 * Keyless WFS. Licence: EMODnet Human Activities (EU), free reuse with
 *   attribution.
 *
 * parse_notes — "Coordinates are visibly rounded to arc-minutes for some
 * entries (e.g. 43.71666667) — that is the upstream precision, do not present
 * sub-metre accuracy." geo_precision is therefore "as-published" and no
 * coordinate is reformatted or refined.
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
                "&outputFormat=application/json&typeName=emodnet:lighthouses"

static const char *lsv(const cJSON *o, const char *k) {
  const char *s = jo_sv(o, k);
  if (!s) return NULL;
  if (strcmp(s, "n.a.") == 0 || strcmp(s, "n/a") == 0) return NULL;
  return s;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, WFS_URL, 90000);
  if (!doc) {
    fprintf(stderr, "[emodnet-lighthouses] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    const char *arlhs = lsv(pr, "arlhs_number");
    const char *name = lsv(pr, "name");
    if (!arlhs && !name) continue;
    const char *key = arlhs ? arlhs : name;

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
    if (arlhs) cJSON_AddStringToObject(p, "arlhs_number", arlhs);
    if (name)  cJSON_AddStringToObject(p, "name", name);
    const char *v;
    if ((v = lsv(pr, "country")))            cJSON_AddStringToObject(p, "country", v);
    if ((v = lsv(pr, "status")))             cJSON_AddStringToObject(p, "status", v);
    if ((v = lsv(pr, "distance_to_shore_m"))) cJSON_AddStringToObject(p, "distance_to_shore_m", v);
    if ((v = lsv(pr, "position_coastline"))) cJSON_AddStringToObject(p, "position_coastline", v);
    if ((v = lsv(pr, "comments")))           cJSON_AddStringToObject(p, "comments", v);
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "as-published");
      cJSON_AddStringToObject(p, "geo_note",
        "some entries are rounded to arc-minutes upstream; not sub-metre accurate");
    }
    cJSON_AddStringToObject(p, "source", "EMODnet Human Activities");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *cc = lsv(pr, "country");
    const char *st = lsv(pr, "status");
    char title[220], summary[240];
    snprintf(title, sizeof title, "Lighthouse %s%s%s", name ? name : key,
             arlhs && name ? " · ARLHS " : "", arlhs && name ? arlhs : "");
    snprintf(summary, sizeof summary, "%s%s%s", cc ? cc : "",
             st ? " · " : "", st ? st : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.link            = "https://emodnet.ec.europa.eu/en/human-activities";
    it.record_type     = "lighthouse";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"navigation-aid\",\"lighthouse\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[emodnet-lighthouses] emitted %d\n", n);
  return 0;
}

static const source_def mar_emodnet_lighthouses_def = {
  .id = "emodnet-lighthouses", .collector = "maritime",
  .name = "EMODnet Human Activities — European lighthouses",
  .update_interval_sec = 2592000, .run = run,
  .category = "transport", .type = "dataset", .url = WFS_URL,
  .description = "4,202 European lighthouses with ARLHS identifiers, existence status and distance offshore — coastal navigation infrastructure and stable visual landmarks for imagery work.",
  .license = "EMODnet Human Activities (EU), free reuse with attribution",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_emodnet_lighthouses_def)
