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
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Charging station %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "name", name);
  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "unknown");
  const char *addr = ov_tag(el, "addr:full");
  if (!addr) addr = ov_tag(el, "addr:street");
  cJSON_AddStringToObject(p, "address", addr ? addr : "");

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
  cJSON_AddStringToObject(p, "network", net ? net : "unknown");
  cJSON_AddStringToObject(p, "status", "operational");
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
