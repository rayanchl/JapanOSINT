/* lib/camfeature.h — the ONE camera-Feature constructor.
 *
 * Every camera channel must hand camera_upsert() a Feature with the same shape,
 * because the store keys on properties.camera_uid and merges on the fixed
 * property set. That shape was copy-pasted into each cam_*.c during the port
 * (16 near-identical make_feature()s, drifting in whitespace and in which extra
 * value types they support), so a new channel had no canonical thing to copy.
 * This is that canonical thing.
 *
 * makeFeature parity with cameraDiscovery.js:48-68:
 *   uid        = "<lat rounded to 4dp>:<lon rounded to 4dp>:<tail>"
 *   tail       = first 60 chars of the "url" extra if present else `name`,
 *                lowercased (ASCII only, exactly as the JS toLowerCase on
 *                these host/name strings)
 *   properties = camera_uid, name, camera_type, discovery_channel, country,
 *                then the extras IN THE ORDER GIVEN
 *
 * The existing fourteen cam_*.c keep their local copies — they already emit
 * correct rows, and cam_camstreamer's variant carries two extra location-
 * precision properties this signature deliberately does not model. New
 * channels, and the ones converted since, use this. */
#ifndef JO_CAMFEATURE_H
#define JO_CAMFEATURE_H

#include "../third_party/cJSON.h"

/* One extra property. Exactly one of the type flags may be set; with none set
 * the value is `sv` (and a NULL `sv` emits JSON null, matching the JS `|| null`
 * chains the channels use). */
typedef struct {
  const char *k;      /* property key                                        */
  const char *sv;     /* string value (default type)                         */
  int    is_num; double nv;
  int    is_bool; int bv;
  int    is_null;     /* force JSON null regardless of sv                    */
} cam_kv;

/* Build the Feature. Caller owns the returned cJSON and frees it (camera_upsert
 * borrows). `name` NULL/empty → "Unknown camera"; `camera_type` NULL/empty →
 * "unknown"; country is always "JP". */
cJSON *cam_make_feature(double lat, double lon, const char *name,
                        const char *camera_type, const char *discovery_channel,
                        const cam_kv *extra, int nextra);

#endif
