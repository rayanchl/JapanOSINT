/* collectors/sources/tsp_doi_radio_sites.c
 * US Department of the Interior radio sites — the federal land-management
 * radio site inventory (BLM, NPS, FWS, BIA ...), which fills the gap left by
 * the FCC databases: they do not cover US federal government spectrum users.
 * Endpoint: https://services1.arcgis.com/Hp6G80Pky0om7QvQ/arcgis/rest/services/
 *   DOI_Radio_Sites_20180226/FeatureServer/0/query?where=1=1&outFields=*&f=json
 *   (keyless Esri FeatureServer)
 * Emits every attribute the layer returns — OBJECTID, SERIAL, ORGCODE,
 *   ORGANIZATI (operating bureau), AntennaHei and the layer's own Latitude /
 *   Longitude — plus the mapped point.
 *
 * parse_notes honoured, quoted:
 *  - "spatialReference is wkid 4269 (NAD83) — for mapping purposes it is
 *    within a metre of WGS84, so no reprojection is needed, but say so in
 *    properties": properties.geo_crs records NAD83 / wkid 4269 and states that
 *    no reprojection was applied.
 *  - "Paginate via resultOffset": PAGE_SIZE / PAGES below.
 *  - "Layer name embeds a 2018 snapshot date — this is a static extract, so
 *    poll weekly at most and state the vintage": update_interval_sec is
 *    604800 and properties.data_vintage names the 2018-02-26 snapshot the
 *    layer itself is published under.
 * R2: the coordinate is the site's own mapped point from the feature service;
 *   nothing is geocoded from an organisation name.
 * Licence: US federal open GIS data on a public ArcGIS Online feature service,
 *   no key. US Government public domain.
 */
#include "lib/jocore.h"
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include "lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DOI_BASE \
  "https://services1.arcgis.com/Hp6G80Pky0om7QvQ/arcgis/rest/services/" \
  "DOI_Radio_Sites_20180226/FeatureServer/0/query"
#define PAGE_SIZE 2000
#define PAGES 3

static int emit_features(cJSON *feats, intel_sink *sink) {
  int n = 0;
  cJSON *ft;
  cJSON_ArrayForEach(ft, feats) {
    cJSON *at = cJSON_GetObjectItem(ft, "attributes");
    cJSON *gm = cJSON_GetObjectItem(ft, "geometry");
    if (!cJSON_IsObject(at)) continue;

    const char *serial = jo_sv(at, "SERIAL");
    const char *org    = jo_sv(at, "ORGANIZATI");
    const char *orgc   = jo_sv(at, "ORGCODE");
    double oid = 0;
    int has_oid = jo_num(at, "OBJECTID", &oid);
    if (!serial && !has_oid) continue;            /* no identity -> no row */

    double lat = 0, lon = 0;
    int has_geo = 0;
    if (cJSON_IsObject(gm)) {
      double x, y;
      if (jo_num(gm, "x", &x) && jo_num(gm, "y", &y) &&
          y >= -90.0 && y <= 90.0 && x >= -180.0 && x <= 180.0 &&
          !(x == 0.0 && y == 0.0)) {
        lon = x; lat = y; has_geo = 1;
      }
    }
    if (!has_geo) {
      double la, lo;
      if (jo_num(at, "Latitude", &la) && jo_num(at, "Longitude", &lo) &&
          la >= -90.0 && la <= 90.0 && lo >= -180.0 && lo <= 180.0 &&
          !(la == 0.0 && lo == 0.0)) {
        lat = la; lon = lo; has_geo = 1;
      }
    }

    cJSON *pr = cJSON_CreateObject();
    const cJSON *a;
    cJSON_ArrayForEach(a, at) {
      if (!a->string) continue;
      if (cJSON_IsString(a) && a->valuestring && a->valuestring[0])
        cJSON_AddStringToObject(pr, a->string, a->valuestring);
      else if (cJSON_IsNumber(a))
        cJSON_AddNumberToObject(pr, a->string, a->valuedouble);
    }
    if (has_geo) {
      cJSON_AddStringToObject(pr, "geo_subject", "DOI radio site");
      cJSON_AddStringToObject(pr, "geo_crs",
        "NAD83 (wkid 4269) as published; not reprojected — within about a metre of WGS84");
    }
    cJSON_AddStringToObject(pr, "data_vintage",
      "2018-02-26 snapshot (the date embedded in the published layer name)");
    cJSON_AddStringToObject(pr, "source", "US DOI Radio Sites ArcGIS feature service");
    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    double ht = 0;
    int has_ht = jo_num(at, "AntennaHei", &ht);
    char title[256], summary[256], key[96];
    if (serial) snprintf(key, sizeof key, "%s", serial);
    else        snprintf(key, sizeof key, "OBJECTID-%.0f", oid);
    snprintf(title, sizeof title, "DOI radio site %s%s%s",
             serial ? serial : key, org ? " — " : "", org ? org : "");
    char httxt[48] = "";
    if (has_ht) snprintf(httxt, sizeof httxt, " · antenna %.1f m", ht);
    snprintf(summary, sizeof summary, "%s%s%s",
             orgc ? "bureau code " : "", orgc ? orgc : "federal radio site",
             httxt);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = "https://www.doi.gov/";
    it.lang            = "en";
    it.record_type     = "federal-radio-site";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"telecom\",\"radio-site\",\"federal\",\"usa\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0, pages = 0;
  for (int i = 0; i < PAGES; i++) {
    char url[512];
    snprintf(url, sizeof url,
             "%s?where=1%%3D1&outFields=*&returnGeometry=true"
             "&resultRecordCount=%d&resultOffset=%d&f=json",
             DOI_BASE, PAGE_SIZE, i * PAGE_SIZE);
    cJSON *doc = feed_get_json(ctx->http, url, 60000);
    if (!doc) break;
    cJSON *feats = cJSON_GetObjectItem(doc, "features");
    if (!cJSON_IsArray(feats)) { cJSON_Delete(doc); break; }
    pages++;
    int got = cJSON_GetArraySize(feats);
    total += emit_features(feats, sink);
    cJSON_Delete(doc);
    if (got == 0) break;
  }
  if (pages == 0) {
    fprintf(stderr, "[doi-radio-sites] fetch failed\n");
    return -1;
  }
  fprintf(stderr, "[doi-radio-sites] emitted %d over %d page(s)\n", total, pages);
  return 0;
}

static const source_def tsp_doi_radio_sites_def = {
  .id = "doi-radio-sites", .collector = "telecom",
  .name = "US Department of the Interior radio sites",
  .update_interval_sec = 604800, .run = run,
  .category = "telecom", .type = "dataset", .url = DOI_BASE,
  .description = "Federal land-management radio site inventory: which DOI bureau operates each site, its serial identifier, antenna height and mapped location — federal spectrum users the FCC databases do not cover.",
  .license = "US federal open GIS data on a public ArcGIS Online feature service — US Government public domain, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_doi_radio_sites_def)
