/* collectors/crime/sources/police_crime.c — port of
 * server/src/collectors/policeCrime.js. Primary live path = tryLive()
 * Overpass amenity=police; the synthetic generateSeedData incident scatter is
 * intentionally not ported (rule 7). No registry row for "police-crime";
 * category derived as "crime" from the collector domain (cf. the related
 * pref-police-crime registry row, category "crime"). */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  char iid[32];
  snprintf(iid, sizeof iid, "POL_LIVE_%05d", i + 1);
  cJSON_AddStringToObject(p, "incident_id", iid);
  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  /* Stable identity. incident_id is the ARRAY INDEX, and no other property is
   * a NATIVE_ID_KEY, so lib/geojson.c fell back to sha1({geometry,properties})
   * — which changes the moment Overpass returns the elements in a different
   * order, re-keying all ~9.9k rows. The OSM element id is the real identity.
   * (One-time re-key on deploy.) */
  char ouid[48];
  snprintf(ouid, sizeof ouid, "osm/%lld", oid);
  cJSON_AddStringToObject(p, "uid", ouid);
  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  char dn[64];
  if (!nm) { snprintf(dn, sizeof dn, "Police %lld", oid); nm = dn; }
  cJSON_AddStringToObject(p, "area", nm);
  /* lib/geojson.c reads title|name|name_ja|label off the properties, and this
   * feature carried the station name under "area" only — so all 9,903 rows
   * persisted with title=NULL (the NO_TITLE verdict). */
  cJSON_AddStringToObject(p, "title", nm);
  const char *nmja = ov_tag(el, "name:ja");
  if (nmja) cJSON_AddStringToObject(p, "name_ja", nmja);
  const char *st = ov_tag(el, "addr:state");
  if (st) cJSON_AddStringToObject(p, "pref", st);
  else cJSON_AddItemToObject(p, "pref", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "incident_type", "police_station");
  cJSON_AddStringToObject(p, "severity", "info");
  const char *op = ov_tag(el, "operator");
  if (op) cJSON_AddStringToObject(p, "operator", op);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  /* contact/address tags OSM already gave us and the port threw away */
  static const char *const EXTRA[] = { "addr:city", "addr:full", "phone",
                                       "website", "opening_hours", NULL };
  for (int k = 0; EXTRA[k]; k++) {
    const char *v = ov_tag(el, EXTRA[k]);
    if (v) cJSON_AddStringToObject(p, EXTRA[k], v);
  }
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "police_live");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"police\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def police_crime_def = {
  .id = "police-crime", .collector = "crime",
  .name = "Police Crime", .name_ja = "警察 犯罪統計",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(police_crime_def)
