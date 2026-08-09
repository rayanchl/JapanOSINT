/* collectors/satellite/sources/alos_palsar.c
 * Port of server/src/collectors/alosPalsar.js.
 * Keyless ASF DAAC SearchAPI (platform=ALOS, JP bbox, jsonlite) → one intel
 * item per SAR scene. Optional JAXA_GPORTAL_TOKEN (note only — ASF path is
 * always attempted, keyless). Each scene pins on its REAL ASF footprint
 * (wkt → GeoJSON Polygon). Honest empty on fetch failure. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ASF returns the scene footprint as WKT, e.g.
 *   POLYGON ((129.837 46.42, 130.015 45.849, ... , 129.837 46.42))
 * Convert the outer ring to a GeoJSON Polygon and its vertex mean to a
 * representative lat/lon. Returns a malloc'd GeoJSON string (caller frees)
 * or NULL if the WKT is missing or not a simple polygon. */
static char *wkt_polygon_to_geojson(const char *wkt, double *out_lat,
                                    double *out_lon) {
  if (!wkt) return NULL;
  const char *p = strstr(wkt, "POLYGON");
  if (!p) return NULL;
  p = strchr(p, '(');
  if (!p) return NULL;
  while (*p == '(' || *p == ' ') p++;     /* into the outer ring */
  cJSON *ring = cJSON_CreateArray();
  double sx = 0, sy = 0;
  int n = 0;
  while (*p && *p != ')') {
    char *end = NULL;
    double lon = strtod(p, &end);
    if (end == p) break;
    p = end;
    while (*p == ' ') p++;
    double lat = strtod(p, &end);
    if (end == p) break;
    p = end;
    cJSON *pt = cJSON_CreateArray();
    cJSON_AddItemToArray(pt, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(pt, cJSON_CreateNumber(lat));
    cJSON_AddItemToArray(ring, pt);
    sx += lon; sy += lat; n++;
    while (*p == ' ' || *p == ',') p++;
  }
  if (n < 4) { cJSON_Delete(ring); return NULL; }
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Polygon");
  cJSON *coords = cJSON_CreateArray();
  cJSON_AddItemToArray(coords, ring);
  cJSON_AddItemToObject(g, "coordinates", coords);
  char *s = cJSON_PrintUnformatted(g);
  cJSON_Delete(g);
  if (out_lat) *out_lat = sy / n;
  if (out_lon) *out_lon = sx / n;
  return s;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *gportal = getenv("JAXA_GPORTAL_TOKEN");
  int has_gp = gportal && *gportal;

  const char *url =
    "https://api.daac.asf.alaska.edu/services/search/param"
    "?platform=ALOS&bbox=122.0,24.0,146.0,46.0"
    "&maxResults=50&output=jsonlite";
  cJSON *data = feed_get_json(ctx->http, url, 20000);

  cJSON *rows = NULL;
  if (data) {
    cJSON *r = cJSON_GetObjectItem(data, "results");
    if (r && cJSON_IsArray(r)) rows = r;
    else if (cJSON_IsArray(data)) rows = data;
  }
  if (!rows || cJSON_GetArraySize(rows) == 0) {
    if (data) cJSON_Delete(data);
    fprintf(stderr, "[alos-palsar] unavailable (ASF DAAC no results)\n");
    return -1;
  }

  int n = 0, i = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    /* (cap removed: every record of the fetched array is emitted —
     * docs/SOURCE_EXHAUSTIVENESS.md) */
    char sidbuf[32];
    const char *sid = jo_sv(r, "granuleName");
    if (!sid) sid = jo_sv(r, "sceneName");
    if (!sid) sid = jo_sv(r, "productID");
    if (!sid) { snprintf(sidbuf, sizeof sidbuf, "scene-%d", i); sid = sidbuf; }

    const char *acquired = jo_sv(r, "startTime");
    if (!acquired) acquired = jo_sv(r, "sceneDate");
    if (!acquired) acquired = jo_sv(r, "processingDate");
    /* the field is downloadUrl, not url — "url" never matched, so every row
     * carried download_url:null and linked to the bare ASF search page */
    const char *dl = jo_sv(r, "downloadUrl");
    if (!dl) dl = jo_sv(r, "url");
    /* granuleName is NOT unique: ASF returns one row per PRODUCT (L1.5, L2.2,
     * …) of the same granule, so half of every batch collided on the uid and
     * was silently overwritten (50 emitted → 25 stored). productID is. */
    const char *pid = jo_sv(r, "productID");
    const char *uidkey = pid ? pid : sid;

    char title[128], summary[256], bodytxt[512];
    snprintf(title, sizeof title, "ALOS PALSAR scene %s", sid);
    snprintf(summary, sizeof summary,
      "JAXA ALOS PALSAR SAR scene acquired %s over Japan.",
      acquired ? acquired : "unknown");
    snprintf(bodytxt, sizeof bodytxt,
      "ALOS PALSAR L-band SAR scene %s (ASF DAAC archive) intersecting the "
      "Japan AOI. Acquired %s.%s",
      sid, acquired ? acquired : "unknown",
      has_gp ? " JAXA G-Portal token present (higher-tier products available "
               "via G-Portal)." : "");

    /* Real per-scene footprint from ASF's wkt, replacing the fixed Japan AOI
     * bbox the port stored on every row (which claimed each 70km SAR scene
     * covered the whole country and left has_geo=0 so it never pinned). */
    double clat = 0, clon = 0;
    char *gj = wkt_polygon_to_geojson(jo_sv(r, "wkt"), &clat, &clon);

    cJSON *p = cJSON_CreateObject();
    if (acquired) cJSON_AddStringToObject(p, "acquired", acquired);
    else cJSON_AddNullToObject(p, "acquired");
    cJSON_AddStringToObject(p, "sensor", "PALSAR");
    cJSON_AddStringToObject(p, "platform", "ALOS");
    cJSON_AddStringToObject(p, "scene_id", sid);
    const char *bm = jo_sv(r, "beamMode");
    if (bm) cJSON_AddStringToObject(p, "beam_mode", bm);
    else cJSON_AddNullToObject(p, "beam_mode");
    const char *pol = jo_sv(r, "polarization");
    if (pol) cJSON_AddStringToObject(p, "polarization", pol);
    else cJSON_AddNullToObject(p, "polarization");
    if (dl) cJSON_AddStringToObject(p, "download_url", dl);
    else cJSON_AddNullToObject(p, "download_url");
    /* --- fields the port dropped: everything that makes a scene usable --- */
    if (pid) cJSON_AddStringToObject(p, "product_id", pid);
    const char *pt = jo_sv(r, "productType");
    if (pt) cJSON_AddStringToObject(p, "product_type", pt);
    const char *fd = jo_sv(r, "flightDirection");
    if (fd) cJSON_AddStringToObject(p, "flight_direction", fd);
    const char *stop = jo_sv(r, "stopTime");
    if (stop) cJSON_AddStringToObject(p, "stop_time", stop);
    const char *fn = jo_sv(r, "fileName");
    if (fn) cJSON_AddStringToObject(p, "file_name", fn);
    const char *ona = jo_sv(r, "offNadirAngle");
    if (ona) cJSON_AddStringToObject(p, "off_nadir_angle", ona);
    cJSON *pathv = cJSON_GetObjectItem(r, "path");
    if (pathv && cJSON_IsNumber(pathv))
      cJSON_AddNumberToObject(p, "path", pathv->valuedouble);
    cJSON *framev = cJSON_GetObjectItem(r, "frame");
    if (framev && cJSON_IsNumber(framev))
      cJSON_AddNumberToObject(p, "frame", framev->valuedouble);
    cJSON *szv = cJSON_GetObjectItem(r, "sizeMB");
    if (szv && cJSON_IsNumber(szv))
      cJSON_AddNumberToObject(p, "size_mb", szv->valuedouble);
    cJSON *orb = cJSON_GetObjectItem(r, "orbit");
    if (orb && cJSON_IsArray(orb) && cJSON_GetArraySize(orb) > 0) {
      cJSON *o0 = cJSON_GetArrayItem(orb, 0);
      if (cJSON_IsString(o0)) cJSON_AddStringToObject(p, "orbit", o0->valuestring);
    }
    cJSON *br = cJSON_GetObjectItem(r, "browse");
    if (br && cJSON_IsArray(br) && cJSON_GetArraySize(br) > 0) {
      cJSON *b0 = cJSON_GetArrayItem(br, 0);
      if (cJSON_IsString(b0)) cJSON_AddStringToObject(p, "browse_url", b0->valuestring);
    }
    if (gj) cJSON_AddStringToObject(p, "footprint_wkt", jo_sv(r, "wkt"));
    char *pj = cJSON_PrintUnformatted(p);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("satellite"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("alos"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("palsar"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("sar"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("jaxa"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("raster"));
    char *tj = cJSON_PrintUnformatted(tags);

    intel_item it = {0};
    it.remote_key = uidkey;          /* uid alos-palsar|<productID> */
    it.title = title;
    it.summary = summary;
    it.body = bodytxt;
    it.link = dl ? dl : "https://search.asf.alaska.edu/";
    it.published_at = acquired;
    it.record_type = "alos-palsar";
    it.has_geo = gj ? 1 : 0;
    it.lat = clat; it.lon = clon;
    it.geometry_geojson = gj;
    it.properties_json = pj;
    it.tags_json = tj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(gj); free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags);
    i++;
  }
  cJSON_Delete(data);
  fprintf(stderr, "[alos-palsar] emitted %d\n", n);
  return 0;      /* the empty-result case already returned -1 above */
}

static const source_def alos_palsar_def = {
  .id = "alos-palsar", .collector = "satellite",
  .name = "ALOS/PALSAR SAR", .name_ja = "ALOS/PALSAR 合成開口レーダー",
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(alos_palsar_def)
