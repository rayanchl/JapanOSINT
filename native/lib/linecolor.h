/* lib/linecolor.h — port of collectors/_lineColor.js computeLineColor.
 * Per-line deterministic color for rail/subway/tram/monorail features so a
 * station Point and its track LineString share a color. Backs the
 * osm-transport-trains/subways + overpass-rail/subway-tracks ports.
 *
 * Parity note: the FNV-1a hash runs over UTF-8 bytes (JS used UTF-16 code
 * units). Output is still deterministic & distinct-per-line, which is the
 * only requirement post-parity (C is the sole backend). */
#ifndef JO_LINECOLOR_H
#define JO_LINECOLOR_H
#include "../third_party/cJSON.h"

/* `tags` = the OSM tags object (el->"tags"), may be NULL. Writes "#rrggbb"
 * into out8 (>=8 bytes) and returns 1, or returns 0 (== JS null → caller
 * emits a JSON null / client default). */
int line_color(cJSON *tags, char *out8);

#endif
