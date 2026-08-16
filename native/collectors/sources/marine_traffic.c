/* collectors/transport/sources/marine_traffic.c — port of
 * server/src/collectors/marineTraffic.js. The JS primary path is the
 * MarineTraffic Exportvessels REST API, gated on MARINETRAFFIC_API_KEY;
 * without a key it returns null and the collector falls through to an OSM
 * Overpass harbour-node proxy (tryOsmPorts, single area.jp query) which is
 * the faithful keyless live path. The curated SEED_PORTS / _meta envelope is
 * intentionally not ported (JS does `features = []` when nothing live). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char mid[64];
  snprintf(mid, sizeof mid, "MT_OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "id", mid);
  const char *name = ov_tag(el, "name");
  char nbuf[64];
  if (!name) { snprintf(nbuf, sizeof nbuf, "Harbour %d", i + 1); name = nbuf; }
  cJSON_AddStringToObject(p, "vessel_name", name);
  /* geojson pickText wants title|name|name_ja|label; "vessel_name" is none of
   * them, so all 577 harbour rows persisted with a NULL title. Mirror the
   * OSM-derived name into "name" — same value, readable row. */
  cJSON_AddStringToObject(p, "name", name);
  cJSON_AddStringToObject(p, "vessel_type", "harbour");
  if (id && cJSON_IsNumber(id)) {
    char lk[96];
    snprintf(lk, sizeof lk, "https://www.openstreetmap.org/node/%lld",
             (long long)id->valuedouble);
    cJSON_AddStringToObject(p, "link", lk);
  }
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass_port");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"seamark:type\"=\"harbour\"](area.jp);"
    "node[\"harbour\"=\"yes\"](area.jp);",
    /* 60s per endpoint could never succeed: ENDPOINTS[0] (overpass-api.de) is
     * unreachable and the first LIVE mirror answers a nationwide area.jp query
     * in ~100s. Budget must exceed that or the source is a guaranteed
     * timeout. */
    180, 150000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def marine_traffic_def = {
  .id = "marine-traffic", .collector = "transport",
  .name = "MarineTraffic AIS", .name_ja = "マリントラフィック AIS",
   .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(marine_traffic_def)
