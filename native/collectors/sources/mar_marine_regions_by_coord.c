/* collectors/sources/mar_marine_regions_by_coord.c
 * OSINT service — MARINE_REGIONS_BY_COORD. On-demand coordinate pivot
 * (ctx->entity = "lat,lon"): answers "whose water is this?" — the EEZ,
 * territorial sea, contiguous zone, IHO sea area and regional names that
 * contain the point, each with its MRGID and the authoritative source.
 *
 * Endpoint: https://marineregions.org/rest/getGazetteerRecordsByLatLong.json/
 *           <lat>/<lon>/
 * Emits (all fetched): MRGID, preferredGazetteerName, placeType,
 *   gazetteerSource, status, precision (metres) and the region's bounding box
 *   (minLatitude/minLongitude/maxLatitude/maxLongitude).
 * Keyless. Licence: Flanders Marine Institute (VLIZ) Marine Regions, CC BY 4.0
 *   — attribution and DOI citation requested; the citation string is returned
 *   in gazetteerSource and is carried through verbatim.
 *
 * parse_notes trap — "The latitude/longitude on each returned record are the
 * CENTROID of the named region (with min/max bounding box and a 'precision' in
 * metres), NOT the point you queried and definitely not a vessel position ...
 * The real geographic answer is the set of region names; the query coordinate
 * itself is the honest point to plot." So each row is plotted at the QUERIED
 * point (labelled queried-point), the region centroid is carried in properties
 * as region_centroid_* with geo_precision "area-centroid", and the bbox is
 * emitted alongside. No region centroid is ever used as the row's lat/lon.
 * parse_notes — "Empty array for a land point is an honest empty."
 */
#include "source.h"
#include "third_party/cJSON.h"
#include "core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static inline void add_num(cJSON *dst, const cJSON *src, const char *from,
                           const char *to) {
  const cJSON *v = cJSON_GetObjectItem(src, from);
  if (cJSON_IsNumber(v)) cJSON_AddNumberToObject(dst, to, v->valuedouble);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  double qlat = 0, qlon = 0;
  if (!jo_parse_coord(ctx->entity, &qlat, &qlon)) return 0;

  char url[220];
  snprintf(url, sizeof url,
           "https://marineregions.org/rest/getGazetteerRecordsByLatLong.json/%.6f/%.6f/",
           qlat, qlon);

  char *body = jo_get(ctx, url, NULL, "MARINE_REGIONS_BY_COORD");
  if (!body) return 0;                        /* 404 on land = honest empty */
  cJSON *doc = cJSON_Parse(body);
  free(body);
  if (!doc) return 0;
  cJSON *arr = cJSON_IsArray(doc) ? doc : cJSON_GetObjectItem(doc, "results");

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    if (!cJSON_IsObject(r)) continue;
    const cJSON *mrgid = cJSON_GetObjectItem(r, "MRGID");
    const char *name = jo_sv(r, "preferredGazetteerName");
    if (!cJSON_IsNumber(mrgid) || !name) continue;
    const char *ptype = jo_sv(r, "placeType");
    const char *src = jo_sv(r, "gazetteerSource");

    char key[64];
    snprintf(key, sizeof key, "%lld|%.4f,%.4f",
             (long long)mrgid->valuedouble, qlat, qlon);

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "service", "MARINE_REGIONS_BY_COORD");
    cJSON_AddNumberToObject(p, "MRGID", mrgid->valuedouble);
    cJSON_AddStringToObject(p, "preferredGazetteerName", name);
    if (ptype) cJSON_AddStringToObject(p, "placeType", ptype);
    if (src)   cJSON_AddStringToObject(p, "gazetteerSource", src);
    const char *st = jo_sv(r, "status");
    if (st) cJSON_AddStringToObject(p, "status", st);
    /* the record's own lat/lon is the REGION centroid — never the row's point */
    add_num(p, r, "latitude",  "region_centroid_lat");
    add_num(p, r, "longitude", "region_centroid_lon");
    add_num(p, r, "minLatitude",  "min_lat");
    add_num(p, r, "minLongitude", "min_lon");
    add_num(p, r, "maxLatitude",  "max_lat");
    add_num(p, r, "maxLongitude", "max_lon");
    add_num(p, r, "precision", "precision_m");
    cJSON_AddStringToObject(p, "region_centroid_precision", "area-centroid");
    cJSON_AddNumberToObject(p, "lat", qlat);
    cJSON_AddNumberToObject(p, "lon", qlon);
    cJSON_AddStringToObject(p, "geo_precision", "queried-point");
    cJSON_AddStringToObject(p, "geo_note",
      "plotted at the queried coordinate; the record's own lat/lon is the region centroid, not this point and not a vessel");
    cJSON_AddStringToObject(p, "source", "VLIZ Marine Regions gazetteer");
    cJSON_AddBoolToObject(p, "success", 1);
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[240], summary[240], link[120];
    snprintf(title, sizeof title, "%s%s%s", name,
             ptype ? " — " : "", ptype ? ptype : "");
    snprintf(summary, sizeof summary, "contains %.4f, %.4f · MRGID %lld",
             qlat, qlon, (long long)mrgid->valuedouble);
    snprintf(link, sizeof link, "https://marineregions.org/gazetteer.php?p=details&id=%lld",
             (long long)mrgid->valuedouble);

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = link;
    it.lang            = "en";
    it.record_type     = "maritime-zone";
    it.has_geo         = 1;
    it.lat             = qlat;
    it.lon             = qlon;
    it.properties_json = pj ? pj : "{}";
    it.tags_json       = "[\"osint-search\",\"maritime\",\"maritime-boundary\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[MARINE_REGIONS_BY_COORD] emitted %d\n", n);
  return 0;
}

static const source_def mar_marine_regions_by_coord_def = {
  .id = "MARINE_REGIONS_BY_COORD", .collector = "osint",
  .name = "Marine Regions gazetteer — maritime zones containing a coordinate",
  .update_interval_sec = 0, .run = run,
  .category = "transport", .type = "api",
  .url = "https://marineregions.org/rest/getGazetteerRecordsByLatLong.json/",
  .description = "Answers 'whose water is this?' for any coordinate — the EEZ, territorial sea, contiguous zone, IHO sea area and regional names that contain the point, each with its MRGID and boundary source.",
  .license = "VLIZ Marine Regions, CC BY 4.0 (attribution and DOI citation requested)",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(mar_marine_regions_by_coord_def)
