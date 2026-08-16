/* collectors/sources/mar_emodnet_dumped_munitions.c
 * EMODnet Human Activities — dumped/encountered munitions on the European
 * seabed (9,476 point records, incl. chemical-munition finds). Hard
 * constraints on anchoring, trawling and cable routing.
 *
 * Endpoint: https://ows.emodnet-humanactivities.eu/wfs?service=WFS
 *   &version=2.0.0&request=GetFeature&outputFormat=application/json
 *   &typeName=emodnet:munitions&count=10000
 * Emits (all fetched): munition_type, description (condition, calibre,
 *   disposal action), infoprov (the reporting source), thedate, dist_coast and
 *   the reported find/dump position from features[].geometry.coordinates.
 * Keyless WFS. Licence: EMODnet Human Activities (EU), aggregating OSPAR ODIMS
 *   national returns; free reuse with attribution.
 *
 * parse_notes trap — "A companion polygon layer emodnet:munitionspoly holds
 * the declared dump AREAS — do not mix the two, and never reduce a dump-area
 * polygon to a point." Only the POINT layer is fetched here; the polygon layer
 * is deliberately not merged into these rows.
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
                "&outputFormat=application/json&typeName=emodnet:munitions" \
                "&count=10000"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, WFS_URL, 120000);
  if (!doc) {
    fprintf(stderr, "[emodnet-dumped-munitions] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    const char *mtype = jo_sv(pr, "munition_type");
    const char *desc = jo_sv(pr, "description");
    if (!mtype && !desc) continue;

    int geo = 0; double lat = 0, lon = 0;
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    if (g && !cJSON_IsNull(g)) {
      cJSON *co = cJSON_GetObjectItem(g, "coordinates");
      cJSON *x = cJSON_GetArrayItem(co, 0), *y = cJSON_GetArrayItem(co, 1);
      if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
        geo = 1; lon = x->valuedouble; lat = y->valuedouble;
      }
    }

    /* the layer carries no stable record id, so key on the WFS feature id */
    const char *fid = jo_sv(f, "id");
    char key[160];
    if (fid) snprintf(key, sizeof key, "%s", fid);
    else if (geo) snprintf(key, sizeof key, "munitions|%.6f,%.6f", lat, lon);
    else continue;

    cJSON *p = cJSON_CreateObject();
    if (mtype) cJSON_AddStringToObject(p, "munition_type", mtype);
    if (desc)  cJSON_AddStringToObject(p, "description", desc);
    const char *v;
    if ((v = jo_sv(pr, "infoprov"))) cJSON_AddStringToObject(p, "infoprov", v);
    if ((v = jo_sv(pr, "thedate")))  cJSON_AddStringToObject(p, "thedate", v);
    const cJSON *dc = cJSON_GetObjectItem(pr, "dist_coast");
    if (cJSON_IsNumber(dc)) cJSON_AddNumberToObject(p, "dist_coast_m", dc->valuedouble);
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "reported-find-position");
      cJSON_AddStringToObject(p, "geo_note",
        "point find/dump record; declared dump AREAS live in a separate polygon layer");
    }
    cJSON_AddStringToObject(p, "source",
      "EMODnet Human Activities (OSPAR ODIMS national returns)");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[200], summary[256];
    snprintf(title, sizeof title, "Dumped munition — %s",
             mtype ? mtype : "unspecified type");
    snprintf(summary, sizeof summary, "%.240s", desc ? desc : "");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary[0] ? summary : NULL;
    it.link            = "https://emodnet.ec.europa.eu/en/human-activities";
    it.published_at    = NULL;
    it.record_type     = "dumped-munition";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"munitions\",\"seabed-hazard\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[emodnet-dumped-munitions] emitted %d\n", n);
  return 0;
}

static const source_def mar_emodnet_dumped_munitions_def = {
  .id = "emodnet-dumped-munitions", .collector = "maritime",
  .name = "EMODnet Human Activities — dumped munitions",
  .update_interval_sec = 2592000, .run = run,
  .category = "transport", .type = "dataset", .url = WFS_URL,
  .description = "Recorded dumped or encountered munitions on the European seabed, including chemical-munition finds, with condition, calibre and disposal action — hard constraints on anchoring, trawling and cable routing.",
  .license = "EMODnet Human Activities (EU) / OSPAR ODIMS, free reuse with attribution",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_emodnet_dumped_munitions_def)
