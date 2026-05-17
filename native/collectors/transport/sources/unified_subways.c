/* collectors/transport/sources/unified_subways.c
 * P5 — port of server/src/collectors/unifiedSubways.js (createUnifiedCollector).
 * Fuses mlit-n02-stations + odpt-transport + osm-transport-subways (all
 * already-registered upstreams), keeps subway/metro/monorail/tram-like stops
 * via isSubwayFeature, dedupes by station_code then qualified station_id, and
 * recomputes line_color via computeLineColor (lib/linecolor). The `_meta`
 * envelope is dropped per RULE 8 (same as every other port). */
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

/* JS String(prop) into buf: string verbatim, number via %g, else "" */
static void str_of(cJSON *v, char *out, size_t n) {
  if (cJSON_IsString(v) && v->valuestring) snprintf(out, n, "%s", v->valuestring);
  else if (cJSON_IsNumber(v)) snprintf(out, n, "%g", v->valuedouble);
  else out[0] = 0;
}

static const char *SUBWAY_LIKE[] = {
  "subway", "metro", "underground", "light_rail", "monorail",
  "tram_stop", "tram",
};
static const char *SUBWAY_LINE_HINTS[] = {
  "Tokyo Metro", "Toei", "Osaka Metro", "Nagoya", "Sapporo", "Sendai",
  "Fukuoka", "Kyoto", "Kobe", "Yokohama", "Monorail",
};
/* JA hints (UTF-8): メトロ, 都営, 市営, モノレール */
static const char *SUBWAY_JA_HINTS[] = {
  "\343\203\241\343\203\210\343\203\255",
  "\351\203\275\345\226\266",
  "\345\270\202\345\226\266",
  "\343\203\242\343\203\216\343\203\254\343\203\274\343\203\253",
};

/* isSubwayFeature */
static int is_subway(cJSON *f) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON *t = p ? cJSON_GetObjectItem(p, "type") : NULL;
  if (!cJSON_IsString(t) || !t->valuestring[0])
    t = p ? cJSON_GetObjectItem(p, "classification") : NULL;
  char ty[64];
  lower(cJSON_IsString(t) ? t->valuestring : "", ty, sizeof ty);
  for (size_t i = 0; i < sizeof(SUBWAY_LIKE)/sizeof(*SUBWAY_LIKE); i++)
    if (!strcmp(ty, SUBWAY_LIKE[i])) return 1;

  cJSON *ln = p ? cJSON_GetObjectItem(p, "line") : NULL;
  if (!cJSON_IsString(ln) && !cJSON_IsNumber(ln))
    ln = p ? cJSON_GetObjectItem(p, "line_name") : NULL;
  char line[512]; str_of(ln, line, sizeof line);
  for (size_t i = 0; i < sizeof(SUBWAY_LINE_HINTS)/sizeof(*SUBWAY_LINE_HINTS); i++)
    if (line[0] && strstr(line, SUBWAY_LINE_HINTS[i])) return 1;
  for (size_t i = 0; i < sizeof(SUBWAY_JA_HINTS)/sizeof(*SUBWAY_JA_HINTS); i++)
    if (line[0] && strstr(line, SUBWAY_JA_HINTS[i])) return 1;

  cJSON *opv = p ? cJSON_GetObjectItem(p, "operator") : NULL;
  char op[512]; str_of(opv, op, sizeof op);
  for (size_t i = 0; i < sizeof(SUBWAY_LINE_HINTS)/sizeof(*SUBWAY_LINE_HINTS); i++)
    if (op[0] && strstr(op, SUBWAY_LINE_HINTS[i])) return 1;
  for (size_t i = 0; i < sizeof(SUBWAY_JA_HINTS)/sizeof(*SUBWAY_JA_HINTS); i++)
    if (op[0] && strstr(op, SUBWAY_JA_HINTS[i])) return 1;
  return 0;
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
    { "mlit-n02-stations",     NULL },
    { "odpt-transport",        NULL },
    { "osm-transport-subways", NULL },
  };
  static const unified_keyfn keys[] = { k_station_code, k_qualified_sid };
  int n = unified_collect(ctx, sink, "unified-subways", ups, 3,
                          keys, 2, 4, is_subway, ensure_line_color);
  return n >= 0 ? 0 : -1;
}

static const source_def unified_subways_def = {
  .id = "unified-subways", .collector = "transport",
  .name = "Unified Subways (fused)", .name_ja = "Unified Subways",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(unified_subways_def)
