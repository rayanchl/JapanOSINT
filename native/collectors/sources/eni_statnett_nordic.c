/* Statnett (Norwegian TSO) live Nordic/Baltic production-consumption overview.
 * Endpoint: https://driftsdata.statnett.no/restapi/ProductionConsumption/
 *           GetLatestDetailedOverview                                (keyless)
 * Emits one row per (country, metric) actually returned:
 *   value_mw (UNIT: MW), metric (production|nuclear|hydro|thermal|wind|
 *   not_specified|consumption|net_exchange), country code, measuredAt.
 *
 * STRING-NUMBER TRAP (quoted from the source survey): "MEASUREMENT IS A STRING,
 * NOT A NUMBER: value is e.g. \"14 942\" with a non-breaking/thin space as
 * thousands separator, and \"-\" where the country has none of that technology.
 * Strip U+00A0/U+2009/space before atof and treat \"-\" as missing, not zero."
 * atof("14 582") returns 14, which is the exact unit/column error the audit
 * found — parse_mw() below strips U+00A0 (C2 A0), U+2009 (E2 80 89), U+202F
 * (E2 80 AF), ASCII space and ASCII apostrophe before conversion.
 *
 * KEYING: each *Data[] element carries its own countryCode; we key off that,
 * never off array index (index 0 is the row LABEL, not a country).
 * Licence: Statnett driftsdata REST API, public, no key. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "statnett-nordic-overview"

static const struct { const char *key, *metric; } SERIES[] = {
  { "ProductionData",   "production"    },
  { "NuclearData",      "nuclear"       },
  { "HydroData",        "hydro"         },
  { "ThermalData",      "thermal"       },
  { "WindData",         "wind"          },
  { "NotSpecifiedData", "not_specified" },
  { "ConsumptionData",  "consumption"   },
  { "NetExchangeData",  "net_exchange"  },
  { NULL, NULL }
};

/* Strip thousands separators (ASCII space, U+00A0, U+2009, U+202F, ') and
 * parse. Returns 1 and writes *out on success; 0 when the token is missing
 * ("-", "", nothing numeric) — missing is NOT zero. */
static int parse_mw(const char *s, double *out) {
  if (!s) return 0;
  char buf[64]; size_t j = 0;
  for (const unsigned char *p = (const unsigned char *)s; *p && j + 1 < sizeof buf; ) {
    if (*p == 0xC2 && p[1] == 0xA0) { p += 2; continue; }              /* NBSP */
    if (*p == 0xE2 && p[1] == 0x80 && (p[2] == 0x89 || p[2] == 0xAF)) { p += 3; continue; }
    if (*p == ' ' || *p == '\'' || *p == '\t') { p++; continue; }
    buf[j++] = (char)*p++;
  }
  buf[j] = 0;
  if (!j) return 0;
  if (strcmp(buf, "-") == 0) return 0;                     /* "none of that" */
  char *end = NULL;
  double v = strtod(buf, &end);
  if (end == buf) return 0;
  *out = v;
  return 1;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *url = "https://driftsdata.statnett.no/restapi/"
                    "ProductionConsumption/GetLatestDetailedOverview";
  cJSON *doc = feed_get_json(ctx->http, url, 25000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  int n = 0, series_found = 0;
  for (int s = 0; SERIES[s].key; s++) {
    cJSON *arr = cJSON_GetObjectItem(doc, SERIES[s].key);
    if (!cJSON_IsArray(arr)) continue;
    series_found = 1;
    int idx = 0;
    cJSON *el;
    cJSON_ArrayForEach(el, arr) {
      int here = idx++;
      if (here == 0) continue;                       /* element 0 = row label */
      const char *cc = jo_sv(el, "countryCode");
      const char *when = jo_sv(el, "measuredAt");
      double mw;
      if (!parse_mw(jo_sv(el, "value"), &mw)) continue; /* "-" / null -> no row */
      const char *scope = cc ? cc : "NORDIC_TOTAL";

      cJSON *p = cJSON_CreateObject();
      cJSON_AddStringToObject(p, "area", scope);
      cJSON_AddStringToObject(p, "metric", SERIES[s].metric);
      cJSON_AddNumberToObject(p, "value_mw", mw);
      cJSON_AddStringToObject(p, "unit", "MW");
      if (when) cJSON_AddStringToObject(p, "measured_at", when);
      const char *ps = jo_sv(el, "periodStart"), *pe = jo_sv(el, "periodEnd");
      if (ps) cJSON_AddStringToObject(p, "period_start", ps);
      if (pe) cJSON_AddStringToObject(p, "period_end", pe);
      char *pj = cJSON_PrintUnformatted(p);
      cJSON_Delete(p);

      char key[96], title[192];
      snprintf(key, sizeof key, "%s|%s|%s", scope, SERIES[s].metric,
               when ? when : "");
      snprintf(title, sizeof title, "Statnett %s %s = %.0f MW",
               scope, SERIES[s].metric, mw);

      intel_item row = {0};
      row.remote_key      = key;
      row.title           = title;
      row.summary         = title;
      row.published_at    = when;
      row.lang            = "en";
      row.link            = url;
      row.record_type     = "grid-overview";
      row.properties_json = pj;
      row.tags_json       = "[\"energy\",\"grid\",\"nordic\",\"statnett\"]";
      if (sink->emit(sink, &row) >= 0) n++;
      free(pj);
    }
  }
  cJSON_Delete(doc);
  if (!series_found) { fprintf(stderr, "[" SRC "] no *Data[] series\n"); return -1; }
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_statnett_nordic_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "Statnett Nordic production/consumption overview",
  .update_interval_sec = 900, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://driftsdata.statnett.no/restapi/ProductionConsumption/GetLatestDetailedOverview",
  .description = "Live Nordic/Baltic power picture from the Norwegian TSO: production, nuclear, hydro, thermal, wind, consumption and net exchange in MW for SE, DK, NO, FI, EE, LT, LV plus totals.",
  .license = "Statnett driftsdata REST API, public, no key.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_statnett_nordic_def)
