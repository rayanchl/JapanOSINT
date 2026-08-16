/* Taipower per-unit real-time generation (Taiwan).
 * Endpoint: https://service.taipower.com.tw/data/opendata/apply/file/d006001/001.json
 * Keyless. Unit-level visibility is rare: every individual Taiwan Power Company
 * generating unit with its installed capacity and current net output.
 *
 * BOM TRAP: the file starts with a UTF-8 BOM (EF BB BF) — cJSON fails on it, so
 * it is skipped before parsing.
 * CHINESE KEYS, STRING VALUES: aaData[] objects are keyed in Chinese and ALL
 * values are strings, including the numbers:
 *   機組類型   unit type
 *   機組名稱   unit name
 *   裝置容量(MW)            installed capacity, MW
 *   淨發電量(MW)            net generation, MW
 *   淨發電量/裝置容量比(%)  utilisation, a PERCENT with a trailing '%'
 *   備註       remarks (e.g. 部分檢修 = partial maintenance)
 * The percent token carries a trailing '%' which is stripped before parsing;
 * a leading '-' is real (pumped storage consuming), and an offline unit
 * legitimately reports 0 MW — neither is dropped.
 * DateTime is Taiwan local time (UTC+8) with no offset marker.
 * Licence: Taiwan Government Open Data Licence (Taipower open data platform). */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "taipower-unit-generation"

static const char *K_TYPE = "\xe6\xa9\x9f\xe7\xb5\x84\xe9\xa1\x9e\xe5\x9e\x8b";      /* 機組類型 */
static const char *K_NAME = "\xe6\xa9\x9f\xe7\xb5\x84\xe5\x90\x8d\xe7\xa8\xb1";      /* 機組名稱 */
static const char *K_CAP  = "\xe8\xa3\x9d\xe7\xbd\xae\xe5\xae\xb9\xe9\x87\x8f(MW)";  /* 裝置容量(MW) */
static const char *K_GEN  = "\xe6\xb7\xa8\xe7\x99\xbc\xe9\x9b\xbb\xe9\x87\x8f(MW)";  /* 淨發電量(MW) */
static const char *K_RAT  = "\xe6\xb7\xa8\xe7\x99\xbc\xe9\x9b\xbb\xe9\x87\x8f/"
                            "\xe8\xa3\x9d\xe7\xbd\xae\xe5\xae\xb9\xe9\x87\x8f\xe6\xaf\x94(%)";
static const char *K_REM  = "\xe5\x82\x99\xe8\xa8\xbb";                              /* 備註 */

/* strtod tolerant of a trailing '%' and of surrounding blanks */
static int numstr(const char *s, double *out) {
  if (!s) return 0;
  while (*s == ' ' || *s == '\t') s++;
  char *e = NULL;
  double v = strtod(s, &e);
  if (e == s) return 0;
  *out = v;
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = "https://service.taipower.com.tw/data/opendata/apply/file/"
                    "d006001/001.json";
  char *txt = feed_get_text(ctx->http, url, 40000);
  if (!txt) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  /* strip the UTF-8 BOM, which cJSON will not parse past */
  const char *json = txt;
  if ((unsigned char)json[0] == 0xEF && (unsigned char)json[1] == 0xBB &&
      (unsigned char)json[2] == 0xBF) json += 3;
  cJSON *doc = cJSON_Parse(json);
  free(txt);
  if (!doc) { fprintf(stderr, "[" SRC "] parse failed\n"); return -1; }

  const char *when = jo_sv(doc, "DateTime");
  cJSON *rows = cJSON_GetObjectItem(doc, "aaData");
  if (!cJSON_IsArray(rows)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no aaData[]\n"); return -1; }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const char *name = jo_sv(r, K_NAME);
    const char *type = jo_sv(r, K_TYPE);
    if (!name) continue;
    double gen, cap = 0, ratio = 0;
    if (!numstr(jo_sv(r, K_GEN), &gen)) continue;   /* no output figure -> no row */
    int have_cap = numstr(jo_sv(r, K_CAP), &cap);
    int have_ratio = numstr(jo_sv(r, K_RAT), &ratio);   /* trailing '%' ignored */

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "unit_name", name);
    if (type) cJSON_AddStringToObject(p, "unit_type", type);
    cJSON_AddNumberToObject(p, "net_generation_mw", gen);
    cJSON_AddStringToObject(p, "generation_unit", "MW");
    if (have_cap) {
      cJSON_AddNumberToObject(p, "installed_capacity_mw", cap);
      cJSON_AddStringToObject(p, "capacity_unit", "MW");
    }
    if (have_ratio) {
      cJSON_AddNumberToObject(p, "utilisation_percent", ratio);
      cJSON_AddStringToObject(p, "utilisation_unit", "%");
    }
    const char *rem = jo_sv(r, K_REM);
    if (rem) {
      /* remarks are frequently a single space on healthy units */
      const char *t = rem; while (*t == ' ') t++;
      if (*t) cJSON_AddStringToObject(p, "remarks", t);
    }
    if (when) {
      cJSON_AddStringToObject(p, "datetime_local", when);
      cJSON_AddStringToObject(p, "timezone", "Asia/Taipei (UTC+8), no offset in feed");
    }
    cJSON_AddStringToObject(p, "country", "TW");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char title[288];
    if (have_cap)
      snprintf(title, sizeof title, "Taipower %s: %.1f / %.1f MW", name, gen, cap);
    else
      snprintf(title, sizeof title, "Taipower %s: %.1f MW", name, gen);

    intel_item row = {0};
    row.remote_key      = name;
    row.title           = title;
    row.summary         = title;
    row.lang            = "zh";
    row.link            = "https://www.taipower.com.tw/tc/page.aspx?mid=206";
    row.record_type     = "power-plant-unit";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"generation\",\"taiwan\",\"power-plant\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_taipower_units_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "Taipower per-unit real-time generation (Taiwan)",
  .update_interval_sec = 600, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://service.taipower.com.tw/data/opendata/apply/file/d006001/001.json",
  .description = "Real-time output (MW) of every individual Taiwan Power Company generating unit with installed capacity and utilisation — unit-level outage visibility.",
  .license = "Taiwan Government Open Data Licence (Taipower open data platform), keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_taipower_units_def)
