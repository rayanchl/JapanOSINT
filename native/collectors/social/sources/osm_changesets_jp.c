/* collectors/social/sources/osm_changesets_jp.c
 * Port of server/src/collectors/osmChangesetsJp.js.
 * OSM changesets.json within JP bbox (closed, limit 100) → FeatureCollection.
 * Keyless. Error/empty branch returns 0 features (correctness-neutral). */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define API_URL "https://api.openstreetmap.org/api/0.6/changesets.json?bbox=122.0,24.0,153.0,46.0&closed=true&limit=100"
#define TIMEOUT_MS 15000

/* JS Number(x): numbers pass through, strings coerced, missing/null → NaN. */
static double num(cJSON *v, int *ok) {
  if (!v) { *ok = 0; return 0; }
  if (cJSON_IsNumber(v)) { *ok = 1; return v->valuedouble; }
  if (cJSON_IsString(v) && v->valuestring[0]) {
    char *e; double d = strtod(v->valuestring, &e);
    if (e != v->valuestring) { *ok = 1; return d; }
  }
  *ok = 0;
  return 0;
}

/* a?.tags?.x || null  (string) */
static void tag_or_null(cJSON *p, const char *k, cJSON *tags, const char *ik) {
  cJSON *v = tags ? cJSON_GetObjectItem(tags, ik) : NULL;
  if (v && cJSON_IsString(v) && v->valuestring[0])
    cJSON_AddStringToObject(p, k, v->valuestring);
  else
    cJSON_AddItemToObject(p, k, cJSON_CreateNull());
}

static void passthru(cJSON *p, const char *k, cJSON *cs, const char *ik) {
  cJSON *v = cJSON_GetObjectItem(cs, ik);
  cJSON_AddItemToObject(p, k, v ? cJSON_Duplicate(v, 1) : cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *hdrs[] = { "accept: application/json",
                         "user-agent: japanosint-collector", NULL };
  cJSON *json = feed_get_json_h(ctx->http, API_URL, hdrs, TIMEOUT_MS);
  cJSON *arr = json ? cJSON_GetObjectItem(json, "changesets") : NULL;

  cJSON *features = cJSON_CreateArray();
  if (cJSON_IsArray(arr)) {
    int i = 0;
    cJSON *cs;
    cJSON_ArrayForEach(cs, arr) {
      int ok1, ok2, ok3, ok4;
      double mnlon = num(cJSON_GetObjectItem(cs, "min_lon"), &ok1);
      double mxlon = num(cJSON_GetObjectItem(cs, "max_lon"), &ok2);
      double mnlat = num(cJSON_GetObjectItem(cs, "min_lat"), &ok3);
      double mxlat = num(cJSON_GetObjectItem(cs, "max_lat"), &ok4);
      double lon = (mnlon + mxlon) / 2.0;
      double lat = (mnlat + mxlat) / 2.0;
      int finite = isfinite(lon) && isfinite(lat) &&
                   (ok1 && ok2 && ok3 && ok4);

      cJSON *f = cJSON_CreateObject();
      cJSON_AddStringToObject(f, "type", "Feature");
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(finite ? lon : 139.6917));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(finite ? lat : 35.6895));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);

      cJSON *tags = cJSON_GetObjectItem(cs, "tags");
      cJSON *p = cJSON_CreateObject();             /* EXACT JS key order */
      cJSON_AddNumberToObject(p, "idx", i);
      passthru(p, "cs_id", cs, "id");
      passthru(p, "user", cs, "user");
      passthru(p, "uid", cs, "uid");
      passthru(p, "created_at", cs, "created_at");
      passthru(p, "closed_at", cs, "closed_at");
      tag_or_null(p, "comment", tags, "comment");
      tag_or_null(p, "created_by", tags, "created_by");
      tag_or_null(p, "source_tag", tags, "source");
      passthru(p, "changes_count", cs, "changes_count");
      cJSON *bb = cJSON_CreateArray();
      cJSON *bx;
      bx = cJSON_GetObjectItem(cs, "min_lon");
      cJSON_AddItemToArray(bb, bx ? cJSON_Duplicate(bx, 1) : cJSON_CreateNull());
      bx = cJSON_GetObjectItem(cs, "min_lat");
      cJSON_AddItemToArray(bb, bx ? cJSON_Duplicate(bx, 1) : cJSON_CreateNull());
      bx = cJSON_GetObjectItem(cs, "max_lon");
      cJSON_AddItemToArray(bb, bx ? cJSON_Duplicate(bx, 1) : cJSON_CreateNull());
      bx = cJSON_GetObjectItem(cs, "max_lat");
      cJSON_AddItemToArray(bb, bx ? cJSON_Duplicate(bx, 1) : cJSON_CreateNull());
      cJSON_AddItemToObject(p, "bbox", bb);
      cJSON *idv = cJSON_GetObjectItem(cs, "id");
      if (idv && !cJSON_IsNull(idv)) {
        char ub[128];
        if (cJSON_IsNumber(idv))
          snprintf(ub, sizeof ub,
            "https://www.openstreetmap.org/changeset/%.0f", idv->valuedouble);
        else
          snprintf(ub, sizeof ub,
            "https://www.openstreetmap.org/changeset/%s",
            cJSON_IsString(idv) ? idv->valuestring : "");
        cJSON_AddStringToObject(p, "url", ub);
      } else {
        cJSON_AddItemToObject(p, "url", cJSON_CreateNull());
      }
      cJSON_AddStringToObject(p, "source", "osm_changesets_jp");
      cJSON_AddItemToObject(f, "properties", p);
      cJSON_AddItemToArray(features, f);
      i++;
    }
  }
  if (json) cJSON_Delete(json);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[osm-changesets-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def osm_changesets_jp_def = {
  .id = "osm-changesets-jp", .collector = "social",
  .name = "OSM changesets (JP)", .name_ja = "OSM チェンジセット 日本",
   .update_interval_sec = 1800, .run = run,
};
REGISTER_SOURCE(osm_changesets_jp_def)
