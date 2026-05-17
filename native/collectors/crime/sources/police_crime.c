/* collectors/crime/sources/police_crime.c — port of
 * server/src/collectors/policeCrime.js. Primary live path = tryLive()
 * Overpass amenity=police; the synthetic generateSeedData incident scatter is
 * intentionally not ported (rule 7). No registry row for "police-crime";
 * category derived as "crime" from the collector domain (cf. the related
 * pref-police-crime registry row, category "crime"). */
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
  char iid[32];
  snprintf(iid, sizeof iid, "POL_LIVE_%05d", i + 1);
  cJSON_AddStringToObject(p, "incident_id", iid);
  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "area", nm);
  } else {
    char dn[48];
    snprintf(dn, sizeof dn, "Police %lld", oid);
    cJSON_AddStringToObject(p, "area", dn);
  }
  const char *st = ov_tag(el, "addr:state");
  if (st) cJSON_AddStringToObject(p, "pref", st);
  else cJSON_AddItemToObject(p, "pref", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "incident_type", "police_station");
  cJSON_AddStringToObject(p, "severity", "info");
  const char *op = ov_tag(el, "operator");
  if (op) cJSON_AddStringToObject(p, "operator", op);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
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
