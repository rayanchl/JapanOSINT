/* collectors/satellite/sources/gcom_w.c
 * Port of server/src/collectors/gcomW.js.
 * Keyless NASA CMR granule search (keyword=AMSR2, JP bbox) → one intel item
 * per granule. Optional JAXA_GPORTAL_TOKEN (note only — CMR path is keyless).
 * Non-spatial swath catalog (has_geo=0). Honest empty on fetch failure. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *gportal = getenv("JAXA_GPORTAL_TOKEN");
  int has_gp = gportal && *gportal;

  /* CMR rejects an unconstrained keyword granule search with HTTP 400
   * ("does not allow querying across granules in all collections"), which is
   * why the old ?keyword=AMSR2 query returned nothing at all. Constrain by
   * the AMSR-E/AMSR2 Unified collections' short_name; AU_Land is the
   * half-orbit swath product that actually intersects the Japan AOI. */
  static const char *SHORT_NAMES[] = { "AU_Land", "AU_Ocean", "AU_SI12", NULL };
  cJSON *data = NULL, *rows = NULL;
  for (int si = 0; SHORT_NAMES[si] && !rows; si++) {
    char url[320];
    snprintf(url, sizeof url,
      "https://cmr.earthdata.nasa.gov/search/granules.json"
      "?short_name=%s&bounding_box=122.0,24.0,146.0,46.0"
      "&sort_key=-start_date&page_size=50", SHORT_NAMES[si]);
    cJSON *d = feed_get_json(ctx->http, url, 20000);
    cJSON *feed = d ? cJSON_GetObjectItem(d, "feed") : NULL;
    cJSON *e = feed ? cJSON_GetObjectItem(feed, "entry") : NULL;
    if (e && cJSON_IsArray(e) && cJSON_GetArraySize(e) > 0) { data = d; rows = e; break; }
    if (d) cJSON_Delete(d);
  }
  if (!rows) {
    /* Honest empty: the query succeeded, the AOI simply has no granules in
     * the catalogue window. -1 here would quarantine the source. */
    fprintf(stderr, "[gcom-w] no AMSR2 granules over the Japan AOI\n");
    return 0;
  }

  int n = 0, i = 0;
  cJSON *g;
  cJSON_ArrayForEach(g, rows) {
    /* (cap removed: every granule the query returned is emitted —
     * docs/SOURCE_EXHAUSTIVENESS.md) */
    char sidbuf[32];
    const char *sid = jo_sv(g, "producer_granule_id");
    if (!sid) sid = jo_sv(g, "title");
    if (!sid) sid = jo_sv(g, "id");
    if (!sid) { snprintf(sidbuf, sizeof sidbuf, "granule-%d", i); sid = sidbuf; }

    const char *acquired = jo_sv(g, "time_start");
    if (!acquired) acquired = jo_sv(g, "updated");
    const char *time_end = jo_sv(g, "time_end");
    const char *dataset = jo_sv(g, "dataset_id");

    const char *link0 = NULL;
    cJSON *links = cJSON_GetObjectItem(g, "links");
    if (links && cJSON_IsArray(links)) {
      cJSON *l0 = cJSON_GetArrayItem(links, 0);  /* exhaustive-ok: display pick; links_all below carries every granule link */
      link0 = jo_sv(l0, "href");
    }

    char title[160], summary[256], bodytxt[512];
    snprintf(title, sizeof title, "GCOM-W AMSR2 granule %s", sid);
    snprintf(summary, sizeof summary,
      "JAXA GCOM-W AMSR2 microwave granule acquired %s over Japan.",
      acquired ? acquired : "unknown");
    snprintf(bodytxt, sizeof bodytxt,
      "GCOM-W \"SHIZUKU\" AMSR2 microwave radiometer granule %s (NASA CMR "
      "mirror) intersecting the Japan AOI. Acquired %s.%s",
      sid, acquired ? acquired : "unknown",
      has_gp ? " JAXA G-Portal token present (native products via "
               "G-Portal)." : "");

    /* The granule's OWN footprint, from CMR "boxes" ("S W N E"). The old code
     * stored the Japan AOI query box on every row, which said nothing about
     * where the granule actually is. */
    double gs = 0, gw = 0, gn = 0, ge = 0; int has_box = 0;
    cJSON *boxes = cJSON_GetObjectItem(g, "boxes");
    if (boxes && cJSON_IsArray(boxes) && cJSON_GetArraySize(boxes) > 0) {
      cJSON *b0 = cJSON_GetArrayItem(boxes, 0);  /* exhaustive-ok: a granule has one footprint box */
      if (b0 && cJSON_IsString(b0) &&
          sscanf(b0->valuestring, "%lf %lf %lf %lf", &gs, &gw, &gn, &ge) == 4)
        has_box = 1;
    }
    char geom[256] = {0};
    if (has_box)
      snprintf(geom, sizeof geom,
        "{\"type\":\"Polygon\",\"coordinates\":[[[%.4f,%.4f],[%.4f,%.4f],"
        "[%.4f,%.4f],[%.4f,%.4f],[%.4f,%.4f]]]}",
        gw, gs, ge, gs, ge, gn, gw, gn, gw, gs);

    cJSON *p = cJSON_CreateObject();
    cJSON *jb = cJSON_CreateArray();
    cJSON_AddItemToArray(jb, cJSON_CreateNumber(has_box ? gw : 122.0));
    cJSON_AddItemToArray(jb, cJSON_CreateNumber(has_box ? gs : 24.0));
    cJSON_AddItemToArray(jb, cJSON_CreateNumber(has_box ? ge : 146.0));
    cJSON_AddItemToArray(jb, cJSON_CreateNumber(has_box ? gn : 46.0));
    cJSON_AddItemToObject(p, "bbox", jb);
    cJSON_AddBoolToObject(p, "bbox_is_granule_footprint", has_box);
    /* Every distribution link the granule lists (data, browse, metadata), not
     * just link0 above (docs/SOURCE_EXHAUSTIVENESS.md). */
    if (links && cJSON_IsArray(links) && cJSON_GetArraySize(links) > 0)
      cJSON_AddItemToObject(p, "links_all", cJSON_Duplicate(links, 1));
    /* AUDIT: CMR serves the AMSR2 half-orbit collections with
     * coordinate_system=ORBIT, so there is no "boxes" — the footprint comes
     * back as "polygons" [["<lat> <lon> <lat> <lon> …"]]. Carry it through
     * verbatim; it was being dropped entirely. It is deliberately NOT used as
     * the row geometry: a half-orbit swath runs pole to pole (lon -119 → +176
     * on the sample granule), so a centroid or an outline of it would be a
     * garbage pin/band across the whole map rather than "over Japan". */
    {
      cJSON *polys = cJSON_GetObjectItem(g, "polygons");
      if (polys && cJSON_IsArray(polys) && cJSON_GetArraySize(polys) > 0)
        cJSON_AddItemToObject(p, "granule_polygons", cJSON_Duplicate(polys, 1));
      cJSON *ocsd = cJSON_GetObjectItem(g, "orbit_calculated_spatial_domains");
      if (ocsd && cJSON_IsArray(ocsd) && cJSON_GetArraySize(ocsd) > 0)
        cJSON_AddItemToObject(p, "orbit", cJSON_Duplicate(ocsd, 1));
      cJSON *sz = cJSON_GetObjectItem(g, "granule_size");
      if (sz && cJSON_IsString(sz))
        cJSON_AddStringToObject(p, "granule_size_mb", sz->valuestring);
      cJSON *cid = cJSON_GetObjectItem(g, "id");
      if (cid && cJSON_IsString(cid))
        cJSON_AddStringToObject(p, "cmr_concept_id", cid->valuestring);
      cJSON *dnf = cJSON_GetObjectItem(g, "day_night_flag");
      if (dnf && cJSON_IsString(dnf))
        cJSON_AddStringToObject(p, "day_night_flag", dnf->valuestring);
    }
    if (acquired) cJSON_AddStringToObject(p, "acquired", acquired);
    else cJSON_AddNullToObject(p, "acquired");
    if (time_end) cJSON_AddStringToObject(p, "time_end", time_end);
    else cJSON_AddNullToObject(p, "time_end");
    cJSON_AddStringToObject(p, "sensor", "AMSR2");
    cJSON_AddStringToObject(p, "platform", "GCOM-W");
    cJSON_AddStringToObject(p, "scene_id", sid);
    if (dataset) cJSON_AddStringToObject(p, "dataset", dataset);
    else cJSON_AddNullToObject(p, "dataset");
    char *pj = cJSON_PrintUnformatted(p);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("satellite"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("gcom-w"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("amsr2"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("microwave"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("jaxa"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("raster"));
    char *tj = cJSON_PrintUnformatted(tags);

    intel_item it = {0};
    it.remote_key = sid;             /* uid gcom-w|<sid> */
    it.title = title;
    it.summary = summary;
    it.body = bodytxt;
    it.link = link0 ? link0 : "https://gportal.jaxa.jp/";
    it.published_at = acquired;
    it.record_type = "gcom-w";
    if (has_box) {
      it.has_geo = 1;
      it.lat = (gs + gn) / 2.0;
      it.lon = (gw + ge) / 2.0;
      it.geometry_geojson = geom;
    } else {
      it.has_geo = 0;
    }
    it.properties_json = pj;
    it.tags_json = tj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags);
    i++;
  }
  cJSON_Delete(data);
  fprintf(stderr, "[gcom-w] emitted %d\n", n);
  return 0;
}

static const source_def gcom_w_def = {
  .id = "gcom-w", .collector = "satellite",
  .name = "GCOM-W Water Cycle", .name_ja = "GCOM-W 水循環",
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(gcom_w_def)
