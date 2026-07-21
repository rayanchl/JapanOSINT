/* collectors/satellite/sources/landsat_jp.c
 * Port of server/src/collectors/landsatJp.js.
 * Keyless USGS LandsatLook STAC search; fallback to Microsoft Planetary
 * Computer STAC. Optional USGS_M2M_TOKEN (X-Auth-Token header on primary;
 * STAC is keyless). One intel item per scene. Non-spatial scene catalog
 * (has_geo=0). Honest empty on failure. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void iso_now(char *out, size_t n) {
  time_t t = time(NULL);
  struct tm g; gmtime_r(&t, &g);
  strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &g);
}
static void iso_ago(char *out, size_t n, int days) {
  time_t t = time(NULL) - (time_t)days * 86400;
  struct tm g; gmtime_r(&t, &g);
  strftime(out, n, "%Y-%m-%dT%H:%M:%SZ", &g);
}

static const char *sstr(cJSON *o, const char *k) {
  cJSON *v = o ? cJSON_GetObjectItem(o, k) : NULL;
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static int centroid(cJSON *geom, double *cx, double *cy) {
  if (!geom) return 0;
  const char *t = sstr(geom, "type");
  cJSON *coords = cJSON_GetObjectItem(geom, "coordinates");
  cJSON *ring = NULL;
  if (t && !strcmp(t, "Polygon") && cJSON_IsArray(coords))
    ring = cJSON_GetArrayItem(coords, 0);
  else if (t && !strcmp(t, "MultiPolygon") && cJSON_IsArray(coords)) {
    cJSON *poly = cJSON_GetArrayItem(coords, 0);
    if (poly) ring = cJSON_GetArrayItem(poly, 0);
  }
  if (!ring || !cJSON_IsArray(ring)) return 0;
  int len = cJSON_GetArraySize(ring);
  if (len == 0) return 0;
  double sx = 0, sy = 0; cJSON *pt;
  cJSON_ArrayForEach(pt, ring) {
    cJSON *x = cJSON_GetArrayItem(pt, 0), *y = cJSON_GetArrayItem(pt, 1);
    if (x && y) { sx += x->valuedouble; sy += y->valuedouble; }
  }
  *cx = sx / len; *cy = sy / len;
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *m2m = getenv("USGS_M2M_TOKEN");

  char from[32], to[32];
  iso_ago(from, sizeof from, 30);
  iso_now(to, sizeof to);

  char body[512];
  snprintf(body, sizeof body,
    "{\"bbox\":[122,24,146,46],\"datetime\":\"%s/%s\","
    "\"collections\":[\"landsat-c2l2-sr\"],\"limit\":50}", from, to);

  char auth[600];
  const char *hdrs[3];
  hdrs[0] = "Content-Type: application/json";
  if (m2m && *m2m) {
    snprintf(auth, sizeof auth, "X-Auth-Token: %s", m2m);
    hdrs[1] = auth; hdrs[2] = NULL;
  } else { hdrs[1] = NULL; }

  cJSON *data = feed_post_json(ctx->http,
    "https://landsatlook.usgs.gov/stac-server/search", body, hdrs, 15000);

  cJSON *feats = data ? cJSON_GetObjectItem(data, "features") : NULL;
  if (!feats || !cJSON_IsArray(feats) || cJSON_GetArraySize(feats) == 0) {
    if (data) { cJSON_Delete(data); data = NULL; }
    char body2[512];
    snprintf(body2, sizeof body2,
      "{\"bbox\":[122,24,146,46],\"datetime\":\"%s/%s\","
      "\"collections\":[\"landsat-c2-l2\"],\"limit\":50,"
      "\"query\":{\"eo:cloud_cover\":{\"lt\":60}}}", from, to);
    const char *h2[] = { "Content-Type: application/json", NULL };
    data = feed_post_json(ctx->http,
      "https://planetarycomputer.microsoft.com/api/stac/v1/search",
      body2, h2, 15000);
    feats = data ? cJSON_GetObjectItem(data, "features") : NULL;
  }
  if (!feats || !cJSON_IsArray(feats) || cJSON_GetArraySize(feats) == 0) {
    if (data) cJSON_Delete(data);
    fprintf(stderr, "[landsat-jp] unavailable (STAC no scenes)\n");
    return -1;
  }

  int n = 0, i = 0;
  cJSON *f;
  cJSON_ArrayForEach(f, feats) {
    cJSON *geom = cJSON_GetObjectItem(f, "geometry");
    cJSON *props = cJSON_GetObjectItem(f, "properties");
    const char *acquired = sstr(props, "datetime");
    cJSON *cc = props ? cJSON_GetObjectItem(props, "eo:cloud_cover") : NULL;
    int has_cloud = cc && cJSON_IsNumber(cc);
    double cloud = has_cloud ? cc->valuedouble : 0;

    char sidbuf[32];
    const char *sid = sstr(f, "id");
    if (!sid) { snprintf(sidbuf, sizeof sidbuf, "scene-%d", i); sid = sidbuf; }

    const char *thumb = NULL;
    cJSON *assets = cJSON_GetObjectItem(f, "assets");
    if (assets) {
      cJSON *rp = cJSON_GetObjectItem(assets, "rendered_preview");
      thumb = sstr(rp, "href");
      if (!thumb) {
        cJSON *tn = cJSON_GetObjectItem(assets, "thumbnail");
        thumb = sstr(tn, "href");
      }
    }

    char title[128], summary[256], bodytxt[512];
    snprintf(title, sizeof title, "Landsat scene %s", sid);
    if (has_cloud)
      snprintf(summary, sizeof summary,
        "Landsat C2 L2 acquired %s, %g%% cloud over Japan.",
        acquired ? acquired : "unknown", cloud);
    else
      snprintf(summary, sizeof summary,
        "Landsat C2 L2 acquired %s over Japan.",
        acquired ? acquired : "unknown");
    if (has_cloud)
      snprintf(bodytxt, sizeof bodytxt,
        "USGS Landsat Collection 2 Level-2 surface-reflectance scene %s "
        "intersecting the Japan AOI. Acquired %s. Cloud cover %g%%.",
        sid, acquired ? acquired : "unknown", cloud);
    else
      snprintf(bodytxt, sizeof bodytxt,
        "USGS Landsat Collection 2 Level-2 surface-reflectance scene %s "
        "intersecting the Japan AOI. Acquired %s. Cloud cover unknown.",
        sid, acquired ? acquired : "unknown");

    cJSON *p = cJSON_CreateObject();
    cJSON *bb = cJSON_GetObjectItem(f, "bbox");
    if (bb) cJSON_AddItemToObject(p, "bbox", cJSON_Duplicate(bb, 1));
    else {
      cJSON *jb = cJSON_CreateArray();
      cJSON_AddItemToArray(jb, cJSON_CreateNumber(122.0));
      cJSON_AddItemToArray(jb, cJSON_CreateNumber(24.0));
      cJSON_AddItemToArray(jb, cJSON_CreateNumber(146.0));
      cJSON_AddItemToArray(jb, cJSON_CreateNumber(46.0));
      cJSON_AddItemToObject(p, "bbox", jb);
    }
    cJSON_AddItemToObject(p, "geometry",
      geom ? cJSON_Duplicate(geom, 1) : cJSON_CreateNull());
    double cx, cy;
    if (centroid(geom, &cx, &cy)) {
      cJSON *cen = cJSON_CreateArray();
      cJSON_AddItemToArray(cen, cJSON_CreateNumber(cx));
      cJSON_AddItemToArray(cen, cJSON_CreateNumber(cy));
      cJSON_AddItemToObject(p, "centroid", cen);
    } else cJSON_AddNullToObject(p, "centroid");
    if (acquired) cJSON_AddStringToObject(p, "acquired", acquired);
    else cJSON_AddNullToObject(p, "acquired");
    if (has_cloud) cJSON_AddNumberToObject(p, "cloud_cover", cloud);
    else cJSON_AddNullToObject(p, "cloud_cover");
    /* sensor = instruments.join(',') || 'OLI/TIRS' */
    cJSON *instr = props ? cJSON_GetObjectItem(props, "instruments") : NULL;
    if (instr && cJSON_IsArray(instr) && cJSON_GetArraySize(instr) > 0) {
      char sbuf[128]; sbuf[0] = '\0';
      cJSON *ii; int first = 1;
      cJSON_ArrayForEach(ii, instr) {
        if (cJSON_IsString(ii)) {
          if (!first) strncat(sbuf, ",", sizeof sbuf - strlen(sbuf) - 1);
          strncat(sbuf, ii->valuestring, sizeof sbuf - strlen(sbuf) - 1);
          first = 0;
        }
      }
      cJSON_AddStringToObject(p, "sensor", sbuf[0] ? sbuf : "OLI/TIRS");
    } else cJSON_AddStringToObject(p, "sensor", "OLI/TIRS");
    const char *plat = sstr(props, "platform");
    cJSON_AddStringToObject(p, "platform", plat ? plat : "landsat");
    cJSON_AddStringToObject(p, "scene_id", sid);
    if (thumb) cJSON_AddStringToObject(p, "preview_url", thumb);
    else cJSON_AddNullToObject(p, "preview_url");
    char *pj = cJSON_PrintUnformatted(p);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("satellite"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("landsat"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("usgs"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("optical"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("raster"));
    char *tj = cJSON_PrintUnformatted(tags);

    intel_item it = {0};
    it.remote_key = sid;             /* uid landsat-jp|<sid> */
    it.title = title;
    it.summary = summary;
    it.body = bodytxt;
    it.link = "https://landsatlook.usgs.gov/";
    it.published_at = acquired;
    it.record_type = "landsat-jp";
    it.has_geo = 0;
    it.properties_json = pj;
    it.tags_json = tj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags);
    i++;
  }
  cJSON_Delete(data);
  fprintf(stderr, "[landsat-jp] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def landsat_jp_def = {
  .id = "landsat-jp", .collector = "satellite",
  .name = "Landsat Japan Coverage", .name_ja = "Landsat 日本域",
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(landsat_jp_def)
