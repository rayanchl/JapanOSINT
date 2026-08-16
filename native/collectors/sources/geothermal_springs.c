/* collectors/industry/sources/geothermal_springs.c
 * Port of server/src/collectors/gsjGeothermalSprings.js
 * (fetchText KML + regex Placemark/coordinates/CDATA-description parse).
 * No SEED fallback in the JS (empty features on fetch fail) — HARD RULE 8 ok.
 * HTML_SCRAPE family — mirrors unesco_heritage.c shape. */
#include "source.h"
#include "lib/feedlib.h"
#include "lib/htmlparse.h"
#include "lib/geojson.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define KML_URL "https://gbank.gsj.jp/gres-db/download/onsen/GSJ_DB_GRES-DB_ONSEN_2020.kml"

static int in_japan_bbox(double lon, double lat) {
  return lon >= 122 && lon <= 154 && lat >= 24 && lat <= 46;
}

/* JS pickRow: new RegExp(`${label}\\s*[：:]\\s*([^<\\n]+?)<br`).
 * Returns malloc'd trimmed string, or NULL if no match / empty / "N/A". */
static char *pick_row(const char *desc, const char *label) {
  const char *p = desc;
  size_t llen = strlen(label);
  while ((p = strstr(p, label)) != NULL) {
    const char *q = p + llen;
    while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
    /* require a ： (U+FF1A: ef bc 9a) or ASCII ':' separator */
    if ((unsigned char)q[0] == 0xEF && (unsigned char)q[1] == 0xBC &&
        (unsigned char)q[2] == 0x9A) {
      q += 3;
    } else if (*q == ':') {
      q += 1;
    } else {
      p += llen;
      continue;
    }
    while (*q == ' ' || *q == '\t' || *q == '\r' || *q == '\n') q++;
    /* capture [^<\n]+? up to "<br" (non-greedy: first <br) */
    const char *start = q;
    const char *end = NULL;
    for (const char *r = q; *r; r++) {
      if (*r == '<' || *r == '\n') {
        if (r[0] == '<' && (r[1] == 'b' || r[1] == 'B') &&
            (r[2] == 'r' || r[2] == 'R')) { end = r; }
        break;
      }
    }
    if (!end || end <= start) { p += llen; continue; }
    size_t len = (size_t)(end - start);
    char *raw = (char *)malloc(len + 1);
    if (!raw) return NULL;
    memcpy(raw, start, len);
    raw[len] = 0;
    /* trim */
    char *a = raw;
    while (*a == ' ' || *a == '\t' || *a == '\r' || *a == '\n') a++;
    char *b = a + strlen(a);
    while (b > a && (b[-1] == ' ' || b[-1] == '\t' || b[-1] == '\r' ||
                     b[-1] == '\n')) b--;
    *b = 0;
    if (a[0] == 0 || strcmp(a, "N/A") == 0) { free(raw); return NULL; }
    char *out = strdup(a);
    free(raw);
    return out;
  }
  return NULL;
}

static cJSON *pick_row_json(const char *desc, const char *label) {
  char *s = pick_row(desc, label);
  if (!s) return cJSON_CreateNull();
  cJSON *v = cJSON_CreateString(s);
  free(s);
  return v;
}

static cJSON *pick_number_json(const char *desc, const char *label) {
  char *s = pick_row(desc, label);
  if (!s) return cJSON_CreateNull();
  char *endp = NULL;
  double n = strtod(s, &endp);
  cJSON *v;
  if (endp != s && isfinite(n)) v = cJSON_CreateNumber(n);
  else v = cJSON_CreateNull();
  free(s);
  return v;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *kml = feed_get_text(ctx->http, KML_URL, 60000);
  if (!kml) return -1;  /* JS: empty features on fetch fail */

  cJSON *features = cJSON_CreateArray();
  const char *cur = kml;
  const char *inner; int ilen;
  long i = 0;
  while ((cur = html_block(cur, "Placemark", &inner, &ilen)) != NULL) {
    char *block = strndup(inner, (size_t)ilen);
    if (!block) continue;

    char coords[512] = {0};
    if (!html_tag(block, "coordinates", coords, sizeof coords)) {
      free(block); continue;
    }
    /* trim, take first whitespace-delimited tuple */
    char *cp = coords;
    while (*cp == ' ' || *cp == '\t' || *cp == '\r' || *cp == '\n') cp++;
    char tuple[256] = {0};
    size_t ti = 0;
    while (cp[ti] && cp[ti] != ' ' && cp[ti] != '\t' && cp[ti] != '\r' &&
           cp[ti] != '\n' && ti < sizeof tuple - 1) { tuple[ti] = cp[ti]; ti++; }
    tuple[ti] = 0;
    char *comma = strchr(tuple, ',');
    if (!comma) { free(block); continue; }
    *comma = 0;
    double lon = atof(tuple);
    double lat = atof(comma + 1);
    if (!isfinite(lon) || !isfinite(lat) || tuple[0] == 0 ||
        comma[1] == 0) { free(block); continue; }
    if (!in_japan_bbox(lon, lat)) { free(block); continue; }

    /* description CDATA */
    char descbuf[8192] = {0};
    const char *desc = "";
    char *descraw = NULL;
    if (html_tag(block, "description", descbuf, sizeof descbuf)) {
      char *d = descbuf;
      char *cds = strstr(d, "<![CDATA[");
      if (cds) {
        d = cds + 9;
        char *cde = strstr(d, "]]>");
        if (cde) *cde = 0;
      }
      descraw = strdup(d);
      desc = descraw ? descraw : "";
    }

    i += 1;
    char sid[32];
    snprintf(sid, sizeof sid, "SPRING_%05ld", i);

    cJSON *f = gj_point_feature(lon, lat);

    cJSON *p = cJSON_CreateObject();  /* EXACT JS key order */
    cJSON_AddStringToObject(p, "spring_id", sid);
    cJSON_AddItemToObject(p, "name_ja", pick_row_json(desc, "\xE6\xB8\xA9\xE6\xB3\x89\xE5\x90\x8D"));
    cJSON_AddItemToObject(p, "source_name_ja", pick_row_json(desc, "\xE6\xB3\x89\xE6\xBA\x90\xE5\x90\x8D"));
    cJSON_AddItemToObject(p, "prefecture", pick_row_json(desc, "\xE9\x83\xBD\xE9\x81\x93\xE5\xBA\x9C\xE7\x9C\x8C"));
    cJSON_AddItemToObject(p, "municipality", pick_row_json(desc, "\xE5\xB8\x82\xE5\x8C\xBA\xE7\x94\xBA\xE6\x9D\x91"));
    cJSON_AddItemToObject(p, "depth_m", pick_number_json(desc, "\xE6\xB7\xB1\xE5\xBA\xA6"));
    cJSON_AddItemToObject(p, "temperature_c", pick_number_json(desc, "\xE6\xB3\x89\xE6\xB8\xA9"));
    cJSON_AddItemToObject(p, "flow_l_min", pick_number_json(desc, "\xE6\xB9\xA7\xE5\x87\xBA\xE9\x87\x8F"));
    cJSON_AddItemToObject(p, "ph", pick_number_json(desc, "pH"));
    cJSON_AddItemToObject(p, "tds_mg_kg", pick_number_json(desc, "TSM"));
    cJSON_AddItemToObject(p, "sample_date", pick_row_json(desc, "\xE6\x8E\xA1\xE6\xB0\xB4\xE5\xB9\xB4\xE6\x9C\x88\xE6\x97\xA5"));
    cJSON_AddStringToObject(p, "country", "JP");
    cJSON_AddStringToObject(p, "source", "gsj_gres_db_onsen_2020");
    cJSON_AddItemToObject(f, "properties", p);
    cJSON_AddItemToArray(features, f);

    free(descraw);
    free(block);
  }
  free(kml);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  fprintf(stderr, "[geothermal-springs] emitted %d\n", n);
  return n >= 0 ? 0 : -1;
}

static const source_def geothermal_springs_def = {
  .id = "geothermal-springs", .collector = "industry",
  .name = "Geothermal Springs", .name_ja = "\xE6\xB8\xA9\xE6\xB3\x89\xE5\x8C\x96\xE5\xAD\xA6\xE5\x88\x86\xE6\x9E\x90\xE7\x82\xB9",
   .update_interval_sec = 86400, .run = run };
REGISTER_SOURCE(geothermal_springs_def)
