/* collectors/environment/sources/wdcgg_co2.c
 * Port of server/src/collectors/wdcggCo2.js (FeatureCollection).
 * The only live path is tryRegistryFetch(), gated on WDCGG_REGISTRY_URL
 * (a loose CSV: header row, then code,name,country,latitude,longitude,
 * elevation). The curated SEED_STATIONS fallback / _meta envelope is
 * dropped per port rule 7 (no env → 0 rows). On the registry path
 * species/operator are always null and name_ja is absent (→ JS null).
 * Feature props order: id, station_code, name, name_ja, country,
 * elevation_m, species, operator, source. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TIMEOUT_MS 15000

/* split one CSV line by ',' into up to max fields (no quote handling — the
 * JS uses a bare String.split(',') too). Returns field count; fields point
 * into the mutated buffer. */
static int csv_split(char *line, char **out, int max) {
  int n = 0;
  char *p = line;
  out[n++] = p;
  while (*p && n < max) {
    if (*p == ',') { *p = 0; out[n++] = p + 1; }
    p++;
  }
  return n;
}

/* header.indexOf(k) after lowercasing the header line */
static int idx_of(char **h, int hn, const char *k) {
  for (int i = 0; i < hn; i++) {
    const char *a = h[i];
    const char *b = k;
    while (*a && *b && tolower((unsigned char)*a) == *b) { a++; b++; }
    if (*a == 0 && *b == 0) return i;
  }
  return -1;
}

static const char *col(char **f, int fn, int i) {
  return (i >= 0 && i < fn) ? f[i] : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = getenv("WDCGG_REGISTRY_URL");
  if (!url || !*url) {
    fprintf(stderr, "[wdcgg-co2] gated (no WDCGG_REGISTRY_URL)\n");
    return 0;
  }

  char *text = feed_get_text(ctx->http, url, TIMEOUT_MS);
  if (!text) return -1;

  /* text.trim().split(/\r?\n/) */
  char *s = text;
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
  char *end = s + strlen(s);
  while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
                     end[-1] == '\n' || end[-1] == '\r')) *--end = 0;

  /* split into lines */
  int cap = 64, ln = 0;
  char **lines = malloc(cap * sizeof(char *));
  for (char *p = s; ; ) {
    if (ln == cap) { cap *= 2; lines = realloc(lines, cap * sizeof(char *)); }
    lines[ln++] = p;
    char *nl = strchr(p, '\n');
    if (!nl) break;
    *nl = 0;
    if (nl > p && nl[-1] == '\r') nl[-1] = 0;
    p = nl + 1;
  }
  if (ln < 2) { free(lines); free(text); return -1; }

  /* header (lowercased lazily inside idx_of) */
  char hbuf[2048];
  snprintf(hbuf, sizeof hbuf, "%s", lines[0]);
  char *hf[64];
  int hn = csv_split(hbuf, hf, 64);
  int iCode = idx_of(hf, hn, "code"), iName = idx_of(hf, hn, "name"),
      iCountry = idx_of(hf, hn, "country"), iLat = idx_of(hf, hn, "latitude"),
      iLon = idx_of(hf, hn, "longitude"), iElev = idx_of(hf, hn, "elevation");
  if (iCode < 0 || iLat < 0 || iLon < 0) { free(lines); free(text); return -1; }

  cJSON *features = cJSON_CreateArray();
  for (int li = 1; li < ln; li++) {
    char *f[64];
    int fn = csv_split(lines[li], f, 64);
    const char *latS = col(f, fn, iLat), *lonS = col(f, fn, iLon);
    if (!latS || !lonS) continue;
    char *e1, *e2;
    double lat = strtod(latS, &e1);
    double lon = strtod(lonS, &e2);
    if (e1 == latS || e2 == lonS) continue; /* !Number.isFinite */

    const char *code = col(f, fn, iCode);
    const char *name = col(f, fn, iName);
    if (!name || !name[0]) name = code; /* cols[iName] || cols[iCode] */
    const char *country = col(f, fn, iCountry);
    const char *elevS = col(f, fn, iElev);
    int hasElev = 0;
    double elev = 0;
    if (elevS) {
      char *ee;
      elev = strtod(elevS, &ee);
      if (ee != elevS) hasElev = 1; /* Number.isFinite(parseFloat(...)) */
    }

    cJSON *feat = gj_point_feature(lon, lat);

    cJSON *p = cJSON_CreateObject(); /* EXACT JS key order */
    char idbuf[96];
    snprintf(idbuf, sizeof idbuf, "WDCGG_%s", code ? code : "");
    cJSON_AddStringToObject(p, "id", idbuf);
    cJSON_AddItemToObject(p, "station_code",
      code ? cJSON_CreateString(code) : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "name",
      name ? cJSON_CreateString(name) : cJSON_CreateNull());
    cJSON_AddNullToObject(p, "name_ja");          /* s.name_ja || null */
    cJSON_AddItemToObject(p, "country",
      (country && country[0]) ? cJSON_CreateString(country)
                              : cJSON_CreateNull());
    cJSON_AddItemToObject(p, "elevation_m",
      hasElev ? cJSON_CreateNumber(elev) : cJSON_CreateNull());
    cJSON_AddNullToObject(p, "species");          /* registry path: null */
    cJSON_AddNullToObject(p, "operator");         /* registry path: null */
    cJSON_AddStringToObject(p, "source", "wdcgg_registry");
    cJSON_AddItemToObject(feat, "properties", p);
    cJSON_AddItemToArray(features, feat);
  }
  free(lines);
  free(text);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[wdcgg-co2] emitted %d\n", n);
  /* run() is a STATUS code, not a row count: fetch/parse failures already
   * returned -1 above, so reaching here with zero rows is an honest empty.
   * Returning -1 here had scheduler.c quarantine the source for working. */
  return 0;
}

static const source_def wdcgg_co2_def = {
  .id = "wdcgg-co2", .collector = "environment",
  .name = "WDCGG GHG Stations", .name_ja = "温室効果ガス観測所",
   .update_interval_sec = 604800, .run = run,
};
REGISTER_SOURCE(wdcgg_co2_def)
