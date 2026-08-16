/* collectors/sources/mar_emodnet_heritage_shipwrecks.c
 * EMODnet Human Activities — legally protected heritage shipwrecks in European
 * waters: war graves, protected military remains and designated heritage
 * sites, where anchoring, dredging or salvage is restricted.
 *
 * Endpoint: https://ows.emodnet-humanactivities.eu/wfs?service=WFS
 *   &version=2.0.0&request=GetFeature&outputFormat=application/json
 *   &typeName=emodnet:heritageshipwrecks&count=2000
 * Emits (all fetched): source_id, name, country, obj_type, sink_yr, dating,
 *   period, statutory protection status, depth_info, site_area, sink_context,
 *   obj_desc, source_inf, yr_updated, point_info and the recorded wreck site
 *   from features[].geometry.coordinates.
 * Keyless WFS. Licence: EMODnet Human Activities (EU), free reuse with
 *   attribution.
 *
 * parse_notes trap — "Numeric zeros are encoded as '0E-11' strings/decimals
 * meaning 'not recorded' — do not present them as a real depth or length."
 * add_measure() below drops any numeric that parses to exactly 0 for the
 * measurement fields, so a "not recorded" never surfaces as a 0 m depth.
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
                "&typeName=emodnet:heritageshipwrecks&count=2000"

static const char *hsv(const cJSON *o, const char *k) {
  const char *s = jo_sv(o, k);
  if (!s) return NULL;
  if (strcmp(s, "n/a") == 0 || strcmp(s, "N/A") == 0) return NULL;
  return s;
}
static inline void add_s(cJSON *dst, const cJSON *src, const char *k) {
  const char *s = hsv(src, k);
  if (s) cJSON_AddStringToObject(dst, k, s);
}
/* '0E-11' means "not recorded" upstream — never emit it as a real measurement */
static inline void add_measure(cJSON *dst, const cJSON *src, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(src, k);
  if (cJSON_IsNumber(v) && v->valuedouble != 0.0)
    cJSON_AddNumberToObject(dst, k, v->valuedouble);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, WFS_URL, 90000);
  if (!doc) {
    fprintf(stderr, "[emodnet-heritage-shipwrecks] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    const char *sid = hsv(pr, "source_id");
    const char *name = hsv(pr, "name");
    if (!sid) continue;

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
    cJSON_AddStringToObject(p, "source_id", sid);
    add_s(p, pr, "name");
    add_s(p, pr, "country");
    add_s(p, pr, "obj_type");
    add_s(p, pr, "sink_yr");
    add_s(p, pr, "dating");
    add_s(p, pr, "period");
    add_s(p, pr, "statutory");
    add_s(p, pr, "depth_info");
    add_s(p, pr, "sink_context");
    add_s(p, pr, "obj_desc");
    add_s(p, pr, "ship_char");
    add_s(p, pr, "source_inf");
    add_s(p, pr, "yr_updated");
    add_s(p, pr, "point_info");
    add_measure(p, pr, "least_depth");
    add_measure(p, pr, "max_depth");
    add_measure(p, pr, "object_length");
    add_measure(p, pr, "est_tonnag");
    add_measure(p, pr, "site_area");
    add_measure(p, pr, "coast_dist");
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "recorded-wreck-site");
    }
    cJSON_AddStringToObject(p, "source", "EMODnet Human Activities");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *statut = hsv(pr, "statutory");
    const char *otype = hsv(pr, "obj_type");
    const char *syr = hsv(pr, "sink_yr");
    char title[220], summary[240];
    snprintf(title, sizeof title, "Protected wreck %s", name ? name : sid);
    snprintf(summary, sizeof summary, "%s%s%s%s%s",
             statut ? statut : "protected site",
             otype ? " · " : "", otype ? otype : "",
             syr ? " · sunk " : "", syr ? syr : "");

    intel_item it = {0};
    it.remote_key      = sid;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://emodnet.ec.europa.eu/en/human-activities";
    it.record_type     = "protected-wreck-site";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"shipwreck\",\"protected-site\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[emodnet-heritage-shipwrecks] emitted %d\n", n);
  return 0;
}

static const source_def mar_emodnet_heritage_shipwrecks_def = {
  .id = "emodnet-heritage-shipwrecks", .collector = "maritime",
  .name = "EMODnet Human Activities — protected heritage shipwrecks",
  .update_interval_sec = 2592000, .run = run,
  .category = "transport", .type = "dataset", .url = WFS_URL,
  .description = "Legally protected wreck sites in European waters — war graves, protected military remains and designated heritage wrecks — with sinking year, protection status and site description.",
  .license = "EMODnet Human Activities (EU), free reuse with attribution",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_emodnet_heritage_shipwrecks_def)
