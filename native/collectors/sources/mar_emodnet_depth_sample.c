/* collectors/sources/mar_emodnet_depth_sample.c
 * OSINT service — EMODNET_DEPTH_SAMPLE. On-demand coordinate pivot
 * (ctx->entity = "lat,lon"): charted depth at a European-seas coordinate from
 * the EMODnet Bathymetry DTM, with the provenance of the survey behind it.
 *
 * Endpoint: https://rest.emodnet-bathymetry.eu/depth_sample?geom=POINT(lon lat)
 * Emits (all fetched): min/max/avg/stdev depth in metres (negative = below sea
 *   level), stdev, elementarySurfaces, smoothed / smoothedOffset, and the
 *   survey reference (organisation_id, identifier, type, metadata_url).
 * Keyless. Licence: EMODnet Bathymetry (EU), CC-BY.
 *
 * parse_notes trap — "There is no lat/lon in the RESPONSE — the coordinate is
 * the one YOU supplied in the geom=POINT(lon lat) query (note lon first), so
 * has_geo comes from the request, which is legitimate for a coord pivot."
 * The emitted point is exactly the queried point and is labelled as such.
 * parse_notes trap — "Coverage is European seas only: a Tokyo Bay point
 * (139.8 35.3) returned HTTP 204 with an EMPTY body — 204/empty is an honest
 * 'outside coverage' and must return 0, not -1." Handled explicitly.
 * parse_notes — "Depths are negative metres."
 */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static int run(const source_ctx *ctx, intel_sink *sink) {
  double lat = 0, lon = 0;
  if (!jo_parse_coord(ctx->entity, &lat, &lon)) return 0;

  /* geom=POINT(lon lat) — longitude FIRST; the space is %20-encoded */
  char url[220];
  snprintf(url, sizeof url,
           "https://rest.emodnet-bathymetry.eu/depth_sample?geom=POINT(%.6f%%20%.6f)",
           lon, lat);

  http_response hr = {0};
  int rc = http_request(ctx->http, "GET", url, NULL, NULL, 0, 25000, 1, &hr);
  if (rc != 0) { http_response_free(&hr); return 0; }
  if (hr.status == 204 || !hr.body || hr.body_len == 0) {
    /* outside EMODnet coverage — honest empty, never an error (R3) */
    http_response_free(&hr);
    return 0;
  }
  if (hr.status != 200) { http_response_free(&hr); return 0; }
  cJSON *doc = cJSON_Parse(hr.body);
  http_response_free(&hr);
  if (!doc) return 0;

  const cJSON *avg = cJSON_GetObjectItem(doc, "avg");
  if (!cJSON_IsNumber(avg)) { cJSON_Delete(doc); return 0; }

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "service", "EMODNET_DEPTH_SAMPLE");
  cJSON_AddNumberToObject(p, "lat", lat);
  cJSON_AddNumberToObject(p, "lon", lon);
  cJSON_AddStringToObject(p, "geo_precision", "queried-point");
  cJSON_AddStringToObject(p, "geo_note",
    "the coordinate is the point supplied in the query; the response carries none");
  jo_copy_num(p, doc, "min");
  jo_copy_num(p, doc, "max");
  jo_copy_num(p, doc, "avg");
  jo_copy_num(p, doc, "stdev");
  jo_copy_num(p, doc, "elementarySurfaces");
  jo_copy_num(p, doc, "smoothed");
  jo_copy_num(p, doc, "smoothedOffset");
  cJSON_AddStringToObject(p, "depth_units", "metres (negative = below sea level)");
  const cJSON *ref = cJSON_GetObjectItem(doc, "reference");
  if (ref) {
    cJSON *r = cJSON_CreateObject();
    jo_copy_num(r, ref, "organisation_id");
    const char *s;
    if ((s = jo_sv(ref, "identifier")))   cJSON_AddStringToObject(r, "identifier", s);
    if ((s = jo_sv(ref, "type")))         cJSON_AddStringToObject(r, "type", s);
    if ((s = jo_sv(ref, "metadata_url"))) cJSON_AddStringToObject(r, "metadata_url", s);
    cJSON_AddItemToObject(p, "reference", r);
  }
  cJSON_AddStringToObject(p, "source", "EMODnet Bathymetry");
  cJSON_AddBoolToObject(p, "success", 1);
  char *pj = cJSON_PrintUnformatted(p);
  cJSON_Delete(p);

  char key[64], title[160], summary[200];
  snprintf(key, sizeof key, "%.5f,%.5f", lat, lon);
  snprintf(title, sizeof title, "EMODnet depth at %.5f, %.5f", lat, lon);
  snprintf(summary, sizeof summary, "%.2f m (avg), min %.2f / max %.2f m",
           avg->valuedouble,
           cJSON_IsNumber(cJSON_GetObjectItem(doc, "min"))
             ? cJSON_GetObjectItem(doc, "min")->valuedouble : avg->valuedouble,
           cJSON_IsNumber(cJSON_GetObjectItem(doc, "max"))
             ? cJSON_GetObjectItem(doc, "max")->valuedouble : avg->valuedouble);

  intel_item it = {0};
  it.remote_key      = key;
  it.title           = title;
  it.summary         = summary;
  it.link            = url;
  it.lang            = "en";
  it.record_type     = "bathymetry-sample";
  it.has_geo         = 1;
  it.lat             = lat;
  it.lon             = lon;
  it.properties_json = pj ? pj : "{}";
  it.tags_json       = "[\"osint-search\",\"maritime\",\"bathymetry\"]";
  sink->emit(sink, &it);
  free(pj);
  cJSON_Delete(doc);
  fprintf(stderr, "[EMODNET_DEPTH_SAMPLE] emitted 1 (%s)\n", key);
  return 0;
}

static const source_def mar_emodnet_depth_sample_def = {
  .id = "EMODNET_DEPTH_SAMPLE", .collector = "osint",
  .name = "EMODnet Bathymetry — depth at a coordinate",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "api",
  .url = "https://rest.emodnet-bathymetry.eu/depth_sample",
  .description = "Charted depth at an arbitrary European-seas coordinate from the EMODnet Bathymetry DTM, with the provenance of the survey that produced it — under-keel clearance checks.",
  .license = "EMODnet Bathymetry (EU), CC-BY",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_emodnet_depth_sample_def)
