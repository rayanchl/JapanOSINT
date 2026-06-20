/* collectors/transport/sources/unified_buses.c
 * P5 — port of server/src/collectors/unifiedBuses.js (createUnifiedCollector).
 * Fuses mlit-p11-bus-stops + gtfs-jp + bus-routes + osm-transport-buses (all
 * already-registered upstreams; first three tagged kind stop/stop/terminal,
 * the OSM layer self-tags kind), dedupes by GTFS-JP qualified stop_id then the
 * name+coord-grid fallback (coordPrecision 4). No postProcess. The `_meta`
 * envelope is dropped per RULE 8 (same as every other port). */
#include "../../source.h"
#include "../../lib/unified.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* dedupeKeys[0]: id=stop_id; if(!id)null; String(id).startsWith("GTFSJP_")?id:null */
static const char *k_gtfsjp_stop_id(cJSON *f, char *b, size_t n) {
  cJSON *p = cJSON_GetObjectItem(f, "properties");
  cJSON *v = p ? cJSON_GetObjectItem(p, "stop_id") : NULL;
  char id[256] = {0};
  if (cJSON_IsString(v) && v->valuestring[0]) snprintf(id,sizeof id,"%s",v->valuestring);
  else if (cJSON_IsNumber(v)) snprintf(id,sizeof id,"%g",v->valuedouble);
  else return NULL;
  if (strncmp(id, "GTFSJP_", 7) != 0) return NULL;
  snprintf(b, n, "%s", id);
  return b;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  static const unified_upstream ups[] = {
    { "mlit-p11-bus-stops",  "stop"     },
    { "gtfs-jp",             "stop"     },
    { "bus-routes",          "terminal" },
    { "osm-transport-buses", NULL       },
  };
  static const unified_keyfn keys[] = { k_gtfsjp_stop_id };
  int n = unified_collect(ctx, sink, "unified-buses", ups, 4,
                          keys, 1, 4, NULL, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def unified_buses_def = {
  .id = "unified-buses", .collector = "transport",
  .name = "Unified Buses (fused)", .name_ja = "Unified Buses",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(unified_buses_def)
