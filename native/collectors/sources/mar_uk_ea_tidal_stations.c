/* collectors/sources/mar_uk_ea_tidal_stations.c
 * UK Environment Agency — tidal level monitoring stations (England).
 *
 * Endpoint: https://environment.data.gov.uk/flood-monitoring/id/stations
 *           ?parameter=level&qualifier=Tidal%20Level&_view=full
 * Emits (all fetched): stationReference, label, catchmentName, riverName,
 *   town, dateOpened, status, RLOIid, the measure URIs (which expose the live
 *   15-minute readings), and the surveyed gauge position from lat/long.
 * Keyless. Licence: Environment Agency, Open Government Licence v3 — the
 *   licence URI is returned inside meta.licence of every response.
 *
 * parse_notes trap — "Real coordinate is items[].lat and items[].long (note
 * 'long', not 'lon') ... Requires _view=full — the default view omits lat/long
 * and returns only the @id URIs ... Some stations legitimately lack lat/long —
 * emit those with has_geo=0." All three honoured (R2).
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

#define EA_URL "https://environment.data.gov.uk/flood-monitoring/id/stations" \
               "?parameter=level&qualifier=Tidal%20Level&_view=full"

/* JSON-LD collapses repeated predicates into arrays; take the first string. */
static const char *ld_str(const cJSON *o, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return NULL;
  if (cJSON_IsString(v) && v->valuestring[0]) return v->valuestring;
  if (cJSON_IsArray(v)) {
    const cJSON *e;
    cJSON_ArrayForEach(e, v)
      if (cJSON_IsString(e) && e->valuestring[0]) return e->valuestring;
  }
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http, EA_URL, 30000);
  if (!doc) {
    fprintf(stderr, "[uk-ea-tidal-stations] fetch/parse failed\n");
    return -1;
  }
  const char *licence = NULL;
  cJSON *meta = cJSON_GetObjectItem(doc, "meta");
  if (meta) licence = jo_sv(meta, "licence");

  cJSON *arr = cJSON_GetObjectItem(doc, "items");
  int n = 0;
  cJSON *s;
  cJSON_ArrayForEach(s, arr) {
    if (!cJSON_IsObject(s)) continue;
    const char *ref = ld_str(s, "stationReference");
    if (!ref) ref = ld_str(s, "notation");
    if (!ref) continue;
    const char *label = ld_str(s, "label");

    const cJSON *latj = cJSON_GetObjectItem(s, "lat");
    const cJSON *lonj = cJSON_GetObjectItem(s, "long");   /* 'long', not 'lon' */
    int geo = cJSON_IsNumber(latj) && cJSON_IsNumber(lonj);
    double lat = geo ? latj->valuedouble : 0;
    double lon = geo ? lonj->valuedouble : 0;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "stationReference", ref);
    const char *v;
    if (label)                          cJSON_AddStringToObject(p, "label", label);
    if ((v = ld_str(s, "catchmentName"))) cJSON_AddStringToObject(p, "catchmentName", v);
    if ((v = ld_str(s, "riverName")))     cJSON_AddStringToObject(p, "riverName", v);
    if ((v = ld_str(s, "town")))          cJSON_AddStringToObject(p, "town", v);
    if ((v = ld_str(s, "dateOpened")))    cJSON_AddStringToObject(p, "dateOpened", v);
    if ((v = ld_str(s, "status")))        cJSON_AddStringToObject(p, "status", v);
    if ((v = ld_str(s, "RLOIid")))        cJSON_AddStringToObject(p, "RLOIid", v);
    if ((v = ld_str(s, "@id")))           cJSON_AddStringToObject(p, "station_uri", v);
    const cJSON *ms = cJSON_GetObjectItem(s, "measures");
    if (cJSON_IsArray(ms)) {
      cJSON *uris = cJSON_CreateArray();
      const cJSON *m;
      cJSON_ArrayForEach(m, ms) {
        const char *mu = ld_str(m, "@id");
        if (mu) cJSON_AddItemToArray(uris, cJSON_CreateString(mu));
      }
      cJSON_AddItemToObject(p, "measures", uris);
    }
    if (geo) {
      cJSON_AddNumberToObject(p, "lat", lat);
      cJSON_AddNumberToObject(p, "lon", lon);
      cJSON_AddStringToObject(p, "geo_precision", "surveyed-gauge-position");
    }
    if (licence) cJSON_AddStringToObject(p, "licence", licence);
    cJSON_AddStringToObject(p, "source", "Environment Agency flood-monitoring");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    const char *town = ld_str(s, "town");
    const char *catch_ = ld_str(s, "catchmentName");
    char title[220], summary[220], link[240];
    snprintf(title, sizeof title, "%s (%s)", label ? label : ref, ref);
    snprintf(summary, sizeof summary, "EA tidal level gauge%s%s%s%s",
             town ? " · " : "", town ? town : "",
             catch_ ? " · " : "", catch_ ? catch_ : "");
    const char *uri = ld_str(s, "@id");
    snprintf(link, sizeof link, "%s", uri ? uri : EA_URL);

    intel_item it = {0};
    it.remote_key      = ref;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.record_type     = "tide-gauge-station";
    it.has_geo         = geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"maritime\",\"tide-gauge\",\"uk\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[uk-ea-tidal-stations] emitted %d\n", n);
  return 0;
}

static const source_def mar_uk_ea_tidal_stations_def = {
  .id = "uk-ea-tidal-stations", .collector = "maritime",
  .name = "UK Environment Agency — tidal level monitoring stations",
  .update_interval_sec = 86400, .run = run,
  .category = "transport", .type = "api", .url = EA_URL,
  .description = "England's tidal-level gauge network — station identity, coordinates and the measure URIs that expose live 15-minute sea-level readings for every estuary and coastal port.",
  .license = "Environment Agency, Open Government Licence v3",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_uk_ea_tidal_stations_def)
