/* collectors/tourism/sources/stadiums.c
 * Port of server/src/collectors/stadiums.js — PRIMARY tryOSMOverpass() path.
 * JS post-maps `.slice(0,200)`; ported UNCAPPED (slice cap dropped per port
 * rules). The curated SEED_STADIUMS fallback is intentionally not ported. */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <stdlib.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *idv = cJSON_GetObjectItem(el, "id");
  char sid[40];
  snprintf(sid, sizeof sid, "OSM_%lld",
           idv && cJSON_IsNumber(idv) ? (long long)idv->valuedouble : 0);
  cJSON_AddStringToObject(p, "stadium_id", sid);

  const char *name = ov_tag(el, "name");
  if (name) {
    cJSON_AddStringToObject(p, "name", name);
  } else {
    char nm[32];
    snprintf(nm, sizeof nm, "Stadium %d", i + 1);
    cJSON_AddStringToObject(p, "name", nm);
  }

  const char *sport = ov_tag(el, "sport");
  cJSON_AddStringToObject(p, "kind", sport ? sport : "multi");

  const char *cap = ov_tag(el, "capacity");
  long cv = cap ? strtol(cap, NULL, 10) : 0;
  cJSON_AddItemToObject(p, "capacity",
    cv > 0 ? cJSON_CreateNumber((double)cv) : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"leisure\"=\"stadium\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def stadiums_def = {
  .id = "stadiums", .collector = "tourism",
  .name = "Stadiums", .name_ja = "スタジアム",
   .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(stadiums_def)
