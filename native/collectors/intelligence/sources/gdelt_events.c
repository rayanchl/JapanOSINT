/* collectors/intelligence/sources/gdelt_events.c
 * Port of server/src/collectors/gdelt.js (registry id "gdelt-events").
 * GDELT 2.0 raw 15-min export: lastupdate.txt → .export.CSV.zip → unzip
 * (lib/zipread) → tab-separated rows, filter ActionGeo_CountryCode=="JA",
 * geocoded→Point / ungeocoded→geometry:null (the geojson sink routes the
 * latter to the intel store, same as Node mirrorCollectorOutput). _meta
 * dropped per RULE 8. GDELT_SLICES env (1..96, default 1) walks back N
 * consecutive 15-min slices, exactly as JS. */
#include "../../../source.h"
#include "../../../core/httpclient.h"
#include "../../../lib/zipread.h"
#include "../../../lib/geojson.h"
#include "../../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUCKET "http://data.gdeltproject.org/gdeltv2"
#define JAPAN_FIPS "JA"
#define MAX_SLICES 96

/* number-or-null cJSON from a CSV cell: "" → null, else parsed (JS parseFloat
 * /parseInt with `s ? ... : null`). is_int truncates like parseInt. */
static cJSON *num_or_null(const char *s, int is_int) {
  if (!s || !*s) return cJSON_CreateNull();
  double d = strtod(s, NULL);
  return cJSON_CreateNumber(is_int ? (double)(long)d : d);
}
static cJSON *str_or_null(const char *s) {
  return (s && *s) ? cJSON_CreateString(s) : cJSON_CreateNull();
}

/* "YYYYMMDDHHMMSS" → "YYYY-MM-DDTHH:MM:SSZ" or JSON null. */
static cJSON *date_added(const char *s) {
  if (!s || strlen(s) < 14) return cJSON_CreateNull();
  char o[24];
  snprintf(o, sizeof o, "%.4s-%.2s-%.2sT%.2s:%.2s:%.2sZ",
           s, s+4, s+6, s+8, s+10, s+12);
  return cJSON_CreateString(o);
}

/* rowsToFeatures: tab-split, need >=61 cols, col53=="JA". */
static void rows_to_features(char *csv, cJSON *features) {
  for (char *line = strtok(csv, "\n"); line; line = strtok(NULL, "\n")) {
    if (!*line) continue;
    char *col[61]; int n = 0;
    for (char *p = line; n < 61; ) {
      col[n++] = p;
      char *t = strchr(p, '\t');
      if (!t) break;
      *t = 0; p = t + 1;
    }
    if (n < 61) continue;
    if (strcmp(col[53], JAPAN_FIPS) != 0) continue;

    char *eptr_lat = NULL, *eptr_lon = NULL;
    double lat = col[56][0] ? strtod(col[56], &eptr_lat) : 0;
    double lon = col[57][0] ? strtod(col[57], &eptr_lon) : 0;
    int geo = (eptr_lat && eptr_lat != col[56]) &&
              (eptr_lon && eptr_lon != col[57]);

    cJSON *f = cJSON_CreateObject();
    cJSON_AddStringToObject(f, "type", "Feature");
    if (geo) {
      cJSON *g = cJSON_CreateObject();
      cJSON_AddStringToObject(g, "type", "Point");
      cJSON *c = cJSON_CreateArray();
      cJSON_AddItemToArray(c, cJSON_CreateNumber(lon));
      cJSON_AddItemToArray(c, cJSON_CreateNumber(lat));
      cJSON_AddItemToObject(g, "coordinates", c);
      cJSON_AddItemToObject(f, "geometry", g);
    } else {
      cJSON_AddItemToObject(f, "geometry", cJSON_CreateNull());
    }
    cJSON *avg = num_or_null(col[34], 0);
    cJSON *da  = date_added(col[59]);
    cJSON *p = cJSON_CreateObject();          /* EXACT JS key order */
    cJSON_AddItemToObject(p, "event_id", str_or_null(col[0]));
    cJSON_AddItemToObject(p, "name", str_or_null(col[52]));
    cJSON_AddItemToObject(p, "location_name", str_or_null(col[52]));
    cJSON_AddItemToObject(p, "event_code", str_or_null(col[26]));
    cJSON_AddItemToObject(p, "event_base_code", str_or_null(col[27]));
    cJSON_AddItemToObject(p, "event_root_code", str_or_null(col[28]));
    cJSON_AddItemToObject(p, "goldstein_scale", num_or_null(col[30], 0));
    cJSON_AddItemToObject(p, "num_mentions", num_or_null(col[31], 1));
    cJSON_AddItemToObject(p, "num_sources", num_or_null(col[32], 1));
    cJSON_AddItemToObject(p, "num_articles", num_or_null(col[33], 1));
    cJSON_AddItemToObject(p, "avg_tone", cJSON_Duplicate(avg, 1));
    cJSON_AddItemToObject(p, "tone", avg);
    cJSON_AddStringToObject(p, "country", "JP");
    cJSON_AddItemToObject(p, "country_code", str_or_null(col[53]));
    cJSON_AddItemToObject(p, "date_added", cJSON_Duplicate(da, 1));
    cJSON_AddItemToObject(p, "timestamp", da);
    cJSON_AddItemToObject(p, "url", str_or_null(col[60]));
    cJSON_AddStringToObject(p, "source", "gdelt");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);
  }
}

/* binary GET → malloc'd buffer + *len (zip is binary; feed_get_text would
 * NUL-truncate). Caller frees. */
static char *http_bytes(http_client *h, const char *url, size_t *len) {
  http_response r = {0};
  if (http_request(h, "GET", url, NULL, NULL, 0, 20000, 1, &r) != 0 ||
      r.status < 200 || r.status >= 300) { http_response_free(&r); return NULL; }
  char *b = r.body; *len = r.body_len; r.body = NULL;   /* take ownership */
  http_response_free(&r);
  return b;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *se = getenv("GDELT_SLICES");
  int slices = se ? atoi(se) : 1;
  if (slices < 1) slices = 1;
  if (slices > MAX_SLICES) slices = MAX_SLICES;

  /* index: first non-empty line "<size> <md5> <url>" */
  http_response ir = {0};
  if (http_request(ctx->http, "GET", BUCKET "/lastupdate.txt", NULL, NULL, 0,
                   20000, 1, &ir) != 0 || ir.status != 200 || !ir.body) {
    http_response_free(&ir);
    cJSON *e = cJSON_CreateArray();
    int z = geojson_emit_features(sink, "gdelt-events", e);
    cJSON_Delete(e); return z >= 0 ? 0 : -1;
  }
  char latest[512] = {0};
  for (char *ln = strtok(ir.body, "\n"); ln; ln = strtok(NULL, "\n")) {
    if (!*ln) continue;
    char *parts[4]; int pn = 0;
    for (char *p = ln; pn < 3 && p; ) {
      while (*p == ' ' || *p == '\t') p++;
      parts[pn++] = p;
      while (*p && *p != ' ' && *p != '\t') p++;
      if (*p) *p++ = 0;
    }
    if (pn >= 3) snprintf(latest, sizeof latest, "%s", parts[2]);
    break;
  }
  http_response_free(&ir);
  size_t L = strlen(latest);
  if (L < 15 || strcmp(latest + L - 15, ".export.CSV.zip") != 0) {
    cJSON *e = cJSON_CreateArray();
    int z = geojson_emit_features(sink, "gdelt-events", e);
    cJSON_Delete(e); return z >= 0 ? 0 : -1;
  }

  /* priorSliceUrls: latest + walk back i*15min on the 14-digit stamp. */
  cJSON *features = cJSON_CreateArray();
  char *stamp = latest + L - 15 - 14;          /* 14 digits before .export */
  struct tm tm0; memset(&tm0, 0, sizeof tm0);
  int have_t0 = (strptime(stamp, "%Y%m%d%H%M%S", &tm0) != NULL);
  time_t t0 = have_t0 ? timegm(&tm0) : 0;

  for (int i = 0; i < slices; i++) {
    char url[512];
    if (i == 0) snprintf(url, sizeof url, "%s", latest);
    else if (have_t0) {
      time_t t = t0 - (time_t)i * 15 * 60;
      struct tm g; gmtime_r(&t, &g);
      char st[16]; strftime(st, sizeof st, "%Y%m%d%H%M%S", &g);
      snprintf(url, sizeof url, "%s/%s.export.CSV.zip", BUCKET, st);
    } else break;
    size_t zl = 0;
    char *zip = http_bytes(ctx->http, url, &zl);
    if (!zip) continue;
    size_t cl = 0;
    char *csv = zip_first_entry(zip, zl, &cl);
    free(zip);
    if (!csv) continue;
    rows_to_features(csv, features);
    free(csv);
  }

  int n = geojson_emit_features(sink, "gdelt-events", features);
  cJSON_Delete(features);
  fprintf(stderr, "[gdelt-events] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def gdelt_events_def = {
  .id = "gdelt-events", .collector = "intelligence",
  .name = "GDELT Global Events", .name_ja = "GDELT グローバル・イベント",
   .update_interval_sec = 900, .run = run };
REGISTER_SOURCE(gdelt_events_def)
