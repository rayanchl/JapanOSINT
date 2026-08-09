/* lib/camfeature.c — see camfeature.h. Byte-for-byte the uid derivation and
 * fixed-property order the ported cam_*.c helpers use, so a Feature built here
 * merges against rows those channels already wrote. */
#include "camfeature.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static double round4(double v) { return floor(v * 1e4 + 0.5) / 1e4; }

/* Node was `(url || name || '').slice(0,60).toLowerCase()`, and the ported
 * helpers copied the 60-byte bound. That bound is a data-loss bug, not a
 * fidelity requirement: aggregator URLs share long path prefixes, so cameras in
 * one city collapse onto the same key and overwrite each other in the camera
 * keyspace. cam_skylinewebcams.c measured it directly — "101 cameras scraped,
 * 36 stored" — and fixed it locally with a shortened prefix plus a hash of the
 * WHOLE source string. That fix belongs here, in the canonical constructor, not
 * in one of fifteen copies.
 *
 * Shared-prefix exposure at 60 bytes, measured against the live collectors:
 *   worldcam_eu   46 chars shared   webcamera24  40   scs_com_ua  40
 *
 * 44-char readable prefix + '#' + FNV-1a-64 of the full string. Still stable
 * across runs (no randomness, no time), still human-inspectable, and now
 * collision-free for any two distinct sources. The prefix is cut on a UTF-8
 * boundary so a Japanese name can never be split mid-codepoint.
 *
 * This re-keys existing camera rows once, on the next run of each channel.
 * That is the intended trade: the old keys were merging distinct cameras. */
static void uid_tail(const char *url, const char *name, char *out,
                     size_t outsz) {
  const char *src = (url && *url) ? url : (name ? name : "");
  unsigned long long h = 1469598103934665603ULL;
  for (const char *p = src; *p; p++) {
    h ^= (unsigned char)*p;
    h *= 1099511628211ULL;
  }
  size_t i = 0;
  for (; src[i] && i < 44 && i + 18 < outsz; i++) {
    unsigned char c = (unsigned char)src[i];
    out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
  }
  /* never split a multi-byte sequence */
  while (i > 0 && ((unsigned char)out[i - 1] & 0xC0) == 0x80) i--;
  out[i] = 0;
  snprintf(out + i, outsz - i, "#%016llx", h);
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
