/* collectors/transport/sources/overpass_subway_tracks.c — port of
 * server/src/collectors/overpassSubwayTracks.js (fetchOverpassWaysTiled,
 * {queryTimeout:240,timeoutMs:180_000}).
 * OSM subway / tram / monorail tracks → LineString for the unified-subways
 * layer. No source_registry.gen.c row — internal layer feed.
 * No SEED / _meta envelope. */
#include "../../source.h"
#include "../../lib/overpass.h"
#include "../../lib/linecolor.h"
#include <stdio.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  (void)ud;
  snprintf(o, n,
    "way[\"railway\"=\"subway\"](%s);"
    "way[\"railway\"=\"tram\"](%s);"
    "way[\"railway\"=\"monorail\"](%s);",
    bbox, bbox, bbox);
}

/* coords toolkit-owned; DUPLICATE into geometry. Properties in EXACT JS
 * key order. */
static cJSON *map(cJSON *el, int i, cJSON *coords, void *ud) {
  (void)i; (void)ud;
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "LineString");
  cJSON_AddItemToObject(g, "coordinates", cJSON_Duplicate(coords, 1));
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();
  cJSON *idv = cJSON_GetObjectItem(el, "id");
  char lid[64];
  snprintf(lid, sizeof lid, "OSM_WAY_%lld",
           idv && cJSON_IsNumber(idv) ? (long long)idv->valuedouble : 0);
  cJSON_AddStringToObject(p, "line_id", lid);

  const char *nm = ov_tag(el, "name:en"); if (!nm) nm = ov_tag(el, "name");
  cJSON_AddItemToObject(p, "name", nm ? cJSON_CreateString(nm) : cJSON_CreateNull());
  const char *nj = ov_tag(el, "name"); if (!nj) nj = ov_tag(el, "name:ja");
  cJSON_AddItemToObject(p, "name_ja", nj ? cJSON_CreateString(nj) : cJSON_CreateNull());
  const char *op = ov_tag(el, "operator"); if (!op) op = ov_tag(el, "network");
  cJSON_AddItemToObject(p, "operator", op ? cJSON_CreateString(op) : cJSON_CreateNull());
  const char *rw = ov_tag(el, "railway");
  cJSON_AddItemToObject(p, "railway", rw ? cJSON_CreateString(rw) : cJSON_CreateNull());
  const char *rf = ov_tag(el, "ref");
  cJSON_AddItemToObject(p, "line_ref", rf ? cJSON_CreateString(rf) : cJSON_CreateNull());
  const char *co = ov_tag(el, "colour");
  cJSON_AddItemToObject(p, "colour", co ? cJSON_CreateString(co) : cJSON_CreateNull());
  char lc[8];
  cJSON_AddItemToObject(p, "line_color",
    line_color(cJSON_GetObjectItem(el, "tags"), lc) ? cJSON_CreateString(lc)
                                                    : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass_subway_track");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_ways_collect(ctx, sink, body, 240, 180000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def overpass_subway_tracks_def = {
  .id = "overpass-subway-tracks", .collector = "transport",
  .name = "OSM Subway Tracks", .name_ja = "OSM 地下鉄線路",
   .update_interval_sec = 86400, .run = run,
  .category = "transport" };
REGISTER_SOURCE(overpass_subway_tracks_def)
