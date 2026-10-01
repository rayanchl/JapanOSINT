/* collectors/tourism/sources/racetracks.c — port of
 * server/src/collectors/racetracks.js. fetchOverpass (single area.jp query,
 * tryLive). SEED_TRACKS offline fallback intentionally not ported (JS does
 * `if (!live) features = []`). */
#include "lib/geojson.h"
#include "source.h"
#include "lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();                   /* EXACT JS key order */
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char tid[64];
  snprintf(tid, sizeof tid, "OSM_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "track_id", tid);
  const char *name = ov_tag(el, "name");
  /* no-fabrication (house rule 1): OSM carried no name tag for this element.
   * The old code wrote "Track %d" + the loop index, which is both an invented
   * label and an UNSTABLE one — it feeds geojson's content-hash uid, so the
   * same object was re-keyed whenever Overpass changed element order. An
   * absent name is serialized as null; pick_text() skips nulls, so the row
   * persists with a NULL title rather than a made-up one. */
  if (name) cJSON_AddStringToObject(p, "name", name);
  else cJSON_AddItemToObject(p, "name", cJSON_CreateNull());
  const char *sport = ov_tag(el, "sport");
  cJSON_AddStringToObject(p, "sport", sport ? sport : "racing");
  const char *operator_ = ov_tag(el, "operator");
  if (operator_) cJSON_AddStringToObject(p, "operator", operator_);
  else cJSON_AddItemToObject(p, "operator", cJSON_CreateNull());
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"leisure\"=\"track\"][\"sport\"~\"horse_racing|cycling|motor|motorboat\"](area.jp);"
    "way[\"leisure\"=\"track\"][\"sport\"~\"horse_racing|cycling|motor|motorboat\"](area.jp);"
    "node[\"leisure\"=\"track\"][\"name\"~\"競馬|競輪|競艇|オート\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def racetracks_def = {
  .id = "racetracks", .collector = "tourism", .name = "Racetracks",
  .name_ja = "競馬・競輪・競艇・オート場", 
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(racetracks_def)
