/* collectors/infrastructure/sources/public_cameras.c — port of
 * server/src/collectors/publicCameras.js. Primary live path = tryLive()
 * Overpass man_made=surveillance; the curated CAMERAS seed list is
 * intentionally not ported (rule 7). No registry row for "public-cameras";
 * category derived as "infrastructure" (cf. public-webcams / traffic-cameras
 * registry rows, category "infrastructure"). */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();
  char cid[32];
  snprintf(cid, sizeof cid, "CAM_LIVE_%05d", i + 1);
  cJSON_AddStringToObject(p, "camera_id", cid);
  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "name", nm);
  } else {
    char dn[48];
    snprintf(dn, sizeof dn, "Camera %lld", oid);
    cJSON_AddStringToObject(p, "name", dn);
  }
  const char *sv = ov_tag(el, "surveillance");
  cJSON_AddStringToObject(p, "type", sv ? sv : "public");
  const char *svt = ov_tag(el, "surveillance:type");
  if (svt) cJSON_AddStringToObject(p, "surveillance_type", svt);
  else cJSON_AddItemToObject(p, "surveillance_type", cJSON_CreateNull());
  const char *op = ov_tag(el, "operator");
  if (op) cJSON_AddStringToObject(p, "operator", op);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  const char *url = ov_tag(el, "url");
  if (url) cJSON_AddStringToObject(p, "url", url);
  else cJSON_AddItemToObject(p, "url", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "public_cameras_live");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"man_made\"=\"surveillance\"]"
    "[\"surveillance\"~\"public|traffic|webcam\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def public_cameras_def = {
  .id = "public-cameras", .collector = "infrastructure",
  .name = "Public Cameras", .name_ja = "公開カメラ",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(public_cameras_def)
