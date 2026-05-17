/* collectors/transport/sources/unified_port_infra.c
 * P6 — port of server/src/collectors/unifiedPortInfra.js
 * (createUnifiedCollector). Fuses port-infra + osm-transport-ports +
 * mlit-c02-ports (already-registered upstreams), dedupes by port_id
 * but only when globally qualified (MLIT_C02_ / OSM_ / PORT_ prefix);
 * coordPrecision 4. No filter, no postProcess. The `_meta` envelope is
 * dropped per RULE 8 (same as every other port). */
#include "../../../source.h"
#include "../../../lib/unified.h"
#include "../../../lib/linecolor.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* dedupeKeys[0]: id=properties?.port_id; if(!id) null;
 * s=String(id); startsWith MLIT_C02_ | OSM_ | PORT_ ? s : null */
static const char *k_port_id(cJSON *f, char *b, size_t n) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON *v = p ? cJSON_GetObjectItem(p, "port_id") : NULL;
  char s[256] = {0};
  if (cJSON_IsString(v) && v->valuestring[0]) snprintf(s,sizeof s,"%s",v->valuestring);
  else if (cJSON_IsNumber(v)) snprintf(s,sizeof s,"%g",v->valuedouble);
  else return NULL;
  if (strncmp(s,"MLIT_C02_",9) == 0 ||
      strncmp(s,"OSM_",4) == 0 ||
      strncmp(s,"PORT_",5) == 0) {
    snprintf(b, n, "%s", s);
    return b;
  }
  return NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const unified_upstream ups[] = {
    { "port-infra",          NULL },
    { "osm-transport-ports", NULL },
    { "mlit-c02-ports",      NULL },
  };
  static const unified_keyfn keys[] = { k_port_id };
  int n = unified_collect(ctx, sink, "unified-port-infra", ups, 3,
                          keys, 1, 4, NULL, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def unified_port_infra_def = {
  .id = "unified-port-infra", .collector = "transport",
  .name = "Unified Port Infrastructure (fused)", .name_ja = "Unified Port Infra",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(unified_port_infra_def)
