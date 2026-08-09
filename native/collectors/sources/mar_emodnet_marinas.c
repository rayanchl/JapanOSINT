/* collectors/sources/mar_emodnet_marinas.c
 * EMODnet Human Activities — European marinas (737 sites with berth counts).
 * Small-craft infrastructure: coastal-access analysis where commercial port
 * data says nothing.
 *
 * Endpoint: https://ows.emodnet-humanactivities.eu/wfs?service=WFS
 *   &version=2.0.0&request=GetFeature&outputFormat=application/json
 *   &typeName=emodnet:emodnetmarinas
 * Emits (all fetched): name, country, berths_n, source and the marina
 *   entrance/basin point from features[].geometry.coordinates.
 * Keyless WFS. Licence: EMODnet Human Activities (EU), aggregating TransEurope
 *   Marinas and national sources; free reuse with attribution.
 *
 * parse_notes — "Coordinate is features[].geometry.coordinates = [lon, lat],
 * duplicated in properties.long/properties.lat — the marina entrance/basin
 * point." The geometry is used; the duplicated properties are not re-derived.
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
                "&outputFormat=application/json&typeName=emodnet:emodnetmarinas"

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, WFS_URL, 60000);
  if (!doc) {
    fprintf(stderr, "[emodnet-marinas] fetch/parse failed\n");
    return -1;
  }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *pr = cJSON_GetObjectItem(f, "properties");
    if (!pr) continue;
    const char *name = jo_sv(pr, "name");
    if (!name) continue;

    int geo = 0; double lat = 0, lon = 0;
    cJSON *g = cJSON_GetObjectItem(f, "geometry");
    if (g && !cJSON_IsNull(g)) {
      cJSON *co = cJSON_GetObjectItem(g, "coordinates");
      cJSON *x = cJSON_GetArrayItem(co, 0), *y = cJSON_GetArrayItem(co, 1);
      if (cJSON_IsNumber(x) && cJSON_IsNumber(y)) {
        geo = 1; lon = x->valuedouble; lat = y->valuedouble;
      }
    }

    const char *fid = jo_sv(f, "id");
    char key[200];
    snprintf(key, sizeof key, "%s", fid ? fid : name);

    const cJSON *berths = cJSON_GetObjectItem(pr, "berths_n");
    const char *cc = jo_sv(pr, "country");
    const char *src = jo_sv(pr, "source");

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "name", name);
    if (cc)  cJSON_AddStringToObject(p, "country", cc);
    if (src) cJSON_AddStringToObject(p, "source_dataset", src);
    if (cJSON_IsNumber(berths))
      cJSON_AddNumberToObject(p, "berths_n", berths->valuedouble);
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "marina-basin-point");
    }
    cJSON_AddStringToObject(p, "source", "EMODnet Human Activities");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[220], summary[200];
    snprintf(title, sizeof title, "Marina %s", name);
    if (cJSON_IsNumber(berths))
      snprintf(summary, sizeof summary, "%s%s%lld berths",
               cc ? cc : "", cc ? " · " : "", (long long)berths->valuedouble);
    else
      snprintf(summary, sizeof summary, "%s", cc ? cc : "European marina");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://emodnet.ec.europa.eu/en/human-activities";
    it.record_type     = "marina";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"marina\",\"emodnet\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[emodnet-marinas] emitted %d\n", n);
  return 0;
}

static const source_def mar_emodnet_marinas_def = {
  .id = "emodnet-marinas", .collector = "maritime",
  .name = "EMODnet Human Activities — European marinas",
  .update_interval_sec = 2592000, .run = run,
  .category = "industry", .type = "dataset", .url = WFS_URL,
  .description = "737 European marinas with berth counts and coordinates — small-craft infrastructure, useful for coastal-access analysis where commercial port data says nothing.",
  .license = "EMODnet Human Activities (EU), free reuse with attribution",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_emodnet_marinas_def)
