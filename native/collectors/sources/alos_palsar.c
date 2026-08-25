/* collectors/satellite/sources/alos_palsar.c
 * Port of server/src/collectors/alosPalsar.js.
 * Keyless ASF DAAC SearchAPI (platform=ALOS, JP bbox, jsonlite) → one intel
 * item per SAR scene. Optional JAXA_GPORTAL_TOKEN (note only — ASF path is
 * always attempted, keyless). Each scene pins on its REAL ASF footprint
 * (wkt → GeoJSON Polygon). Honest empty on fetch failure. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
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

/* Rows asked of ASF per run. Raise with $JO_ALOS_MAX_RESULTS; a full response
 * is disclosed as a collector-truncation-notice, never dropped in silence. */
#define ALOS_MAX_RESULTS 50   /* exhaustive-ok: request page size, and a full page emits a collector-truncation-notice */

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *gportal = getenv("JAXA_GPORTAL_TOKEN");
  int has_gp = gportal && *gportal;

  /* ASF's SearchAPI has no offset/cursor: `maxResults` is the only bound it
   * offers, and the archive behind this bbox is far larger than one run wants
   * to pull. That makes this a bounded VIEW, which house rule 2 allows only if
   * the bound is stated in the data — so when the response comes back exactly
   * full we say so with a collector-truncation-notice below rather than
   * letting the shortfall be invisible. */
  int max_results = ALOS_MAX_RESULTS;
  const char *menv = getenv("JO_ALOS_MAX_RESULTS");
  if (menv && *menv) { int m = atoi(menv); if (m > 0) max_results = m; }

  char url[256];
  snprintf(url, sizeof url,
    "https://api.daac.asf.alaska.edu/services/search/param"
    "?platform=ALOS&bbox=122.0,24.0,146.0,46.0"
    "&maxResults=%d&output=jsonlite", max_results);
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
    /* `orbit` and `browse` are ARRAYS in ASF's jsonlite: one orbit number per
     * revolution the granule spans, one browse JPEG per product view. Taking
     * [0] threw the rest away, which for a granule that straddles two orbits
     * or carries several quicklooks is a real loss. Every element is emitted
     * as `orbits` / `browse_urls`; the scalar `orbit` / `browse_url` stays as
     * the display pick so existing consumers keep working. */
    cJSON *orb = cJSON_GetObjectItem(r, "orbit");
    if (orb && cJSON_IsArray(orb) && cJSON_GetArraySize(orb) > 0) {
      cJSON *all = cJSON_CreateArray(), *o;
      cJSON_ArrayForEach(o, orb) {
        if (!cJSON_IsString(o) || !o->valuestring || !o->valuestring[0]) continue;
        if (cJSON_GetArraySize(all) == 0)
          cJSON_AddStringToObject(p, "orbit", o->valuestring);  /* display pick */
        cJSON_AddItemToArray(all, cJSON_CreateString(o->valuestring));
      }
      if (cJSON_GetArraySize(all) > 0) cJSON_AddItemToObject(p, "orbits", all);
      else cJSON_Delete(all);
    }
    cJSON *br = cJSON_GetObjectItem(r, "browse");
    if (br && cJSON_IsArray(br) && cJSON_GetArraySize(br) > 0) {
      cJSON *all = cJSON_CreateArray(), *b;
      cJSON_ArrayForEach(b, br) {
        if (!cJSON_IsString(b) || !b->valuestring || !b->valuestring[0]) continue;
        if (cJSON_GetArraySize(all) == 0)
          cJSON_AddStringToObject(p, "browse_url", b->valuestring);  /* display */
        cJSON_AddItemToArray(all, cJSON_CreateString(b->valuestring));
      }
      if (cJSON_GetArraySize(all) > 0) cJSON_AddItemToObject(p, "browse_urls", all);
      else cJSON_Delete(all);
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
  int available = cJSON_GetArraySize(rows);
  cJSON_Delete(data);
  fprintf(stderr, "[alos-palsar] emitted %d of %d fetched\n", n, available);
  /* A response that came back exactly `max_results` long means ASF had at
   * least that many and we asked for no more. The archive total is not
   * published by this endpoint, so `available` is honestly reported as
   * unknown rather than guessed. */
  if (available >= max_results)
    jo_trunc_notice(sink, "alos-palsar", url, n, -1,
                    "ASF SearchAPI returned a full maxResults page and offers "
                    "no offset or cursor parameter, so scenes beyond this page "
                    "were not requested.",
                    "raise $JO_ALOS_MAX_RESULTS, or narrow the bbox/time range "
                    "so a run fits inside one response");
  return 0;      /* the empty-result case already returned -1 above */
}

static const source_def alos_palsar_def = {
  .id = "alos-palsar", .collector = "satellite",
  .name = "ALOS/PALSAR SAR", .name_ja = "ALOS/PALSAR 合成開口レーダー",
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(alos_palsar_def)
