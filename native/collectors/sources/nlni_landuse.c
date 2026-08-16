/* collectors/geospatial/sources/nlni_landuse.c
 * Port of server/src/collectors/nlniLanduse.js — MLIT KSJ L03-b national
 * land-use segmented mesh. No official GeoJSON endpoint and MIRRORS=[] in
 * Node, so the ONLY source is the optional env var MLIT_L03B_GEOJSON_URL
 * (Node: envFor → native: getenv). Without it (or on failure / no usable
 * geometry) → honest empty (rule 8 — no fabricated land-use polygons).
 * Property key order (mesh_id, landuse_code, landuse, source) mirrors JS. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* L03-b 土地利用種別 code → label (Node LANDUSE_LABELS). */
static const char *landuse_label(const char *code) {
  if (!code) return NULL;
  struct { const char *c, *l; } M[] = {
    {"0100","田"}, {"0200","その他の農用地"}, {"0500","森林"},
    {"0600","荒地"}, {"0700","建物用地"}, {"0901","道路"},
    {"0902","鉄道"}, {"1000","その他の用地"}, {"1100","河川地及び湖沼"},
    {"1400","海浜"}, {"1500","海水域"}, {"1600","ゴルフ場"},
  };
  for (size_t i = 0; i < sizeof M / sizeof M[0]; i++)
    if (strcmp(M[i].c, code) == 0) return M[i].l;
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = getenv("MLIT_L03B_GEOJSON_URL");
  if (!url || !*url) {
    /* Gated, not failed: -1 logs fetch_log status='error' and opens a
     * collector_anomaly every tick for a source that was never configured. */
    fprintf(stderr, "[nlni-landuse] gated (no MLIT_L03B_GEOJSON_URL)\n");
    return 0;
  }

  cJSON *data = feed_get_json(ctx->http, url, 30000);
  if (!data) { fprintf(stderr, "[nlni-landuse] unavailable\n"); return -1; }
  cJSON *src = cJSON_GetObjectItem(data, "features");
  if (!src || !cJSON_IsArray(src)) {
    cJSON_Delete(data);
    fprintf(stderr, "[nlni-landuse] unavailable (no features)\n");
    return -1;
  }

  cJSON *features = cJSON_CreateArray();
  cJSON *f;
  cJSON_ArrayForEach(f, src) {
    cJSON *geom = cJSON_GetObjectItem(f, "geometry");
    if (!geom) continue;
    if (!cJSON_GetObjectItem(geom, "type")) continue;
    if (!cJSON_GetObjectItem(geom, "coordinates")) continue;
    cJSON *p = cJSON_GetObjectItem(f, "properties");

    const char *code = NULL;
    if (p) {
      code = jo_str(p, "landuse");
      if (!code) code = jo_str(p, "L03b_002");
      if (!code) code = jo_str(p, "L03b_001");
      if (!code) code = jo_str(p, "code");
    }
    char codebuf[32] = {0};
    if (code) {
      /* trim */
      const char *s = code; while (*s == ' ') s++;
      snprintf(codebuf, sizeof codebuf, "%s", s);
      size_t L = strlen(codebuf);
      while (L && codebuf[L-1] == ' ') codebuf[--L] = 0;
    }

    const char *mesh = p ? jo_str(p, "mesh") : NULL;
    if (!mesh && p) mesh = jo_str(p, "L03b_001");
    if (!mesh && p) mesh = jo_str(p, "id");
    char meshbuf[64];
    if (!mesh) {
      char *gs = cJSON_PrintUnformatted(geom);
      char hk[24];
      const char *parts[] = { gs ? gs : "" };
      feed_hash_key(hk, parts, 1);
      snprintf(meshbuf, sizeof meshbuf, "L03B_%s", hk);
      mesh = meshbuf;
      free(gs);
    }

    const char *label = landuse_label(codebuf[0] ? codebuf : NULL);
    if (!label && p) label = jo_str(p, "landuse_name");

    cJSON *nf = cJSON_CreateObject();
    cJSON_AddStringToObject(nf, "type", "Feature");
    cJSON_AddItemToObject(nf, "geometry", cJSON_Duplicate(geom, 1));
    cJSON *np = cJSON_CreateObject();             /* EXACT JS key order */
    cJSON_AddStringToObject(np, "mesh_id", mesh);
    if (codebuf[0]) cJSON_AddStringToObject(np, "landuse_code", codebuf);
    else            cJSON_AddNullToObject(np, "landuse_code");
    if (label) cJSON_AddStringToObject(np, "landuse", label);
    else       cJSON_AddNullToObject(np, "landuse");
    cJSON_AddStringToObject(np, "source", "mlit_l03b");
    cJSON_AddItemToObject(nf, "properties", np);
    cJSON_AddItemToArray(features, nf);
  }
  cJSON_Delete(data);

  if (cJSON_GetArraySize(features) == 0) {
    cJSON_Delete(features);
    fprintf(stderr, "[nlni-landuse] unavailable (no usable geometry)\n");
    return -1;
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[nlni-landuse] emitted %d\n", n);
  /* run() is a STATUS code, not a row count: fetch/parse failures already
   * returned -1 above, so reaching here with zero rows is an honest empty.
   * Returning -1 here had scheduler.c quarantine the source for working. */
  return 0;
}

static const source_def nlni_landuse_def = {
  .id = "nlni-landuse", .collector = "geospatial",
  .name = "National Land Numerical Info", .name_ja = "国土数値情報 土地利用",
  .update_interval_sec = 604800, .run = run };
REGISTER_SOURCE(nlni_landuse_def)
