/* collectors/marketplace/sources/job_boards.c — port of
 * server/src/collectors/jobBoards.js (fetchOverpass single area.jp).
 * Curated JOB_AREAS offline fallback intentionally not ported (rule 8).
 * `updated_at`-free; `source` is job_boards_live. */
#include "../../lib/geojson.h"
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  (void)ud;
  cJSON *f = gj_point_feature(lon, lat);

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  long long oid = id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0;
  const char *ety = NULL;
  { cJSON *t = cJSON_GetObjectItem(el, "type");
    if (t && cJSON_IsString(t)) ety = t->valuestring; }

  /* The OSM element id is the stable natural key. The old JOB_LIVE_%05d
   * index label made the uid depend on Overpass' result ORDER, so a
   * re-ordered response re-inserted every row as new. */
  char lb[64];
  if (oid) snprintf(lb, sizeof lb, "%s/%lld", ety ? ety : "node", oid);
  else     snprintf(lb, sizeof lb, "JOB_LIVE_%05d", i + 1);
  cJSON_AddStringToObject(p, "uid", lb);
  cJSON_AddStringToObject(p, "listing_id", lb);

  const char *nm = ov_tag(el, "name");
  if (!nm) nm = ov_tag(el, "name:en");
  if (nm) {
    cJSON_AddStringToObject(p, "area", nm);
  } else {
    char ab[40];
    snprintf(ab, sizeof ab, "HelloWork %lld", oid);
    cJSON_AddStringToObject(p, "area", ab);
  }
  /* Without one of title|name|name_ja|label lib/geojson.c emits a NULL title
   * and the row is unreadable in every UI (this was the NO_TITLE case). */
  {
    char tb[200];
    snprintf(tb, sizeof tb, "%s (employment agency)",
             nm ? nm : "HelloWork employment agency");
    cJSON_AddStringToObject(p, "title", tb);
  }
  if (oid) {
    char ub[128];
    snprintf(ub, sizeof ub, "https://www.openstreetmap.org/%s/%lld",
             ety ? ety : "node", oid);
    cJSON_AddStringToObject(p, "url", ub);
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
