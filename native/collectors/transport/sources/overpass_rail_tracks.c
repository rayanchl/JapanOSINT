/* collectors/transport/sources/overpass_rail_tracks.c
 * Port of server/src/collectors/overpassRailTracks.js (fetchOverpassWaysTiled).
 * OSM rail tracks → LineString features for the unified-trains layer.
 * REFERENCE source.c for the OVERPASS_WAYS + line_color family.
 * (No source_registry.gen.c row — internal unified-trains layer feed.) */
#include "../../../source.h"
#include "../../../lib/overpass.h"
#include "../../../lib/linecolor.h"
#include <stdio.h>

static void body(const char *bbox, char *o, size_t n, void *ud) {
  snprintf(o, n,
    "way[\"railway\"=\"rail\"](%s);way[\"railway\"=\"light_rail\"](%s);"
    "way[\"railway\"=\"narrow_gauge\"](%s);", bbox, bbox, bbox);
}

/* coords is a cJSON array of [lon,lat]; toolkit frees it after we return, so
 * DUPLICATE into the geometry. Properties in EXACT JS key order. */
static cJSON *map(cJSON *el, int i, cJSON *coords, void *ud) {
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
  const char *el2 = ov_tag(el, "electrified");
  cJSON_AddItemToObject(p, "electrified", el2 ? cJSON_CreateString(el2) : cJSON_CreateNull());
  const char *gg = ov_tag(el, "gauge");
  cJSON_AddItemToObject(p, "gauge", gg ? cJSON_CreateString(gg) : cJSON_CreateNull());
  const char *us = ov_tag(el, "usage");
  cJSON_AddItemToObject(p, "usage", us ? cJSON_CreateString(us) : cJSON_CreateNull());
  const char *co = ov_tag(el, "colour");
  cJSON_AddItemToObject(p, "colour", co ? cJSON_CreateString(co) : cJSON_CreateNull());
  const char *rf = ov_tag(el, "ref");
  cJSON_AddItemToObject(p, "line_ref", rf ? cJSON_CreateString(rf) : cJSON_CreateNull());
  char lc[8];
  cJSON_AddItemToObject(p, "line_color",
    line_color(cJSON_GetObjectItem(el, "tags"), lc) ? cJSON_CreateString(lc)
                                                    : cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass_rail_track");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_ways_collect(ctx, sink, body, 240, 180000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def overpass_rail_tracks_def = {
  .id = "overpass-rail-tracks", .collector = "transport",
  .name = "OSM Rail Tracks", .name_ja = "OSM 鉄道線路",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(overpass_rail_tracks_def)
