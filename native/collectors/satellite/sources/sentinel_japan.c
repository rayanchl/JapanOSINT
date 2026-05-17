/* collectors/satellite/sources/sentinel_japan.c
 * Port of server/src/collectors/sentinelJapan.js.
 * OAuth2 client-credentials (SENTINELHUB_CLIENT_ID/SENTINELHUB_CLIENT_SECRET)
 * → Sentinel Hub Catalog STAC search → one intel item per Sentinel-2 L2A
 * scene. Non-spatial scene catalog (has_geo=0). Gated on creds. Honest empty
 * on auth/fetch failure. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../third_party/cJSON.h"
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

/* centroid of first ring of Polygon / MultiPolygon */
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
  const char *cid = getenv("SENTINELHUB_CLIENT_ID");
  const char *csec = getenv("SENTINELHUB_CLIENT_SECRET");
  if (!cid || !*cid || !csec || !*csec) {
    fprintf(stderr, "[sentinel-japan] gated (no SENTINELHUB_CLIENT_ID/SECRET)\n");
    return 0;
  }

  char form[1024];
  snprintf(form, sizeof form,
    "grant_type=client_credentials&client_id=%s&client_secret=%s", cid, csec);
  const char *tok_hdr[] = {
    "Content-Type: application/x-www-form-urlencoded", NULL };
  cJSON *tokresp = feed_post_json(ctx->http,
    "https://services.sentinel-hub.com/oauth/token", form, tok_hdr, 12000);
  const char *token = tokresp ? sstr(tokresp, "access_token") : NULL;
  if (!token) {
    if (tokresp) cJSON_Delete(tokresp);
    fprintf(stderr, "[sentinel-japan] unavailable (OAuth token failed)\n");
    return -1;
  }
  char tkbuf[2048];
  snprintf(tkbuf, sizeof tkbuf, "%s", token);
  cJSON_Delete(tokresp);

  char from[32], to[32];
  iso_ago(from, sizeof from, 21);
  iso_now(to, sizeof to);
  char body[512];
  snprintf(body, sizeof body,
    "{\"bbox\":[122,24,146,46],\"datetime\":\"%s/%s\","
    "\"collections\":[\"sentinel-2-l2a\"],\"limit\":50,"
    "\"filter\":{\"op\":\"<=\",\"args\":[{\"property\":\"eo:cloud_cover\"},40]}}",
    from, to);

  char auth[2100];
  snprintf(auth, sizeof auth, "Authorization: Bearer %s", tkbuf);
  const char *hdrs[] = { "Content-Type: application/json", auth, NULL };
  cJSON *data = feed_post_json(ctx->http,
    "https://services.sentinel-hub.com/api/v1/catalog/1.0.0/search",
    body, hdrs, 15000);

  cJSON *feats = data ? cJSON_GetObjectItem(data, "features") : NULL;
  if (!feats || !cJSON_IsArray(feats) || cJSON_GetArraySize(feats) == 0) {
    if (data) cJSON_Delete(data);
    fprintf(stderr, "[sentinel-japan] unavailable (no scenes)\n");
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

    char title[128], summary[256], bodytxt[512];
    snprintf(title, sizeof title, "Sentinel-2 L2A scene %s", sid);
    if (has_cloud)
      snprintf(summary, sizeof summary,
        "Sentinel-2 L2A acquired %s, %g%% cloud over Japan.",
        acquired ? acquired : "unknown", cloud);
    else
      snprintf(summary, sizeof summary,
        "Sentinel-2 L2A acquired %s over Japan.",
        acquired ? acquired : "unknown");
    if (has_cloud)
      snprintf(bodytxt, sizeof bodytxt,
        "Copernicus Sentinel-2 Level-2A optical scene %s intersecting the "
        "Japan AOI. Acquired %s. Cloud cover %g%%.",
        sid, acquired ? acquired : "unknown", cloud);
    else
      snprintf(bodytxt, sizeof bodytxt,
        "Copernicus Sentinel-2 Level-2A optical scene %s intersecting the "
        "Japan AOI. Acquired %s. Cloud cover unknown.",
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
    cJSON_AddStringToObject(p, "sensor", "MSI");
    const char *plat = sstr(props, "platform");
    cJSON_AddStringToObject(p, "platform", plat ? plat : "sentinel-2");
    cJSON_AddStringToObject(p, "scene_id", sid);
    char *pj = cJSON_PrintUnformatted(p);

    cJSON *tags = cJSON_CreateArray();
    cJSON_AddItemToArray(tags, cJSON_CreateString("satellite"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("sentinel-2"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("copernicus"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("optical"));
    cJSON_AddItemToArray(tags, cJSON_CreateString("raster"));
    char *tj = cJSON_PrintUnformatted(tags);

    intel_item it = {0};
    it.remote_key = sid;             /* uid sentinel-japan|<sid> */
    it.title = title;
    it.summary = summary;
    it.body = bodytxt;
    it.link = "https://dataspace.copernicus.eu/";
    it.published_at = acquired;
    it.record_type = "sentinel-japan";
    it.has_geo = 0;
    it.properties_json = pj;
    it.tags_json = tj;
    if (sink->emit(sink, &it) >= 0) n++;

    free(pj); free(tj);
    cJSON_Delete(p); cJSON_Delete(tags);
    i++;
  }
  cJSON_Delete(data);
  fprintf(stderr, "[sentinel-japan] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def sentinel_japan_def = {
  .id = "sentinel-japan", .collector = "satellite",
  .name = "Sentinel Hub Japan", .name_ja = "Sentinel Hub 日本",
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(sentinel_japan_def)
