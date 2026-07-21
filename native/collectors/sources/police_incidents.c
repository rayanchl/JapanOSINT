/* collectors/safety/sources/police_incidents.c
 * Port of server/src/collectors/policeIncidents.js — prefectural police
 * incident export CSV. No national feed, so the ONLY source is the
 * optional env var POLICE_INCIDENTS_CSV_URL (Node: envFor → native:
 * getenv); CSV w/ defensive header names + Japan bbox filter. No
 * configured source / fetch fail / no rows → honest empty (rule 8).
 * Property key order (incident_type, date, place, source) mirrors JS. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/csv.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *cell(cJSON *row, const char *k) {
  cJSON *v = cJSON_GetObjectItem(row, k);
  if (!v || !cJSON_IsString(v)) return NULL;
  const char *s = v->valuestring;
  if (!s || !*s) return NULL;
  return s;
}
static int pick_num(cJSON *row, const char *const *keys, int nk, double *out) {
  for (int i = 0; i < nk; i++) {
    const char *s = cell(row, keys[i]);
    if (!s) continue;
    char *end; double n = strtod(s, &end);
    if (end != s) { *out = n; return 1; }
  }
  return 0;
}
static const char *pick_str(cJSON *row, const char *const *keys, int nk) {
  for (int i = 0; i < nk; i++) {
    const char *s = cell(row, keys[i]);
    if (s) return s;
  }
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = getenv("POLICE_INCIDENTS_CSV_URL");
  if (!url || !*url) {
    fprintf(stderr, "[police-incidents] no source (POLICE_INCIDENTS_CSV_URL unset)\n");
    return -1;
  }
  char *text = feed_get_text(ctx->http, url, 20000);
  if (!text || !*text) { free(text); fprintf(stderr, "[police-incidents] unavailable\n"); return -1; }

  cJSON *rows = csv_parse(text, 1);
  free(text);
  if (!rows || cJSON_GetArraySize(rows) == 0) {
    if (rows) cJSON_Delete(rows);
    fprintf(stderr, "[police-incidents] unavailable (no rows)\n");
    return -1;
  }

  static const char *LAT_K[] = { "lat","latitude","緯度","Y","y" };
  static const char *LON_K[] = { "lon","lng","longitude","経度","X","x" };
  static const char *TYP_K[] = { "type","罪種","手口","category" };
  static const char *DAT_K[] = { "date","発生年月日","日付","datetime" };
  static const char *PLC_K[] = { "place","発生場所","市区町村","address" };

  cJSON *features = cJSON_CreateArray();
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    double lat, lon;
    if (!pick_num(r, LAT_K, 5, &lat)) continue;
    if (!pick_num(r, LON_K, 6, &lon)) continue;
    if (lat < 20 || lat > 46 || lon < 122 || lon > 154) continue;
    const char *typ = pick_str(r, TYP_K, 4);
    const char *dat = pick_str(r, DAT_K, 4);
    const char *plc = pick_str(r, PLC_K, 4);

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *c = cJSON_CreateArray();
    cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
    cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
    cJSON_AddItemToObject(g, "coordinates", c);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
    if (typ) cJSON_AddStringToObject(p, "incident_type", typ); else cJSON_AddNullToObject(p, "incident_type");
    if (dat) cJSON_AddStringToObject(p, "date", dat); else cJSON_AddNullToObject(p, "date");
    if (plc) cJSON_AddStringToObject(p, "place", plc); else cJSON_AddNullToObject(p, "place");
    cJSON_AddStringToObject(p, "source", "police_incidents");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  cJSON_Delete(rows);

  if (cJSON_GetArraySize(features) == 0) {
    cJSON_Delete(features);
    fprintf(stderr, "[police-incidents] unavailable (no geo rows)\n");
    return -1;
  }
  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[police-incidents] emitted %d\n", n);
  return n > 0 ? 0 : -1;
}

static const source_def police_incidents_def = {
  .id = "police-incidents", .collector = "safety",
  .name = "Prefectural Police Crime Maps", .name_ja = "都道府県警 犯罪発生マップ",
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(police_incidents_def)
