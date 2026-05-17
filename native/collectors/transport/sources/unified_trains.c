/* collectors/transport/sources/unified_trains.c
 * P6 GOLD REF — port of server/src/collectors/unifiedTrains.js
 * (createUnifiedCollector). Fuses mlit-n02-stations + odpt-transport +
 * osm-transport-trains (all already-registered upstreams), excludes
 * subway/tram/monorail, dedupes by station_code then qualified station_id,
 * and recomputes line_color via computeLineColor (lib/linecolor). The
 * `_meta` envelope is dropped per RULE 8 (same as every other port). */
#include "../../../source.h"
#include "../../../lib/unified.h"
#include "../../../lib/linecolor.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static void lower(const char *in, char *out, size_t n) {
  size_t i = 0;
  for (; in && in[i] && i + 1 < n; i++)
    out[i] = (char)tolower((unsigned char)in[i]);
  out[i] = 0;
}

/* isTrainFeature: drop subway/metro/underground + tram/monorail/light_rail */
static int is_train(cJSON *f) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON *t = p ? cJSON_GetObjectItem(p, "type") : NULL;
  if (!cJSON_IsString(t) || !t->valuestring[0])
    t = p ? cJSON_GetObjectItem(p, "classification") : NULL;
  char ty[64];
  lower(cJSON_IsString(t) ? t->valuestring : "", ty, sizeof ty);
  if (!strcmp(ty,"subway") || !strcmp(ty,"metro") || !strcmp(ty,"underground"))
    return 0;
  if (!strcmp(ty,"tram_stop") || !strcmp(ty,"tram") ||
      !strcmp(ty,"monorail") || !strcmp(ty,"light_rail"))
    return 0;
  return 1;
}

/* dedupeKeys[0]: f.properties?.station_code || null */
static const char *k_station_code(cJSON *f, char *b, size_t n) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON *v = p ? cJSON_GetObjectItem(p, "station_code") : NULL;
  if (cJSON_IsString(v) && v->valuestring[0]) { snprintf(b,n,"%s",v->valuestring); return b; }
  if (cJSON_IsNumber(v)) { snprintf(b,n,"%g",v->valuedouble); return b; }
  return NULL;
}
/* dedupeKeys[1]: sid=station_id; String(sid).includes(':') ? sid : null */
static const char *k_qualified_sid(cJSON *f, char *b, size_t n) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON *v = p ? cJSON_GetObjectItem(p, "station_id") : NULL;
  char sid[256] = {0};
  if (cJSON_IsString(v) && v->valuestring[0]) snprintf(sid,sizeof sid,"%s",v->valuestring);
  else if (cJSON_IsNumber(v)) snprintf(sid,sizeof sid,"%g",v->valuedouble);
  else return NULL;
  if (!strchr(sid, ':')) return NULL;
  snprintf(b, n, "%s", sid);
  return b;
}

/* ensureLineColor: line_color := computeLineColor(properties) || null,
 * always (re)set so stale values are overwritten (placed last). */
static void ensure_line_color(cJSON *f) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  if (!p) return;
  char c[8];
  int ok = line_color(p, c);
  cJSON_DeleteItemFromObject(p, "line_color");
  if (ok) cJSON_AddStringToObject(p, "line_color", c);
  else    cJSON_AddItemToObject(p, "line_color", cJSON_CreateNull());
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const unified_upstream ups[] = {
    { "mlit-n02-stations",    NULL },
    { "odpt-transport",       NULL },
    { "osm-transport-trains", NULL },
  };
  static const unified_keyfn keys[] = { k_station_code, k_qualified_sid };
  int n = unified_collect(ctx, sink, "unified-trains", ups, 3,
                          keys, 2, 4, is_train, ensure_line_color);
  return n >= 0 ? 0 : -1;
}

static const source_def unified_trains_def = {
  .id = "unified-trains", .collector = "transport",
  .name = "Unified Trains (fused)", .name_ja = "Unified Trains",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(unified_trains_def)
