/* collectors/infrastructure/sources/ev_charging.c — port of
 * server/src/collectors/evCharging.js. The JS merges OpenChargeMap (JSON API)
 * and OSM Overpass results; the OSM `amenity=charging_station` Overpass query
 * (tryOSMChargers, single area.jp) is the faithful keyless live path ported
 * here. The curated EV_CHARGERS seed / _meta envelope is intentionally not
 * ported (JS does `features = []` when nothing live). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tag_yes(cJSON *el, const char *k) {
  const char *v = ov_tag(el, k);
  return v && strcmp(v, "yes") == 0;
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char cid[64];
  snprintf(cid, sizeof cid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "charger_id", cid);
  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  /* no-fabrication (house rule 1): OSM carried no name tag for this element.
   * The old code wrote "Charging station %d" + the loop index, which is both an invented
   * label and an UNSTABLE one — it feeds geojson's content-hash uid, so the
   * same object was re-keyed whenever Overpass changed element order. An
   * absent name is serialized as null; pick_text() skips nulls, so the row
   * persists with a NULL title rather than a made-up one. */
  if (name) cJSON_AddStringToObject(p, "name", name);
  else cJSON_AddItemToObject(p, "name", cJSON_CreateNull());
  const char *op = ov_tag(el, "operator");
  if (op) cJSON_AddStringToObject(p, "operator", op);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  const char *addr = ov_tag(el, "addr:full");
  if (!addr) addr = ov_tag(el, "addr:street");
  if (addr) cJSON_AddStringToObject(p, "address", addr);
  else cJSON_AddItemToObject(p, "address", cJSON_CreateNull());

  cJSON *ct = cJSON_CreateArray();
  if (tag_yes(el, "socket:chademo"))
    cJSON_AddItemToArray(ct, cJSON_CreateString("CHAdeMO"));
  if (tag_yes(el, "socket:type2"))
    cJSON_AddItemToArray(ct, cJSON_CreateString("Type 2"));
  if (tag_yes(el, "socket:type2_combo"))
    cJSON_AddItemToArray(ct, cJSON_CreateString("CCS"));
  if (tag_yes(el, "socket:tesla_supercharger"))
    cJSON_AddItemToArray(ct, cJSON_CreateString("Tesla"));
  cJSON_AddItemToObject(p, "connector_types", ct);

  const char *outs = ov_tag(el, "charging_station:output");
  double outkw = outs ? strtod(outs, NULL) : 0;
  if (outkw) cJSON_AddNumberToObject(p, "power_kw", outkw);
  else cJSON_AddItemToObject(p, "power_kw", cJSON_CreateNull());
  const char *cap = ov_tag(el, "capacity");
  long np = cap ? strtol(cap, NULL, 10) : 0;
  if (np) cJSON_AddNumberToObject(p, "num_ports", (double)np);
  else cJSON_AddItemToObject(p, "num_ports", cJSON_CreateNull());
  cJSON_AddBoolToObject(p, "is_rapid", outs && outkw >= 50);
  const char *fee = ov_tag(el, "fee");
  cJSON_AddBoolToObject(p, "is_free", fee && strcmp(fee, "no") == 0);
  const char *net = ov_tag(el, "network");
  if (!net) net = ov_tag(el, "operator");
  if (net) cJSON_AddStringToObject(p, "network", net);
  else cJSON_AddItemToObject(p, "network", cJSON_CreateNull());
  /* no-fabrication (house rule 1): this read `status = "operational"` for
   * EVERY station, unconditionally. Overpass was never asked whether the
   * charger works, so the field asserted an operational state that had not
   * been observed — the map-of-healthy-fleet failure. OSM's own lifecycle tag
   * is reported when it is present, and null when it is not. */
  const char *opstatus = ov_tag(el, "operational_status");
  if (!opstatus) opstatus = ov_tag(el, "disused");
  if (opstatus) cJSON_AddStringToObject(p, "status", opstatus);
  else cJSON_AddItemToObject(p, "status", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"amenity\"=\"charging_station\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def ev_charging_def = {
  .id = "ev-charging", .collector = "infrastructure",
  .name = "EV Charging Stations", .name_ja = "EV充電スタンド",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(ev_charging_def)
