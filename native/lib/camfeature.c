/* lib/camfeature.c — see camfeature.h. Byte-for-byte the uid derivation and
 * fixed-property order the ported cam_*.c helpers use, so a Feature built here
 * merges against rows those channels already wrote. */
#include "camfeature.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static double round4(double v) { return floor(v * 1e4 + 0.5) / 1e4; }

/* JS: (url || name || '').slice(0,60).toLowerCase(). ASCII-only lowering, which
 * is what the JS does for these strings in practice — a multi-byte UTF-8 name
 * passes through untouched, and the 60-char bound is on bytes, exactly as the
 * ported helpers already bound it. */
static void uid_tail(const char *url, const char *name, char *out,
                     size_t outsz) {
  const char *src = (url && *url) ? url : (name ? name : "");
  size_t i = 0;
  for (; src[i] && i < 60 && i + 1 < outsz; i++) {
    unsigned char c = (unsigned char)src[i];
    out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
  }
  out[i] = 0;
}

cJSON *cam_make_feature(double lat, double lon, const char *name,
                        const char *camera_type, const char *discovery_channel,
                        const cam_kv *extra, int nextra) {
  /* uid tail prefers the "url" extra when it carries a real string. */
  const char *url = NULL;
  for (int i = 0; i < nextra; i++)
    if (strcmp(extra[i].k, "url") == 0 && !extra[i].is_null && extra[i].sv) {
      url = extra[i].sv; break;
    }

  char lats[32], lons[32], tail[80], uid[160];
  snprintf(lats, sizeof lats, "%.4f", round4(lat));
  snprintf(lons, sizeof lons, "%.4f", round4(lon));
  uid_tail(url, name, tail, sizeof tail);
  snprintf(uid, sizeof uid, "%s:%s:%s", lats, lons, tail);

  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");

  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *co = cJSON_CreateArray();
  cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", co);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();
  cJSON_AddStringToObject(p, "camera_uid", uid);
  cJSON_AddStringToObject(p, "name", (name && *name) ? name : "Unknown camera");
  cJSON_AddStringToObject(p, "camera_type",
                          (camera_type && *camera_type) ? camera_type
                                                        : "unknown");
  cJSON_AddStringToObject(p, "discovery_channel",
                          discovery_channel ? discovery_channel : "unknown");
  cJSON_AddStringToObject(p, "country", "JP");
  for (int i = 0; i < nextra; i++) {
    const cam_kv *e = &extra[i];
    if (e->is_null)      cJSON_AddNullToObject(p, e->k);
    else if (e->is_bool) cJSON_AddBoolToObject(p, e->k, e->bv);
    else if (e->is_num)  cJSON_AddNumberToObject(p, e->k, e->nv);
    else if (e->sv)      cJSON_AddStringToObject(p, e->k, e->sv);
    else                 cJSON_AddNullToObject(p, e->k);
  }
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}
