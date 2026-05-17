/* collectors/marketplace/sources/job_boards.c — port of
 * server/src/collectors/jobBoards.js (fetchOverpass single area.jp).
 * Curated JOB_AREAS offline fallback intentionally not ported (rule 8).
 * `updated_at`-free; `source` is job_boards_live. */
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
  char lb[32];
  snprintf(lb, sizeof lb, "JOB_LIVE_%05d", i + 1);
  cJSON_AddStringToObject(p, "listing_id", lb);

  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "area", nm);
  } else {
    char ab[40];
    snprintf(ab, sizeof ab, "HelloWork %lld", oid);
    cJSON_AddStringToObject(p, "area", ab);
  }

  const char *st = ov_tag(el, "addr:state");
  cJSON_AddItemToObject(p, "pref",
                        st ? cJSON_CreateString(st) : cJSON_CreateNull());

  cJSON_AddStringToObject(p, "job_type", "employment_agency");
  cJSON_AddStringToObject(p, "industry", "office");

  const char *op = ov_tag(el, "operator");
  cJSON_AddStringToObject(p, "operator", op ? op : "HelloWork");

  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "job_boards_live");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_collect(ctx, sink,
    "node[\"office\"=\"employment_agency\"](area.jp);"
    "way[\"office\"=\"employment_agency\"](area.jp);",
    180, 60000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def job_boards_def = {
  .id = "job-boards", .collector = "marketplace",
  .name = "TownWork / Baitoru / Indeed",
  .name_ja = "タウンワーク・バイトル・Indeed",
   .update_interval_sec = 3600, .run = run };
REGISTER_SOURCE(job_boards_def)
