/* collectors/safety/sources/koban_map.c
 * Port of server/src/collectors/kobanMap.js (fetchOverpassTiled). Curated
 * SEED_KOBAN offline fallback intentionally NOT ported — correctness-neutral
 * (JS does `if (!live) features = []` anyway). */
#include "../../source.h"
#include "../../lib/overpass.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/* case-insensitive ASCII substring (mirrors JS /…/i for the latin alts). */
static int icontains(const char *hay, const char *needle) {
  size_t nl = strlen(needle);
  for (const char *h = hay; *h; h++) {
    size_t k = 0;
    while (k < nl && h[k] &&
           tolower((unsigned char)h[k]) == tolower((unsigned char)needle[k]))
      k++;
    if (k == nl) return 1;
  }
  return 0;
}

static cJSON *map(cJSON *el, int i, double lon, double lat, void *ud) {
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *c = cJSON_CreateArray();
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
  cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
  cJSON_AddItemToObject(g, "coordinates", c);
  cJSON_AddItemToObject(f, "geometry", g);

  const char *name = ov_tag(el, "name");
  if (!name) name = ov_tag(el, "name:en");
  if (!name) name = "Police";
  const char *ptype;
  if (strstr(name, "\xE4\xBA\xA4\xE7\x95\xAA") || icontains(name, "koban"))
    ptype = "koban";                                  /* 交番 | koban */
  else if (strstr(name, "\xE6\x9C\xAC\xE9\x83\xA8") ||
           icontains(name, "headquarters"))
    ptype = "headquarters";                           /* 本部 | headquarters */
  else if (strstr(name, "\xE9\xA7\x90\xE5\x9C\xA8\xE6\x89\x80"))
    ptype = "chuzaisho";                              /* 駐在所 */
  else
    ptype = "station";

  cJSON *p = cJSON_CreateObject();
  cJSON *id = cJSON_GetObjectItem(el, "id");
  char fid[64];
  snprintf(fid, sizeof fid, "KOBAN_%lld",
           id && cJSON_IsNumber(id) ? (long long)id->valuedouble : 0);
  cJSON_AddStringToObject(p, "facility_id", fid);
  cJSON_AddStringToObject(p, "name", name);
  cJSON_AddStringToObject(p, "police_type", ptype);
  const char *op = ov_tag(el, "operator");
  cJSON_AddItemToObject(p, "operator",
                        op ? cJSON_CreateString(op) : cJSON_CreateNull());
  const char *ph = ov_tag(el, "phone");
  cJSON_AddItemToObject(p, "phone",
                        ph ? cJSON_CreateString(ph) : cJSON_CreateNull());
  const char *oh = ov_tag(el, "opening_hours");
  cJSON_AddStringToObject(p, "opening_hours", oh ? oh : "24/7");
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "source", "osm_overpass");
  cJSON_AddItemToObject(f, "properties", p);
  return f;
}

static void body(const char *bbox, char *o, size_t n, void *ud) {
  snprintf(o, n,
    "node[\"amenity\"=\"police\"](%s);"
    "way[\"amenity\"=\"police\"](%s);",
    bbox, bbox);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int n = overpass_tiled_collect(ctx, sink, body, 180, 90000, map, NULL);
  return n >= 0 ? 0 : -1;
}

static const source_def koban_map_def = {
  .id = "koban-map", .collector = "safety", .name = "Koban / Police Boxes",
  .name_ja = "交番マップ", 
  .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(koban_map_def)
