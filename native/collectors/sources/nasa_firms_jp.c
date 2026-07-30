/* collectors/environment/sources/nasa_firms_jp.c — port of
 * server/src/collectors/nasaFirmsJp.js.
 * NASA FIRMS area CSV (requires NASA_FIRMS_MAP_KEY / FIRMS_MAP_KEY; free).
 * No key / fetch error / <2 lines → 0 features (SEED/_meta dropped, rule 7).
 * Header-indexed columns; Number() coercion → NaN becomes JSON null for the
 * numeric props (bright/frp); geometry null when lon/lat not finite.
 * Props order: bright, acq_date, acq_time, confidence, frp, satellite, source. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../lib/csv.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TIMEOUT_MS 20000

/* header.indexOf(name) — exact string match on a trimmed header cell. */
static int idx_of(cJSON *hdr, const char *name) {
  int i = 0;
  cJSON *c;
  cJSON_ArrayForEach(c, hdr) {
    if (cJSON_IsString(c) && strcmp(c->valuestring, name) == 0) return i;
    i++;
  }
  return -1;
}

static const char *cell(cJSON *row, int i) {
  if (i < 0) return NULL;
  cJSON *c = cJSON_GetArrayItem(row, i);
  return (c && cJSON_IsString(c)) ? c->valuestring : NULL;
}

/* JS Number(x): "" or non-numeric → NaN. We emit number or JSON null. */
static cJSON *num_or_null(const char *s) {
  if (!s || !*s) return cJSON_CreateNull();
  char *e;
  double v = strtod(s, &e);
  if (e == s) return cJSON_CreateNull();
  return cJSON_CreateNumber(v);
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *key = getenv("NASA_FIRMS_MAP_KEY");
  if (!key || !*key) key = getenv("FIRMS_MAP_KEY");
  if (!key || !*key) {
    fprintf(stderr, "[nasa-firms-jp] no NASA_FIRMS_MAP_KEY — 0 rows\n");
    return 0;
  }
  const char *sensor = getenv("FIRMS_SENSOR");
  if (!sensor || !*sensor) sensor = "VIIRS_NOAA20_NRT";
  int days = 1;
  const char *ds = getenv("FIRMS_DAYS");
  if (ds && *ds) { int d = atoi(ds); days = d > 10 ? 10 : d; }

  char url[512];
  snprintf(url, sizeof url,
    "https://firms.modaps.eosdis.nasa.gov/api/area/csv/%s/%s/JPN/%d",
    key, sensor, days);

  char *csv = feed_get_text(ctx->http, url, TIMEOUT_MS);
  if (!csv) {
    fprintf(stderr, "[nasa-firms-jp] fetch failed\n");
    return -1;
  }

  /* csv.split(/\r?\n/) ; need >=2 lines (header + data). csv_parse with
   * headers=0 returns array of rows of string cells; row 0 is the header. */
  cJSON *rows = csv_parse(csv, 0);
  free(csv);
  int nrows = cJSON_GetArraySize(rows);
  if (nrows < 2) { cJSON_Delete(rows); return 0; }

  cJSON *hdr = cJSON_GetArrayItem(rows, 0);  /* exhaustive-ok: CSV header row, not a record */
  int iLat = idx_of(hdr, "latitude"), iLon = idx_of(hdr, "longitude");
  int iBright = idx_of(hdr, "bright_ti4");
  if (iBright == -1) iBright = idx_of(hdr, "brightness");
  int iAcq = idx_of(hdr, "acq_date"), iAcqT = idx_of(hdr, "acq_time");
  int iConf = idx_of(hdr, "confidence"), iFrp = idx_of(hdr, "frp");
  int iSat = idx_of(hdr, "satellite");

  cJSON *features = cJSON_CreateArray();
  for (int li = 1; li < nrows; li++) {
    cJSON *row = cJSON_GetArrayItem(rows, li);
    /* JS: line.trim(); skip empty (a 1-cell empty row) */
    if (cJSON_GetArraySize(row) <= 1) {
      const char *only = cell(row, 0);
      if (!only || !*only) continue;
    }
    const char *lonS = cell(row, iLon), *latS = cell(row, iLat);
    char *e1 = NULL, *e2 = NULL;
    double lon = lonS ? strtod(lonS, &e1) : 0;
    double lat = latS ? strtod(latS, &e2) : 0;
    int geocoded = lonS && latS && e1 != lonS && e2 != latS && *e1 == 0 && *e2 == 0;

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    if (geocoded) {
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *co = cJSON_CreateArray();
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lon));
      cJSON_AddItemToArray(co, cJSON_CreateNumber(lat));
      cJSON_AddItemToObject(g, "coordinates", co);
      cJSON_AddItemToObject(f, "geometry", g);
    } else {
      cJSON_AddItemToObject(f, "geometry", cJSON_CreateNull());
    }
    cJSON *p = cJSON_CreateObject();
    cJSON_AddItemToObject(p, "bright", num_or_null(cell(row, iBright)));
    const char *acq = cell(row, iAcq);
    cJSON_AddStringToObject(p, "acq_date", acq ? acq : "");
    const char *acqt = cell(row, iAcqT);
    cJSON_AddStringToObject(p, "acq_time", acqt ? acqt : "");
    const char *conf = cell(row, iConf);
    cJSON_AddStringToObject(p, "confidence", conf ? conf : "");
    cJSON_AddItemToObject(p, "frp", num_or_null(cell(row, iFrp)));
    const char *sat = cell(row, iSat);
    cJSON_AddStringToObject(p, "satellite", sat ? sat : "");
    cJSON_AddStringToObject(p, "source", "firms_active_fire");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
  cJSON_Delete(rows);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[nasa-firms-jp] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def nasa_firms_jp_def = {
  .id = "nasa-firms-jp", .collector = "environment",
  .name = "NASA FIRMS active fires (JP)", .name_ja = "NASA FIRMS 活火災 日本",
   .update_interval_sec = 1800, .run = run };
REGISTER_SOURCE(nasa_firms_jp_def)
