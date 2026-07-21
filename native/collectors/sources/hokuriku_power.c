/* collectors/infrastructure/sources/hokuriku_power.c
 * Port of server/src/collectors/hokurikuPower.js (makeDenkiYohoCollector,
 * server/src/collectors/_denkiYoho.js). Real public juyo CSV → parse the
 * last numeric load row → emit ONE GeoJSON point at the utility HQ with
 * load_mw. Honest empty on fetch/parse failure (RULE 8). */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/geojson.h"
#include "../../lib/probe.h"   /* probe_iso_now: same ISO-8601 as JS Date */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SOURCE_ID  "hokuriku-power"
#define META_SRC   "hokuriku_power"
#define OPERATOR   "北陸電力送配電"
#define CSV_URL    "https://www.rikuden.co.jp/nw/denki-yoho/csv/juyo_05_rikuden.csv"
#define HQ_LAT     36.6953
#define HQ_LON     137.2113

/* JS _denkiYoho.parseLastLoad: split on \r?\n, trim, drop empty; from the
 * last line backward, split on ',', need >=2 cols, parseFloat the LAST col;
 * first finite >0 wins → { load_mw, raw }. */
static char *djtrim(char *s) {
  while (*s==' '||*s=='\t'||*s=='\r'||*s=='\n') s++;
  size_t n = strlen(s);
  while (n && (s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n')) s[--n]=0;
  return s;
}

static int parse_last_load(char *csv, double *load_mw, char **raw_out) {
  if (!csv) return 0;
  /* collect trimmed non-empty lines */
  char *lines[8192]; int nl = 0;
  char *save = NULL;
  for (char *ln = strtok_r(csv, "\n", &save); ln && nl < 8192;
       ln = strtok_r(NULL, "\n", &save)) {
    char *t = djtrim(ln);
    if (*t) lines[nl++] = t;
  }
  for (int i = nl - 1; i >= 0; i--) {
    /* split on ',' counting cols, capture last col */
    char *line = lines[i];
    int cols = 0; const char *last_col = NULL;
    char buf[2048];
    size_t bl = strlen(line);
    if (bl >= sizeof buf) bl = sizeof buf - 1;
    memcpy(buf, line, bl); buf[bl] = 0;
    char *cs = NULL;
    for (char *tok = strtok_r(buf, ",", &cs); tok;
         tok = strtok_r(NULL, ",", &cs)) {
      last_col = tok; cols++;
    }
    if (cols < 2 || !last_col) continue;
    char *end;
    double last = strtod(last_col, &end);
    if (end != last_col && last == last && last > 0) {
      *load_mw = last;
      *raw_out = strdup(line);
      return 1;
    }
  }
  return 0;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *csv = feed_get_text(ctx->http, CSV_URL, 10000);
  double load_mw = 0; char *raw = NULL;
  int ok = parse_last_load(csv, &load_mw, &raw);
  if (csv) free(csv);

  if (!ok) {
    fprintf(stderr, "[" SOURCE_ID "] " META_SRC "_unavailable\n");
    return 0;                       /* honest empty */
  }

  char now[32];
  probe_iso_now(now, sizeof now);   /* === new Date().toISOString() */

  cJSON *features = cJSON_CreateArray();
  cJSON *f = cJSON_CreateObject();
  cJSON_AddStringToObject(f, "type", "Feature");
  cJSON *g = cJSON_CreateObject();
  cJSON_AddStringToObject(g, "type", "Point");
  cJSON *co = cJSON_CreateArray();
  cJSON_AddItemToArray(co, cJSON_CreateNumber(HQ_LON));
  cJSON_AddItemToArray(co, cJSON_CreateNumber(HQ_LAT));
  cJSON_AddItemToObject(g, "coordinates", co);
  cJSON_AddItemToObject(f, "geometry", g);

  cJSON *p = cJSON_CreateObject();              /* EXACT JS key order */
  cJSON_AddStringToObject(p, "source_id", SOURCE_ID);
  cJSON_AddStringToObject(p, "operator", OPERATOR);
  cJSON_AddNumberToObject(p, "load_mw", load_mw);
  cJSON_AddStringToObject(p, "raw", raw ? raw : "");
  cJSON_AddStringToObject(p, "feed_url", CSV_URL);
  cJSON_AddStringToObject(p, "country", "JP");
  cJSON_AddStringToObject(p, "updated_at", now);
  cJSON_AddStringToObject(p, "source", META_SRC);
  cJSON_AddItemToObject(f, "properties", p);
  cJSON_AddItemToArray(features, f);

  int n = geojson_emit_features(sink, ctx->source_id, features);
  cJSON_Delete(features);
  if (raw) free(raw);
  fprintf(stderr, "[" SOURCE_ID "] emitted %d load_mw=%.1f\n", n, load_mw);
  return n >= 0 ? 0 : -1;
}

static const source_def hokuriku_power_def = {
  .id = SOURCE_ID, .collector = "infrastructure",
  .name = "Hokuriku Electric Power", .name_ja = "北陸電力 電力使用状況",
  .update_interval_sec = 300, .run = run };
REGISTER_SOURCE(hokuriku_power_def)
