/* collectors/safety/sources/jma_warnings.c
 * Port of server/src/collectors/jmaWarnings.js — JMA bosai warning JSON,
 * one file per prefecture office code. Emit one geo point per area that
 * currently has >=1 active warning (status != "解除", code != "00").
 * Real JMA data only; honest empty when nothing fetched (rule 8).
 * Property key order mirrors the JS mapFn for featureUid hash parity. */
#include "../../../source.h"
#include "../../../lib/feedlib.h"
#include "../../../lib/geojson.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WARN_URL "https://www.jma.go.jp/bosai/warning/data/warning/"

struct area { const char *code, *name; double lat, lon; };
static const struct area AREAS[] = {
  {"016000","北海道（石狩・空知・後志）",43.06,141.35},
  {"020000","青森県",40.82,140.74}, {"030000","岩手県",39.70,141.15},
  {"040000","宮城県",38.27,140.87}, {"050000","秋田県",39.72,140.10},
  {"060000","山形県",38.24,140.36}, {"070000","福島県",37.75,140.47},
  {"080000","茨城県",36.37,140.47}, {"090000","栃木県",36.57,139.88},
  {"100000","群馬県",36.39,139.06}, {"110000","埼玉県",35.86,139.65},
  {"120000","千葉県",35.61,140.12}, {"130000","東京都",35.69,139.69},
  {"140000","神奈川県",35.45,139.64}, {"150000","新潟県",37.90,139.02},
  {"160000","富山県",36.70,137.21}, {"170000","石川県",36.59,136.63},
  {"180000","福井県",36.07,136.22}, {"190000","山梨県",35.66,138.57},
  {"200000","長野県",36.65,138.18}, {"210000","岐阜県",35.39,136.72},
  {"220000","静岡県",34.98,138.38}, {"230000","愛知県",35.18,136.91},
  {"240000","三重県",34.73,136.51}, {"250000","滋賀県",35.00,135.87},
  {"260000","京都府",35.02,135.76}, {"270000","大阪府",34.69,135.52},
  {"280000","兵庫県",34.69,135.18}, {"290000","奈良県",34.69,135.83},
  {"300000","和歌山県",34.23,135.17}, {"310000","鳥取県",35.50,134.24},
  {"320000","島根県",35.47,133.05}, {"330000","岡山県",34.66,133.93},
  {"340000","広島県",34.40,132.46}, {"350000","山口県",34.19,131.47},
  {"360000","徳島県",34.07,134.56}, {"370000","香川県",34.34,134.04},
  {"380000","愛媛県",33.84,132.77}, {"390000","高知県",33.56,133.53},
  {"400000","福岡県",33.59,130.40}, {"410000","佐賀県",33.25,130.30},
  {"420000","長崎県",32.74,129.87}, {"430000","熊本県",32.80,130.73},
  {"440000","大分県",33.24,131.61}, {"450000","宮崎県",31.91,131.42},
  {"460100","鹿児島県",31.56,130.56}, {"471000","沖縄県（本島中南部）",26.21,127.68},
};
#define NAREA ((int)(sizeof AREAS / sizeof AREAS[0]))

/* Collect distinct active warning names from a bosai warning doc. */
static cJSON *extract_active(cJSON *doc) {
  cJSON *out = cJSON_CreateArray();
  cJSON *areaTypes = cJSON_GetObjectItem(doc, "areaTypes");
  if (!areaTypes || !cJSON_IsArray(areaTypes)) return out;
  cJSON *at;
  cJSON_ArrayForEach(at, areaTypes) {
    cJSON *areas = cJSON_GetObjectItem(at, "areas");
    if (!areas || !cJSON_IsArray(areas)) continue;
    cJSON *a;
    cJSON_ArrayForEach(a, areas) {
      cJSON *warns = cJSON_GetObjectItem(a, "warnings");
      if (!warns || !cJSON_IsArray(warns)) continue;
      cJSON *w;
      cJSON_ArrayForEach(w, warns) {
        cJSON *st = cJSON_GetObjectItem(w, "status");
        cJSON *cd = cJSON_GetObjectItem(w, "code");
        if (!st || !cJSON_IsString(st)) continue;
        if (strcmp(st->valuestring, "解除") == 0) continue;
        if (!cd || !cJSON_IsString(cd)) continue;
        if (strcmp(cd->valuestring, "00") == 0) continue;
        cJSON *nm = cJSON_GetObjectItem(w, "name");
        const char *val = (nm && cJSON_IsString(nm)) ? nm->valuestring
                                                     : cd->valuestring;
        /* unique */
        int dup = 0; cJSON *e;
        cJSON_ArrayForEach(e, out)
          if (cJSON_IsString(e) && strcmp(e->valuestring, val) == 0) { dup = 1; break; }
        if (!dup) cJSON_AddItemToArray(out, cJSON_CreateString(val));
      }
    }
  }
  return out;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *features = cJSON_CreateArray();
  int any_fetched = 0;

  for (int i = 0; i < NAREA; i++) {
    char url[160];
    snprintf(url, sizeof url, "%s%s.json", WARN_URL, AREAS[i].code);
    cJSON *doc = feed_get_json(ctx->http, url, 8000);
    if (!doc) continue;
    any_fetched = 1;

    cJSON *warns = extract_active(doc);
    if (cJSON_GetArraySize(warns) == 0) { cJSON_Delete(warns); cJSON_Delete(doc); continue; }

    cJSON *rd = cJSON_GetObjectItem(doc, "reportDatetime");

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    cJSON *g = cJSON_CreateObject();
    cJSON_AddStringToObject(g, "type", "Point");
    cJSON *c = cJSON_CreateArray();
    cJSON_AddItemToArray(c, cJSON_CreateNumber(AREAS[i].lon));
    cJSON_AddItemToArray(c, cJSON_CreateNumber(AREAS[i].lat));
    cJSON_AddItemToObject(g, "coordinates", c);
    cJSON_AddItemToObject(f, "geometry", g);

    cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
    cJSON_AddStringToObject(p, "area_code", AREAS[i].code);
    cJSON_AddStringToObject(p, "area_name", AREAS[i].name);
    cJSON_AddItemToObject(p, "warnings", warns);
    cJSON_AddNumberToObject(p, "warning_count", cJSON_GetArraySize(warns));
    if (rd && cJSON_IsString(rd)) cJSON_AddStringToObject(p, "report_at", rd->valuestring);
    else                          cJSON_AddNullToObject(p, "report_at");
    cJSON_AddStringToObject(p, "source", "jma_warning");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
    cJSON_Delete(doc);
  }

  if (!any_fetched) { cJSON_Delete(features); fprintf(stderr, "[jma-warnings] unavailable\n"); return -1; }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[jma-warnings] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def jma_warnings_def = {
  .id = "jma-warnings", .collector = "safety",
  .name = "JMA Weather Warnings", .name_ja = "気象庁 気象警報",
  .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(jma_warnings_def)
