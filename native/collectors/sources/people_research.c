/* collectors/osint/sources/people_research.c
 * OSINT services — place/business reviews.
 *   • Google Places — business reviews/hours (real API; GOOGLE_PLACES_API_KEY).
 * Real fetch or honest empty.
 *
 * NOTE: the person/company database sources that used to live here
 * (KAKEN, CINII, RESEARCHMAP, MANSION_COMMUNITY, TDB_TSR) now live in
 * people_finder.c, which is the person+company superset. Only the place/
 * business-review source remains here. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

static int run_places(const source_ctx *ctx, intel_sink *sink) {
  if (!ctx->entity || !*ctx->entity) return -1;
  const char *key = jo_env("GOOGLE_PLACES_API_KEY");
  if (!key) { fprintf(stderr, "[gplaces] gated (no GOOGLE_PLACES_API_KEY)\n"); return 0; }
  char *q = jo_urlencode(ctx->entity); if (!q) return 0;
  char url[768];
  snprintf(url, sizeof url,
    "https://maps.googleapis.com/maps/api/place/textsearch/json?query=%s&language=ja&key=%s", q, key);
  free(q);
  char *body = jo_get(ctx, url, NULL, "gplaces");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body); free(body);
  if (!root) return 0;
  int emitted = 0;
  cJSON *results = cJSON_GetObjectItem(root, "results");
  cJSON *r = NULL;
  cJSON_ArrayForEach(r, results) {
    const char *name = jo_sv(r, "name");
    const char *addr = jo_sv(r, "formatted_address");
    if (!name) continue;
    cJSON *loc = cJSON_GetObjectItem(cJSON_GetObjectItem(r, "geometry"), "location");
    double lat = 0, lon = 0; int hasgeo = 0;
    if (loc) { cJSON *la = cJSON_GetObjectItem(loc, "lat"), *ln = cJSON_GetObjectItem(loc, "lng");
      if (cJSON_IsNumber(la) && cJSON_IsNumber(ln)) { lat = la->valuedouble; lon = ln->valuedouble; hasgeo = 1; } }
    char *pj = cJSON_PrintUnformatted(r);
    intel_item it = {0};
    it.remote_key = jo_sv(r, "place_id"); it.title = name; it.summary = addr;
    it.body = pj; it.lang = "ja"; it.record_type = "place-review";
    it.has_geo = hasgeo; it.lat = lat; it.lon = lon;
    it.properties_json = "{\"service\":\"Google Places\",\"success\":true}";
    it.tags_json = "[\"osint-search\",\"places\"]";
    if (sink->emit(sink, &it) >= 0) emitted++;
    free(pj);
  }
  cJSON_Delete(root);
  fprintf(stderr, "[gplaces] emitted %d\n", emitted);
  return 0;
}

#define DEFR(SYM, ID, NAME, NAMEJA, RUN, CAT, TYPE, URL, DESC, FREE) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = RUN, .category = CAT, \
    .type = TYPE, .url = URL, .description = DESC, .layer = NULL, .free_tier = FREE }; \
  REGISTER_SOURCE(SYM)

DEFR(gplaces_def, "GOOGLE_PLACES", "Google Places", "Google マップ クチコミ", run_places,
     "commercial", "api", "https://maps.googleapis.com/", "Business reviews, hours, geo & ratings (needs GOOGLE_PLACES_API_KEY)", 0);
