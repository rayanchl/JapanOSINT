/* collectors/transport/sources/unified_airports.c
 * P6 — port of server/src/collectors/unifiedAirports.js
 * (createUnifiedCollector). Fuses mlit-p02-airports + airport-infra
 * (already-registered upstreams), dedupes by icao, then iata, then
 * name (lowercased + trimmed). No filter, no postProcess. The `_meta`
 * envelope is dropped per RULE 8 (same as every other port). */
#include "../../source.h"
#include "../../lib/unified.h"
#include "../../lib/linecolor.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* dedupeKeys[0]: f.properties?.icao || null */
static const char *k_icao(cJSON *f, char *b, size_t n) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON *v = p ? cJSON_GetObjectItem(p, "icao") : NULL;
  if (cJSON_IsString(v) && v->valuestring[0]) { snprintf(b,n,"%s",v->valuestring); return b; }
  if (cJSON_IsNumber(v)) { snprintf(b,n,"%g",v->valuedouble); return b; }
  return NULL;
}
/* dedupeKeys[1]: f.properties?.iata || null */
static const char *k_iata(cJSON *f, char *b, size_t n) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON *v = p ? cJSON_GetObjectItem(p, "iata") : NULL;
  if (cJSON_IsString(v) && v->valuestring[0]) { snprintf(b,n,"%s",v->valuestring); return b; }
  if (cJSON_IsNumber(v)) { snprintf(b,n,"%g",v->valuedouble); return b; }
  return NULL;
}
/* dedupeKeys[2]: n=(properties?.name||'').toLowerCase().trim();
 * n.length>0 ? n : null */
static const char *k_name(cJSON *f, char *b, size_t n) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON *v = p ? cJSON_GetObjectItem(p, "name") : NULL;
  char raw[512] = {0};
  if (cJSON_IsString(v) && v->valuestring[0]) snprintf(raw,sizeof raw,"%s",v->valuestring);
  else return NULL;
  char low[512];
  size_t i = 0;
  for (; raw[i] && i + 1 < sizeof low; i++)
    low[i] = (char)tolower((unsigned char)raw[i]);
  low[i] = 0;
  /* trim leading/trailing ASCII whitespace (JS .trim()) */
  char *s = low;
  while (*s && isspace((unsigned char)*s)) s++;
  char *e = s + strlen(s);
  while (e > s && isspace((unsigned char)e[-1])) e--;
  *e = 0;
  if (s == e) return NULL;
  snprintf(b, n, "%s", s);
  return b;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const unified_upstream ups[] = {
    { "mlit-p02-airports", NULL },
    { "airport-infra",     NULL },
  };
  static const unified_keyfn keys[] = { k_icao, k_iata, k_name };
  int n = unified_collect(ctx, sink, "unified-airports", ups, 2,
                          keys, 3, 4, NULL, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def unified_airports_def = {
  .id = "unified-airports", .collector = "transport",
  .name = "Unified Airports (fused)", .name_ja = "Unified Airports",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(unified_airports_def)
