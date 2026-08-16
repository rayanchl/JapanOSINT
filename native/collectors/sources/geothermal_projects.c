/* collectors/industry/sources/geothermal_projects.c
 * Port of server/src/collectors/jogmecGeothermalProjects.js (fetchText probe).
 * JS probes each of 8 JOGMEC geothermal district pages, then emits 8 features;
 * a probed page contributes its <title> and live source_url, otherwise the
 * row falls back to a SEED variant (source: *_seed_fallback). Per P5 rule 7
 * the SEED-fallback branch is dropped: a district feature is emitted ONLY
 * when its page probe succeeds (live), 0 rows when every page is down. The
 * district coordinates/metadata are intrinsic record identity (the pages are
 * narrative HTML with no parseable capacity), kept verbatim from JS.
 * uid/title derived by the geojson sink. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "lib/geojson.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASE "https://geothermal-model.jogmec.go.jp"

/* JS String.prototype.trim() — strip leading/trailing ASCII whitespace. */
static void js_trim(char *s) {
  size_t L = strlen(s);
  while (L && (s[L-1]==' '||s[L-1]=='\t'||s[L-1]=='\n'||s[L-1]=='\r'||
               s[L-1]=='\f'||s[L-1]=='\v')) s[--L] = 0;
  size_t i = 0;
  while (s[i]==' '||s[i]=='\t'||s[i]=='\n'||s[i]=='\r'||
         s[i]=='\f'||s[i]=='\v') i++;
  if (i) memmove(s, s + i, strlen(s + i) + 1);
}

typedef struct {
  const char *name_ja, *name, *district_type, *prefecture, *town;
  double lat, lon;
  int has_cap; double cap_mw;
  const char *notable_plants[3];
  const char *path;
} geo_d;

static const geo_d DISTRICTS[] = {
  { "\xe5\x8c\x97\xe6\xb5\xb7\xe9\x81\x93\xe8\x8c\x85\xe9\x83\xa8\xe9\x83\xa1\xe6\xa3\xae\xe7\x94\xba",
    "Mori, Hokkaido", "model", "\xe5\x8c\x97\xe6\xb5\xb7\xe9\x81\x93", "\xe6\xa3\xae\xe7\x94\xba",
    42.106, 140.583, 1, 50,
    { "\xe6\xa3\xae\xe5\x9c\xb0\xe7\x86\xb1\xe7\x99\xba\xe9\x9b\xbb\xe6\x89\x80 (Mori, 50 MW)", 0, 0 },
    "/model/mori-machi/" },
  { "\xe5\xb2\xa9\xe6\x89\x8b\xe7\x9c\x8c\xe5\x85\xab\xe5\xb9\xa1\xe5\xb9\xb3\xe5\xb8\x82",
    "Hachimantai, Iwate", "model", "\xe5\xb2\xa9\xe6\x89\x8b\xe7\x9c\x8c", "\xe5\x85\xab\xe5\xb9\xa1\xe5\xb9\xb3\xe5\xb8\x82",
    39.927, 140.987, 1, 103.5,
    { "\xe6\x9d\xbe\xe5\xb7\x9d\xe5\x9c\xb0\xe7\x86\xb1\xe7\x99\xba\xe9\x9b\xbb\xe6\x89\x80 (Matsukawa, 23.5 MW)",
      "\xe8\x91\x9b\xe6\xa0\xb9\xe7\x94\xb0\xe5\x9c\xb0\xe7\x86\xb1\xe7\x99\xba\xe9\x9b\xbb\xe6\x89\x80 (Kakkonda, 80 MW)", 0 },
    "/model/hachimantai-shi/" },
  { "\xe7\xa7\x8b\xe7\x94\xb0\xe7\x9c\x8c\xe6\xb9\xaf\xe6\xb2\xa2\xe5\xb8\x82",
    "Yuzawa, Akita", "model", "\xe7\xa7\x8b\xe7\x94\xb0\xe7\x9c\x8c", "\xe6\xb9\xaf\xe6\xb2\xa2\xe5\xb8\x82",
    39.164, 140.495, 1, 73.7,
    { "\xe4\xb8\x8a\xe3\x81\xae\xe5\xb2\xb1\xe5\x9c\xb0\xe7\x86\xb1\xe7\x99\xba\xe9\x9b\xbb\xe6\x89\x80 (Uenotai, 27.5 MW)",
      "\xe5\xb1\xb1\xe8\x91\xb5\xe6\xb2\xa2\xe5\x9c\xb0\xe7\x86\xb1\xe7\x99\xba\xe9\x9b\xbb\xe6\x89\x80 (Wasabizawa, 46.2 MW)", 0 },
    "/model/yuzawa-shi/" },
  { "\xe5\x8c\x97\xe6\xb5\xb7\xe9\x81\x93\xe5\xbc\x9f\xe5\xad\x90\xe5\xb1\x88\xe7\x94\xba",
    "Teshikaga, Hokkaido", "case_study", "\xe5\x8c\x97\xe6\xb5\xb7\xe9\x81\x93", "\xe5\xbc\x9f\xe5\xad\x90\xe5\xb1\x88\xe7\x94\xba",
    43.484, 144.460, 0, 0,
    { "Mashu / Kawayu exploration district", 0, 0 },
    "/case/teshikaga-chou/" },
  { "\xe5\xae\xae\xe5\x9f\x8e\xe7\x9c\x8c\xe5\xa4\xa7\xe5\xb4\x8e\xe5\xb8\x82",
    "Osaki, Miyagi", "case_study", "\xe5\xae\xae\xe5\x9f\x8e\xe7\x9c\x8c", "\xe5\xa4\xa7\xe5\xb4\x8e\xe5\xb8\x82",
    38.730, 140.738, 0, 0,
    { "\xe9\xb3\xb4\xe5\xad\x90\xe6\xb8\xa9\xe6\xb3\x89\xe9\x83\xb7 (Naruko hot-spring district)", 0, 0 },
    "/case/osaki-shi/" },
  { "\xe7\xa6\x8f\xe5\xb3\xb6\xe7\x9c\x8c\xe7\xa6\x8f\xe5\xb3\xb6\xe5\xb8\x82",
    "Fukushima City, Fukushima", "case_study", "\xe7\xa6\x8f\xe5\xb3\xb6\xe7\x9c\x8c", "\xe7\xa6\x8f\xe5\xb3\xb6\xe5\xb8\x82",
    37.760, 140.473, 1, 0.4,
    { "\xe5\x9c\x9f\xe6\xb9\xaf\xe6\xb8\xa9\xe6\xb3\x89\xe3\x83\x90\xe3\x82\xa4\xe3\x83\x8a\xe3\x83\xaa\xe3\x83\xbc\xe7\x99\xba\xe9\x9b\xbb\xe6\x89\x80 (Tsuchiyu binary, 0.4 MW)", 0, 0 },
    "/case/fukushima-shi/" },
  { "\xe5\xa4\xa7\xe5\x88\x86\xe7\x9c\x8c\xe4\xb9\x9d\xe9\x87\x8d\xe7\x94\xba",
    "Kokonoe, Oita", "case_study", "\xe5\xa4\xa7\xe5\x88\x86\xe7\x9c\x8c", "\xe4\xb9\x9d\xe9\x87\x8d\xe7\x94\xba",
    33.183, 131.215, 1, 110,
    { "\xe5\x85\xab\xe4\xb8\x81\xe5\x8e\x9f\xe5\x9c\xb0\xe7\x86\xb1\xe7\x99\xba\xe9\x9b\xbb\xe6\x89\x80 (Hatchobaru, 110 MW \xe2\x80\x94 largest in Japan)", 0, 0 },
    "/case/kokonoe-machi/" },
  { "\xe7\x86\x8a\xe6\x9c\xac\xe7\x9c\x8c\xe5\xb0\x8f\xe5\x9b\xbd\xe7\x94\xba",
    "Oguni, Kumamoto", "case_study", "\xe7\x86\x8a\xe6\x9c\xac\xe7\x9c\x8c", "\xe5\xb0\x8f\xe5\x9b\xbd\xe7\x94\xba",
    33.110, 131.080, 1, 50,
    { "\xe3\x82\x8f\xe3\x81\x84\xe3\x81\x9f\xe5\x9c\xb0\xe7\x86\xb1\xe7\x99\xba\xe9\x9b\xbb\xe6\x89\x80 (Waita, ~50 MW)", 0, 0 },
    "/case/oguni-machi/" },
};
#define NDIST ((int)(sizeof DISTRICTS / sizeof DISTRICTS[0]))

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *features = cJSON_CreateArray();
  int live = 0;
  for (int i = 0; i < NDIST; i++) {
    const geo_d *d = &DISTRICTS[i];
    char url[256];
    snprintf(url, sizeof url, "%s%s", BASE, d->path);
    char *html = feed_get_text(ctx->http, url, 10000);
    if (!html) continue;                 /* probe null -> SEED branch (drop) */
    live++;

    /* probe.page_title = html.match(/<title>([\s\S]*?)<\/title>/)[1].trim()
     * or null if no <title>. */
    char title[1024] = {0};
    int has_title = 0;
    if (html_tag(html, "title", title, sizeof title)) {
      js_trim(title);
      has_title = 1;                       /* matched -> trimmed (may be "") */
    }
    free(html);

    cJSON *f = gj_point_feature(d->lon, d->lat);

    cJSON *p = cJSON_CreateObject();       /* EXACT JS key order */
    char pid[16];
    snprintf(pid, sizeof pid, "GEO_%03d", i + 1);
    cJSON_AddStringToObject(p, "project_id", pid);
    cJSON_AddStringToObject(p, "name", d->name);
    cJSON_AddStringToObject(p, "name_ja", d->name_ja);
    cJSON_AddStringToObject(p, "district_type", d->district_type);
    cJSON_AddStringToObject(p, "prefecture", d->prefecture);
    cJSON_AddStringToObject(p, "town", d->town);
    /* This number is NOT fetched. It is the sum of the nameplate ratings of
     * the plants listed in `notable_plants` on this record (Hachimantai
     * 23.5+80 = 103.5, Yuzawa 27.5+46.2 = 73.7, …). The JOGMEC district pages
     * are narrative HTML and publish no capacity figure at all, so tagging the
     * row `jogmec_geothermal_live` presented a static catalogue total as a
     * live measurement. The figure is kept — it is real and citable — but it
     * now carries its own provenance and the row says what it actually is. */
    cJSON_AddItemToObject(p, "installed_capacity_mw",
      d->has_cap ? cJSON_CreateNumber(d->cap_mw) : cJSON_CreateNull());
    cJSON_AddStringToObject(p, "installed_capacity_provenance",
      "static catalogue: sum of the plants in notable_plants; not published by "
      "the fetched page");
    cJSON *np = cJSON_CreateArray();
    for (int k = 0; k < 3 && d->notable_plants[k]; k++)
      cJSON_AddItemToArray(np, cJSON_CreateString(d->notable_plants[k]));
    cJSON_AddItemToObject(p, "notable_plants", np);
    /* probe?.page_title ?? null: probe present here; null only if no <title> */
    cJSON_AddItemToObject(p, "page_title",
      has_title ? cJSON_CreateString(title) : cJSON_CreateNull());
    cJSON_AddStringToObject(p, "country", "JP");
    /* was "jogmec_geothermal_live": only page_title and source_url come from
     * the fetch; the district identity and capacity are a static catalogue. */
    cJSON_AddStringToObject(p, "source", "jogmec_geothermal_page_probe");
    cJSON_AddStringToObject(p, "source_url", url);
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[geothermal-projects] live_pages=%d emitted %d\n", live, n);
  return n >= 0 ? 0 : -1;
}

static const source_def geothermal_projects_def = {
  .id = "geothermal-projects", .collector = "industry",
  .name = "Geothermal Projects", .name_ja = "\xe5\x9c\xb0\xe7\x86\xb1\xe3\x83\xa2\xe3\x83\x87\xe3\x83\xab\xe5\x9c\xb0\xe5\x8c\xba",
   .update_interval_sec = 2592000, .run = run };
REGISTER_SOURCE(geothermal_projects_def)
