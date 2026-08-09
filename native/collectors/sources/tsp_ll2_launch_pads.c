/* collectors/sources/tsp_ll2_launch_pads.c
 * Launch Library 2 — every known orbital launch pad on Earth.
 * Endpoint: https://ll.thespacedevs.com/2.2.0/pad/?limit=100&offset=N (keyless)
 * Emits: pad id, name, operating agency, host location name + country code +
 *   timezone, pad total_launch_count, location total_launch_count and
 *   orbital_launch_attempt_count, map_url / wiki_url.
 * Geo (R2): latitude/longitude are the PAD's own coordinates as published by
 *   Launch Library. parse_notes: "latitude/longitude are STRINGS, parse with
 *   strtod" — a plain cJSON number read would silently yield 0/0 and stack
 *   every pad on Null Island, so both string and number forms are handled and
 *   a pad without parseable coordinates is emitted without geometry.
 * parse_notes honoured: "Paginated envelope {count,next,previous,results}";
 *   "238 pads total, so 3-4 paged requests at limit=100; given the anonymous
 *   throttle, refresh weekly at most" — update_interval_sec is 604800 and a
 *   429 simply stops paging (what was fetched is kept, R3).
 * Licence: The Space Devs Launch Library 2 — free/open, anonymous tier is rate
 *   limited, attribution requested.
 */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include "../../lib/feedlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LL2_PAD_BASE "https://ll.thespacedevs.com/2.2.0/pad/"
#define PAGES 4
#define PAGE_SIZE 100

/* Launch Library returns coordinates as STRINGS. Accept either shape. */
static int coordv(const cJSON *o, const char *k, double *out) {
  const cJSON *v = cJSON_GetObjectItem(o, k);
  if (!v) return 0;
  if (cJSON_IsNumber(v)) { *out = v->valuedouble; return 1; }
  if (cJSON_IsString(v) && v->valuestring && v->valuestring[0]) {
    char *end = NULL;
    double d = strtod(v->valuestring, &end);
    if (end && end != v->valuestring) { *out = d; return 1; }
  }
  return 0;
}

static int emit_pads(cJSON *results, intel_sink *sink) {
  int n = 0;
  cJSON *pad;
  cJSON_ArrayForEach(pad, results) {
    double pid;
    if (!jo_num(pad, "id", &pid)) continue;
    const char *name = jo_sv(pad, "name");
    if (!name) continue;                          /* no title -> no row (R1) */

    const cJSON *loc = cJSON_GetObjectItem(pad, "location");
    const char *locname = loc ? jo_sv(loc, "name") : NULL;
    const char *cc = jo_sv(pad, "country_code");
    if (!cc && loc) cc = jo_sv(loc, "country_code");

    cJSON *pr = cJSON_CreateObject();
    cJSON_AddNumberToObject(pr, "pad_id", pid);
    cJSON_AddStringToObject(pr, "pad_name", name);
    if (locname) cJSON_AddStringToObject(pr, "location_name", locname);
    if (cc)      cJSON_AddStringToObject(pr, "country_code", cc);
    const char *s;
    double d;
    if (loc && (s = jo_sv(loc, "timezone_name")))
      cJSON_AddStringToObject(pr, "timezone", s);
    if (loc && jo_num(loc, "total_launch_count", &d))
      cJSON_AddNumberToObject(pr, "location_total_launch_count", d);
    if (jo_num(pad, "total_launch_count", &d))
      cJSON_AddNumberToObject(pr, "pad_total_launch_count", d);
    if (jo_num(pad, "orbital_launch_attempt_count", &d))
      cJSON_AddNumberToObject(pr, "orbital_launch_attempt_count", d);
    if ((s = jo_sv(pad, "map_url")))  cJSON_AddStringToObject(pr, "map_url", s);
    if ((s = jo_sv(pad, "wiki_url"))) cJSON_AddStringToObject(pr, "wiki_url", s);
    const cJSON *ag = cJSON_GetObjectItem(pad, "agency_id");
    if (cJSON_IsNumber(ag))
      cJSON_AddNumberToObject(pr, "agency_id", ag->valuedouble);
    cJSON_AddStringToObject(pr, "source", "The Space Devs Launch Library 2");

    double lat = 0, lon = 0;
    int has_geo = 0;
    if (coordv(pad, "latitude", &lat) && coordv(pad, "longitude", &lon) &&
        lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0 &&
        !(lat == 0.0 && lon == 0.0))
      has_geo = 1;

    char *pj = cJSON_PrintUnformatted(pr);
    cJSON_Delete(pr);

    char title[256], summary[256], key[32];
    snprintf(key, sizeof key, "%.0f", pid);
    if (locname) snprintf(title, sizeof title, "%s — %s", name, locname);
    else         snprintf(title, sizeof title, "Launch pad %s", name);
    double tlc = 0;
    if (jo_num(pad, "total_launch_count", &tlc))
      snprintf(summary, sizeof summary, "%s · %.0f launches from this pad",
               cc ? cc : "country n/a", tlc);
    else
      snprintf(summary, sizeof summary, "%s", cc ? cc : "launch pad");

    intel_item it = {0};
    it.remote_key      = key;
    it.title           = title;
    it.summary         = summary;
    it.link            = jo_sv(pad, "wiki_url");
    it.lang            = "en";
    it.record_type     = "launch-pad";
    it.has_geo         = has_geo;
    it.lat             = lat;
    it.lon             = lon;
    it.properties_json = pj;
    it.tags_json       = "[\"space\",\"launch\",\"spaceport\"]";
    if (sink->emit(sink, &it) >= 0) n++;
    free(pj);
  }
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  int total = 0, pages = 0;
  for (int i = 0; i < PAGES; i++) {
    char url[192];
    snprintf(url, sizeof url, "%s?limit=%d&offset=%d",
             LL2_PAD_BASE, PAGE_SIZE, i * PAGE_SIZE);
    cJSON *doc = feed_get_json(ctx->http, url, 30000);
    if (!doc) break;                 /* 429 / transport error: keep what we got */
    cJSON *res = cJSON_GetObjectItem(doc, "results");
    if (!cJSON_IsArray(res)) { cJSON_Delete(doc); break; }
    pages++;
    int got = cJSON_GetArraySize(res);
    total += emit_pads(res, sink);
    int done = !cJSON_IsString(cJSON_GetObjectItem(doc, "next"));
    cJSON_Delete(doc);
    if (got == 0 || done) break;
  }
  if (pages == 0) {
    fprintf(stderr, "[ll2-launch-pads] fetch failed (anonymous tier throttled?)\n");
    return -1;
  }
  fprintf(stderr, "[ll2-launch-pads] emitted %d over %d page(s)\n", total, pages);
  return 0;
}

static const source_def tsp_ll2_launch_pads_def = {
  .id = "ll2-launch-pads", .collector = "satellite",
  .name = "Launch Library 2 — orbital launch pads",
  .update_interval_sec = 604800, .run = run,
  .category = "satellite", .type = "api", .url = LL2_PAD_BASE,
  .description = "Every known orbital launch pad with pad-level coordinates, operating agency, host spaceport, timezone and lifetime launch counts.",
  .license = "The Space Devs Launch Library 2 — free/open, anonymous tier rate limited, attribution requested.",
  .free_tier = 1,
};
REGISTER_SOURCE(tsp_ll2_launch_pads_def)
