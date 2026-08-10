/* collectors/sources/ag_commodity.c
 * Scheduled feeds — agriculture and commodity production/price series.
 *
 * WHY THIS FILE EXISTS. Agriculture was the thinnest category in a 2,500-source
 * engine: before this, `agriculture` had a drought raster, a handful of Nigerian
 * farm POIs, some ministry org pages and a few journal RSS feeds — no prices,
 * no yields, no production. These are the series a commodity or food-industry
 * analyst actually opens.
 *
 *   • usda-nass-quickstats    — USDA National Agricultural Statistics Service
 *                               QuickStats: US prices received and yields by
 *                               commodity. Free key; gated on USDA_NASS_KEY.
 *   • worldbank-ag-*          — five World Bank global agricultural indicator
 *                               series (production indices, cereal yield, raw
 *                               materials exports). Keyless.
 *
 * Endpoints (verified live 2026-08-09):
 *   https://quickstats.nass.usda.gov/api/api_GET/?key=…&format=JSON
 *       → {"data":[{short_desc,Value,unit_desc,year,state_name,…}]}
 *       (401 {"error":["unauthorized"]} without a key — hence the R6 gate)
 *   https://api.worldbank.org/v2/country/WLD/indicator/<code>?format=json&mrv=N
 *       → [ {page,pages,…}, [ {indicator:{id,value},country:{id,value},
 *                              countryiso3code,date,value,unit,…} ] ]
 *
 * Licence: USDA NASS QuickStats is US-government public domain. World Bank
 * open data is CC-BY-4.0 — attribution is carried in every row's `source`.
 *
 * R2: neither upstream returns coordinates. NASS rows name a state and World
 * Bank rows name a country, and resolving either to a point would be the
 * area-centroid fabrication the 2026-07-31 audit removed. No row sets has_geo.
 *
 * NOT INCLUDED, deliberately: USDA AMS Market News (marsapi.ams.usda.gov)
 * needs both a key and a per-report slug whose response shape could not be
 * verified without one, and FAOSTAT's parameter names could not be confirmed.
 * Adding either against a guessed schema is an R1 violation even when the URL
 * resolves, so they are left out rather than shipped unverified. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Emit one row. `data` is TAKEN OVER (printed, then deleted). Returns 1 if the
 * sink accepted it. */
static int ag_emit(intel_sink *sink, cJSON *data, const char *record_type,
                   const char *rk, const char *title, const char *summary,
                   const char *link, const char *published, const char *tags) {
  if (!data) return 0;
  char *pj = cJSON_PrintUnformatted(data);
  cJSON_Delete(data);
  if (!pj) return 0;

  intel_item it = {0};
  it.remote_key      = rk;
  it.title           = title;
  it.summary         = summary;
  it.link            = link;
  it.published_at    = published;
  it.lang            = "en";
  it.record_type     = record_type;
  it.body            = pj;
  it.properties_json = pj;
  it.tags_json       = tags;
  int rc = sink->emit(sink, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

/* ---------------------------------------------------- USDA NASS QuickStats */

static int run_nass(const source_ctx *ctx, intel_sink *sink) {
  const char *key = jo_env("USDA_NASS_KEY");
  if (!key) {
    fprintf(stderr, "[usda-nass-quickstats] gated (no USDA_NASS_KEY)\n");
    return 0;                                   /* R6: gate, do not emit a row */
  }
  char since[16];
  jo_days_ago_iso(730, since, sizeof since);

  char *ekey = jo_urlencode(key);
  if (!ekey) return 0;
  char url[900];
  snprintf(url, sizeof url,
           "https://quickstats.nass.usda.gov/api/api_GET/?key=%s"
           "&source_desc=SURVEY&sector_desc=CROPS"
           "&statisticcat_desc=PRICE%%20RECEIVED"
           "&agg_level_desc=NATIONAL&year__GE=%.4s&format=JSON",
           ekey, since);
  free(ekey);

  const char *hdrs[] = { "Accept: application/json", NULL };
  cJSON *root = feed_get_json_h(ctx->http, url, hdrs, 45000);
  if (!root) { fprintf(stderr, "[usda-nass-quickstats] fetch failed\n"); return -1; }

  int n = 0;
  cJSON *arr = cJSON_GetObjectItem(root, "data");
  cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    /* short_desc is NASS's own fully-qualified series label. */
    const char *desc = jo_sv(r, "short_desc");
    const char *val  = jo_sv(r, "Value");
    if (!desc || !val) continue;                /* no label/value → no row (R1) */
    const char *year = jo_sv(r, "year");
    const char *unit = jo_sv(r, "unit_desc");
    const char *period = jo_sv(r, "reference_period_desc");

    cJSON *data = cJSON_Duplicate(r, 1);
    if (!data) continue;
    cJSON_AddStringToObject(data, "source", "USDA NASS QuickStats");

    char rk[420], title[520], pub[16] = {0};
    snprintf(rk, sizeof rk, "nass:%s:%s:%s",
             desc, year ? year : "?", period ? period : "-");
    snprintf(title, sizeof title, "%s — %s%s%s", desc, val,
             unit ? " " : "", unit ? unit : "");
    if (year && strlen(year) == 4) snprintf(pub, sizeof pub, "%s-01-01", year);
    n += ag_emit(sink, data, "ag-statistic", rk, title, unit,
                 "https://quickstats.nass.usda.gov/", pub[0] ? pub : NULL,
                 "[\"agriculture\",\"price\",\"usda\"]");
  }
  cJSON_Delete(root);
  fprintf(stderr, "[usda-nass-quickstats] emitted %d\n", n);
  return 0;
}

/* -------------------------------------------- World Bank indicator series */

/* The World Bank v2 payload is a two-element array: [meta, rows]. Rows nest
 * their label under indicator.value and their subject under country.value, so
 * lib/jsonlist.h's name-precedence mapping finds no top-level title and would
 * emit nothing — hence this small explicit emitter rather than VJSON. */
static int wb_run(const source_ctx *ctx, intel_sink *sink,
                  const char *code, const char *tag) {
  char url[400];
  snprintf(url, sizeof url,
           "https://api.worldbank.org/v2/country/WLD/indicator/%s"
           "?format=json&per_page=60&mrv=20", code);
  const char *hdrs[] = { "Accept: application/json", NULL };
  cJSON *doc = feed_get_json_h(ctx->http, url, hdrs, 30000);
  if (!doc) { fprintf(stderr, "[%s] fetch failed\n", tag); return -1; }

  cJSON *rows = cJSON_IsArray(doc) ? cJSON_GetArrayItem(doc, 1) : NULL;
  int n = 0;
  if (cJSON_IsArray(rows)) {
    cJSON *r;
    cJSON_ArrayForEach(r, rows) {
      /* value is null for years not yet published — skip rather than emit a
       * row asserting an observation that does not exist. */
      cJSON *v = cJSON_GetObjectItem(r, "value");
      if (!cJSON_IsNumber(v)) continue;
      const cJSON *ind = cJSON_GetObjectItem(r, "indicator");
      const cJSON *cty = cJSON_GetObjectItem(r, "country");
      const char *label = jo_sv(ind, "value");
      const char *place = jo_sv(cty, "value");
      const char *year  = jo_sv(r, "date");
      if (!label || !year) continue;

      cJSON *data = cJSON_CreateObject();
      cJSON_AddStringToObject(data, "indicator_code", code);
      cJSON_AddStringToObject(data, "indicator", label);
      jo_add_str(data, "country", place);
      jo_add_str(data, "country_iso3", jo_sv(r, "countryiso3code"));
      cJSON_AddStringToObject(data, "year", year);
      cJSON_AddNumberToObject(data, "value", v->valuedouble);
      jo_add_str(data, "unit", jo_sv(r, "unit"));
      cJSON_AddStringToObject(data, "source", "World Bank Open Data (CC-BY-4.0)");

      char rk[320], title[520], pub[16];
      snprintf(rk, sizeof rk, "wb:%s:%s:%s", code,
               jo_sv(r, "countryiso3code") ? jo_sv(r, "countryiso3code") : "WLD",
               year);
      snprintf(title, sizeof title, "%s — %s %s: %.4g",
               label, place ? place : "World", year, v->valuedouble);
      snprintf(pub, sizeof pub, "%.4s-01-01", year);
      n += ag_emit(sink, data, "ag-indicator", rk, title, place,
                   "https://data.worldbank.org/", pub,
                   "[\"agriculture\",\"indicator\",\"worldbank\"]");
    }
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[%s] emitted %d\n", tag, n);
  return 0;
}

/* One World Bank indicator = one source. The macro keeps each to a single
 * declarative line, the same shape collectors/sources/_verified_macros.inc
 * uses for generated feeds. Daily interval: these series update annually, so
 * anything tighter is pure load on a public API for no new data. */
#define WB_SRC(SYM, ID, CODE, NAME, NAMEJA, DESC)                            \
  static int run_##SYM(const source_ctx *c, intel_sink *s) {                 \
    return wb_run(c, s, CODE, ID); }                                         \
  static const source_def SYM = {                                            \
    .id = ID, .collector = "agriculture", .name = NAME, .name_ja = NAMEJA,   \
    .update_interval_sec = 86400, .run = run_##SYM,                          \
    .category = "agriculture", .type = "api",                                \
    .url = "https://api.worldbank.org/v2/country/WLD/indicator/" CODE,       \
    .description = DESC, .layer = NULL, .free_tier = 1 };                    \
  REGISTER_SOURCE(SYM)

WB_SRC(wb_food_prod, "worldbank-ag-food-production", "AG.PRD.FOOD.XD",
       "World Bank Food Production Index", "世界銀行 食料生産指数",
       "Global food production index (2014-2016 = 100), most recent 20 years.")

WB_SRC(wb_crop_prod, "worldbank-ag-crop-production", "AG.PRD.CROP.XD",
       "World Bank Crop Production Index", "世界銀行 作物生産指数",
       "Global crop production index (2014-2016 = 100), most recent 20 years.")

WB_SRC(wb_lvsk_prod, "worldbank-ag-livestock-production", "AG.PRD.LVSK.XD",
       "World Bank Livestock Production Index", "世界銀行 畜産生産指数",
       "Global livestock production index (2014-2016 = 100), most recent 20 years.")

WB_SRC(wb_cereal_yld, "worldbank-ag-cereal-yield", "AG.YLD.CREL.KG",
       "World Bank Cereal Yield", "世界銀行 穀物単収",
       "Global cereal yield in kg per hectare, most recent 20 years.")

WB_SRC(wb_raw_exports, "worldbank-ag-raw-material-exports", "TX.VAL.AGRI.ZS.UN",
       "World Bank Agricultural Raw Material Exports", "世界銀行 農産原材料輸出比率",
       "Agricultural raw materials as a share of merchandise exports, worldwide.")

static const source_def nass_def = {
  .id = "usda-nass-quickstats", .collector = "agriculture",
  .name = "USDA NASS QuickStats — Prices Received",
  .name_ja = "米国農務省 農産物受取価格統計",
  .update_interval_sec = 86400, .run = run_nass,
  .category = "agriculture", .type = "api",
  .url = "https://quickstats.nass.usda.gov/api",
  .description = "US national prices received for crops from the USDA NASS survey "
                 "programme, last two years (needs free USDA_NASS_KEY).",
  .layer = NULL, .free_tier = 1,
};
REGISTER_SOURCE(nass_def)
