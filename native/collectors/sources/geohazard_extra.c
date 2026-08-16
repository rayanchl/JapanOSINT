/* Additional real-time geohazard feeds (seismic, cyclone, volcano, weather, disaster).
 * RSS ones go through rss_collect; the seismic ones go through the EMSC/ORFEUS
 * seismicportal FDSN event web service, which — unlike the retired RSS feeds —
 * carries real hypocentre coordinates, so those rows can actually pin. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/rss_atom.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "_source_macros.inc"

/* ---- seismicportal (EMSC/ORFEUS) FDSN event service -------------------- */

static int pnum(cJSON *o, const char *k, double *out) {
  cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v || !cJSON_IsNumber(v)) return 0;
  *out = cJSON_GetNumberValue(v); return 1;
}

/* One intel row per event, geo-pinned on the hypocentre. Returns the number
 * emitted, or -1 only when the fetch itself failed (an empty catalogue window
 * is an honest 0, not an error). */
static int fdsn_quakes(const source_ctx *ctx, intel_sink *sink,
                       const char *url, const char *tags) {
  cJSON *doc = feed_get_json(ctx->http, url, 15000);
  if (!doc) { fprintf(stderr, "[%s] fetch failed\n", ctx->source_id); return -1; }
  cJSON *feats = cJSON_GetObjectItem(doc, "features");
  if (!cJSON_IsArray(feats)) { cJSON_Delete(doc); return -1; }

  int n = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *p = cJSON_GetObjectItem(f, "properties");
    if (!p) continue;
    double lat = 0, lon = 0, mag = 0, depth = 0;
    int has_lat = pnum(p, "lat", &lat), has_lon = pnum(p, "lon", &lon);
    int has_mag = pnum(p, "mag", &mag);
    int has_dep = pnum(p, "depth", &depth);
    const char *unid = jo_sv(p, "unid");
    if (!unid) unid = jo_sv(f, "id");
    if (!unid) continue;
    const char *region  = jo_sv(p, "flynn_region");
    const char *time_s  = jo_sv(p, "time");
    const char *magtype = jo_sv(p, "magtype");
    const char *auth    = jo_sv(p, "auth");
    const char *catalog = jo_sv(p, "source_catalog");
    const char *evtype  = jo_sv(p, "evtype");
    const char *lastupd = jo_sv(p, "lastupdate");

    char title[320], body[640], link[160], geom[160];
    if (has_mag)
      snprintf(title, sizeof title, "M%.1f %s — %s", mag,
               magtype ? magtype : "", region ? region : "earthquake");
    else
      snprintf(title, sizeof title, "Earthquake — %s",
               region ? region : "unknown region");
    snprintf(body, sizeof body,
             "%s magnitude %.1f%s%s at %.4f, %.4f, depth %.1f km, %s "
             "(catalog %s, contributed by %s, event %s).",
             evtype && !strcmp(evtype, "ke") ? "Earthquake" : (evtype ? evtype : "Event"),
             has_mag ? mag : 0.0, magtype ? " " : "", magtype ? magtype : "",
             lat, lon, has_dep ? depth : 0.0,
             region ? region : "unknown region",
             catalog ? catalog : "EMSC", auth ? auth : "unknown", unid);
    snprintf(link, sizeof link,
             "https://www.seismicportal.eu/eventdetails.html?unid=%s", unid);
    snprintf(geom, sizeof geom,
             "{\"type\":\"Point\",\"coordinates\":[%.6f,%.6f]}", lon, lat);

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddStringToObject(pr, "event_id", unid);
    if (has_mag) cJSON_AddNumberToObject(pr, "magnitude", mag);
    if (magtype) cJSON_AddStringToObject(pr, "magnitude_type", magtype);
    if (has_dep) cJSON_AddNumberToObject(pr, "depth_km", depth);
    if (has_lat) cJSON_AddNumberToObject(pr, "lat", lat);
    if (has_lon) cJSON_AddNumberToObject(pr, "lon", lon);
    if (region)  cJSON_AddStringToObject(pr, "region", region);
    if (evtype)  cJSON_AddStringToObject(pr, "event_type", evtype);
    if (auth)    cJSON_AddStringToObject(pr, "agency", auth);
    if (catalog) cJSON_AddStringToObject(pr, "catalog", catalog);
    if (lastupd) cJSON_AddStringToObject(pr, "last_update", lastupd);
    cJSON_AddStringToObject(pr, "source", "seismicportal.eu");
    char *pj = cJSON_PrintUnformatted(pr);

    intel_item it = {0};
    it.remote_key = unid;
    it.title = title;
    it.body = body;
    it.summary = body;
    it.link = link;
    it.lang = "en";
    it.published_at = time_s;
    it.record_type = "earthquake";
    it.has_geo = (has_lat && has_lon);
    it.lat = lat; it.lon = lon;
    it.geometry_geojson = (has_lat && has_lon) ? geom : NULL;
    it.properties_json = pj;
    it.tags_json = tags;
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj); cJSON_Delete(pr);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[%s] emitted %d\n", ctx->source_id, n);
  return n;
}

#define FDSN_GLOBAL "https://www.seismicportal.eu/fdsnws/event/1/query" \
                    "?format=json&orderby=time&limit=300"
/* Iceland AOI — events here are contributed by IMO (Icelandic Met Office),
 * whose own RSS endpoint (en.vedur.is/.../earthquakes/rss/) now 404s. */
#define FDSN_ICELAND "https://www.seismicportal.eu/fdsnws/event/1/query" \
                     "?format=json&orderby=time&limit=300" \
                     "&minlat=62.5&maxlat=67.5&minlon=-25.5&maxlon=-12.5"

static int run_geo_emsc_quakes(const source_ctx *c, intel_sink *s) {
  int n = fdsn_quakes(c, s, FDSN_GLOBAL, "[\"hazard\",\"seismic\",\"alert\"]");
  return n < 0 ? -1 : 0;
}
static const source_def geo_emsc_quakes = {
  .id = "emsc-quakes", .collector = "environment",
  .name = "EMSC Latest Earthquakes", .name_ja = "EMSC 最新地震",
  .update_interval_sec = 900, .run = run_geo_emsc_quakes,
  .category = "environment", .type = "api", .url = FDSN_GLOBAL,
  .description = "EMSC/ORFEUS seismicportal FDSN event service — global "
                 "real-time earthquakes with hypocentre coordinates.",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(geo_emsc_quakes)

static int run_geo_iceland_quakes(const source_ctx *c, intel_sink *s) {
  int n = fdsn_quakes(c, s, FDSN_ICELAND, "[\"hazard\",\"seismic\",\"alert\"]");
  return n < 0 ? -1 : 0;
}
static const source_def geo_iceland_quakes = {
  .id = "iceland-quakes", .collector = "environment",
  .name = "Iceland Region Earthquakes (IMO via EMSC)",
  .name_ja = "アイスランド周辺の地震",
  .update_interval_sec = 900, .run = run_geo_iceland_quakes,
  .category = "environment", .type = "api", .url = FDSN_ICELAND,
  .description = "Earthquakes inside the Iceland AOI from the EMSC "
                 "seismicportal FDSN service (IMO-contributed solutions).",
  .layer = NULL, .free_tier = 1 };
REGISTER_SOURCE(geo_iceland_quakes)

RSSX(geo_nhc_cpac, "nhc-cpac", "NOAA NHC Central Pacific", "NOAA NHC Central Pacific", "environment", "environment",
  "https://www.nhc.noaa.gov/index-cp.xml", "en", "[\"hazard\",\"cyclone\",\"alert\"]", 900,
  "NOAA NHC Central Pacific — real-time cyclone hazard feed");

/* /rss.xml 404s since the site rebuild; the live feed is /volcano-news.rss. */
RSSX(geo_volcanodiscovery, "volcanodiscovery", "VolcanoDiscovery", "VolcanoDiscovery", "environment", "environment",
  "https://www.volcanodiscovery.com/volcano-news.rss", "en", "[\"hazard\",\"volcano\",\"alert\"]", 900,
  "VolcanoDiscovery — real-time volcano hazard feed");

/* bom-au-warnings REMOVED 2026-07-31.
 *
 * bom.gov.au/rss/1.xml has 404'd since before this audit. The only live
 * replacement, api.weather.bom.gov.au/v1/warnings, returns real warnings but
 * its own response metadata states: "This API is owned by the Bureau of
 * Meteorology. You must not use, copy or share it." We do not wire the product
 * to an endpoint whose response forbids use, so there is no honest source here
 * to keep. If Australian warnings are wanted, the route is a licensed BOM feed
 * or a redistributor that holds one. */

RSSX(geo_metoffice_warnings, "metoffice-warnings", "UK Met Office Warnings", "UK Met Office Warnings", "environment", "environment",
  "https://www.metoffice.gov.uk/public/data/PWSCache/WarningsRSS/Region/UK", "en", "[\"hazard\",\"weather\",\"alert\"]", 900,
  "UK Met Office Warnings — real-time weather hazard feed");

/* CEMS moved to mapping.emergency.copernicus.eu; the old activations-rapid
 * rss.xml 404s. /latest/feed/ is the live RSS 2.0 activation news feed. */
RSSX(geo_copernicus_ems, "copernicus-ems", "Copernicus Emergency Mgmt", "Copernicus Emergency Mgmt", "environment", "environment",
  "https://mapping.emergency.copernicus.eu/latest/feed/", "en", "[\"hazard\",\"disaster\",\"alert\"]", 900,
  "Copernicus Emergency Mgmt — real-time disaster hazard feed");

RSSX(geo_pdc_disasters, "pdc-disasters", "Pacific Disaster Center", "Pacific Disaster Center", "environment", "environment",
  "https://www.pdc.org/feed/", "en", "[\"hazard\",\"disaster\",\"alert\"]", 900,
  "Pacific Disaster Center — real-time disaster hazard feed");
