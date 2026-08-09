/* collectors/sources/mar_emodnet_ww_shipwrecks.c
 * EMODnet Human Activities — world-wide shipwreck database (61,023 charted
 * wrecks). Navigation hazards and a record of where ships have been lost.
 *
 * Endpoint: https://ows.emodnet-humanactivities.eu/wfs?service=WFS
 *   &version=2.0.0&request=GetFeature&outputFormat=application/json
 *   &typeName=emodnet:wwshipwrecks&count=5000
 *   (parse_notes: "Large layer: always page with &count= and &startIndex=" —
 *    one 5,000-feature page is taken per run.)
 * Emits (all fetched): wreck_id, name, type, flag, wreck_cate (hazard
 *   category), depth / water_dept, length, width, tonnage, cargo, date_sunk,
 *   original_d / last_detec and the charted position from
 *   features[].geometry.coordinates.
 * Keyless WFS. Licence: EMODnet Human Activities (EU), free reuse with
 *   attribution.
 *
 * parse_notes trap — "Use features[].geometry.coordinates = [lon, lat]; the
 * properties also carry a redundant DMS 'position' string and separate
 * 'latitude'/'longitude' DMS strings — parse the geometry, not the strings."
 * Only the geometry is used for lat/lon here.
 * parse_notes trap — "Very many fields are the literal string 'n/a' rather
 * than null, so treat 'n/a' as missing." wsv() below drops them.
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
                "&outputFormat=application/json&typeName=emodnet:wwshipwrecks" \
                "&count=5000"

/* string field, with the literal "n/a" treated as missing */
static const char *wsv(const cJSON *o, const char *k) {
  const char *s = jo_sv(o, k);
  if (!s) return NULL;
  if (strcmp(s, "n/a") == 0 || strcmp(s, "N/A") == 0) return NULL;
  return s;
}
static inline void add_w(cJSON *dst, const cJSON *src, const char *k) {
  const char *s = wsv(src, k);
  if (s) cJSON_AddStringToObject(dst, k, s);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, WFS_URL, 120000);
  if (!doc) {
    fprintf(stderr, "[emodnet-ww-shipwrecks] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    const char *wid = wsv(pr, "wreck_id");
    if (!wid) continue;
    const char *name = wsv(pr, "name");
    const char *cat  = wsv(pr, "wreck_cate");

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
    cJSON_AddStringToObject(p, "wreck_id", wid);
    add_w(p, pr, "name");
    add_w(p, pr, "type");
    add_w(p, pr, "flag");
    add_w(p, pr, "wreck_cate");
    add_w(p, pr, "status");
    add_w(p, pr, "depth");
    add_w(p, pr, "depth_qual");
    add_w(p, pr, "water_dept");
    add_w(p, pr, "water_leve");
    add_w(p, pr, "length");
    add_w(p, pr, "width");
    add_w(p, pr, "tonnage");
    add_w(p, pr, "tonnage_ty");
    add_w(p, pr, "cargo");
    add_w(p, pr, "date_sunk");
    add_w(p, pr, "original_d");
    add_w(p, pr, "last_detec");
    add_w(p, pr, "conspic_vi");
    add_w(p, pr, "conspic_ra");
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "charted-wreck-position");
    }
    cJSON_AddStringToObject(p, "source", "EMODnet Human Activities");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *depth = wsv(pr, "depth");
    const char *type = wsv(pr, "type");
    char title[220], summary[240];
    snprintf(title, sizeof title, "Wreck %s", name ? name : wid);
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             cat ? cat : "charted wreck",
             type ? " · " : "", type ? type : "",
             depth ? " · least depth " : "", depth ? depth : "");

    intel_item it = {0};
    it.remote_key      = wid;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://emodnet.ec.europa.eu/en/human-activities";
    it.record_type     = "shipwreck";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"shipwreck\",\"navigation-hazard\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[emodnet-ww-shipwrecks] emitted %d\n", n);
  return 0;
}

static const source_def mar_emodnet_ww_shipwrecks_def = {
  .id = "emodnet-ww-shipwrecks", .collector = "maritime",
  .name = "EMODnet Human Activities — world-wide shipwreck database",
  .update_interval_sec = 604800, .run = run,
  .category = "transport", .type = "dataset", .url = WFS_URL,
  .description = "Charted wrecks worldwide with hazard category (dangerous wreck / non-dangerous / distributed remains), least depth, ship name, type, flag, tonnage and cargo.",
  .license = "EMODnet Human Activities (EU), free reuse with attribution",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_emodnet_ww_shipwrecks_def)
