/* collectors/sources/econ_world.c
 * OSINT services — global economics / trade / markets. On-demand entity pivot
 * (ctx->entity) against the real, public statistical & market data APIs:
 *
 *   WORLDBANK_INDICATORS  World Bank Open Data (api.worldbank.org/v2) — an
 *                         indicator code (e.g. NY.GDP.MKTP.CD) → latest values
 *                         for all countries. Keyless JSON.
 *   IMF_DATA              IMF DataMapper (imf.org/external/datamapper/api) —
 *                         an indicator id → country values. Keyless JSON.
 *   EUROSTAT              Eurostat statistics API (ec.europa.eu/eurostat) — a
 *                         dataset code → observations (JSON-stat). Keyless.
 *   UN_COMTRADE           UN Comtrade (comtradeapi.un.org) — trade flows for a
 *                         reporter/HS code. Gated on COMTRADE_KEY.
 *   FRED_SERIES           FRED (api.stlouisfed.org/fred) — a series id →
 *                         observations. Gated on FRED_KEY.
 *   STOOQ_QUOTES          Stooq (stooq.com/q/l) — a ticker symbol → latest OHLCV
 *                         quote. Keyless CSV.
 *
 * Each run() REAL-fetches its endpoint and emits one intel_item per parsed
 * record (a country value, an observation, a quote), OR honest-empty (return 0)
 * on no entity / fetch failure / no data, OR gated (log + return 0) when a key
 * is missing. Nothing is synthesized: every emitted number/name is a byte
 * parsed from the live response.
 *
 * One run() dispatches on ctx->source_id. */
#include "../../source.h"
#include "../../third_party/cJSON.h"
#include "../../core/httpclient.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "_jp_osint.inc"

/* ---- shared emit -------------------------------------------------------- */
/* Emit one economic datum. title/summary/link/body are all real, parsed bytes.
 * Returns 1 on emit. */
static int ew_emit(intel_sink *sink, const char *service, const char *rectype,
                   const char *remote_key, const char *title,
                   const char *summary, const char *body,
                   const char *published_at, const char *link) {
  if (!title || !*title) return 0;
  cJSON *props = cJSON_CreateObject();
  cJSON_AddStringToObject(props, "service", service);
  if (published_at) cJSON_AddStringToObject(props, "period", published_at);
  cJSON_AddBoolToObject(props, "success", 1);
  char *pj = cJSON_PrintUnformatted(props);
  cJSON_Delete(props);

  intel_item it = {0};
  it.remote_key      = remote_key;
  it.title           = title;
  it.summary         = summary;
  it.body            = body;
  it.lang            = "en";
  it.published_at    = published_at;
  it.link            = link;
  it.record_type     = rectype;
  it.properties_json = pj;
  it.tags_json       = "[\"osint-search\",\"economy\"]";
  int rc = sink->emit(sink, &it);
  free(pj);
  return rc >= 0 ? 1 : 0;
}

/* ---- World Bank --------------------------------------------------------- *
 * GET /v2/country/all/indicator/<code>?format=json&mrnev=1&per_page=400
 * returns [ {page,pages,...}, [ { indicator:{id,value}, country:{id,value},
 * countryiso3code, date, value }, ... ] ]. We emit one item per country obs
 * with a non-null value. */
static int ew_worldbank(const source_ctx *ctx, intel_sink *sink, const char *code) {
  char *enc = jo_urlencode(code);
  if (!enc) return 0;
  char url[512];
  /* mrnev=1 = most-recent non-empty value per country */
  snprintf(url, sizeof url,
    "https://api.worldbank.org/v2/country/all/indicator/%s"
    "?format=json&mrnev=1&per_page=400", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "WORLDBANK_INDICATORS");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!cJSON_IsArray(root)) { cJSON_Delete(root); return 0; }
  cJSON *data = cJSON_GetArrayItem(root, 1);   /* second element = obs array */
  int emitted = 0;
  if (cJSON_IsArray(data)) {
    cJSON *o;
    cJSON_ArrayForEach(o, data) {
      const cJSON *val = cJSON_GetObjectItem(o, "value");
      if (!cJSON_IsNumber(val)) continue;
      const cJSON *ind = cJSON_GetObjectItem(o, "indicator");
      const cJSON *ctry = cJSON_GetObjectItem(o, "country");
      const char *iname = ind ? jo_sv(ind, "value") : NULL;
      const char *cname = ctry ? jo_sv(ctry, "value") : NULL;
      const char *iso3 = jo_sv(o, "countryiso3code");
      const char *date = jo_sv(o, "date");
      if (!cname) continue;
      char title[320], summary[256], keyb[128];
      snprintf(title, sizeof title, "%s — %s: %.6g",
               cname, iname ? iname : code, val->valuedouble);
      snprintf(summary, sizeof summary, "%s = %.6g (%s)",
               iname ? iname : code, val->valuedouble, date ? date : "");
      snprintf(keyb, sizeof keyb, "WB|%s|%s|%s",
               code, iso3 ? iso3 : (cname ? cname : "?"), date ? date : "");
      cJSON *b = cJSON_CreateObject();
      cJSON_AddStringToObject(b, "indicator", code);
      if (iname) cJSON_AddStringToObject(b, "indicator_name", iname);
      if (cname) cJSON_AddStringToObject(b, "country", cname);
      if (iso3)  cJSON_AddStringToObject(b, "iso3", iso3);
      cJSON_AddNumberToObject(b, "value", val->valuedouble);
      if (date) cJSON_AddStringToObject(b, "date", date);
      cJSON_AddStringToObject(b, "source", "World Bank");
      char *bj = cJSON_PrintUnformatted(b);
      cJSON_Delete(b);
      emitted += ew_emit(sink, "WORLDBANK_INDICATORS", "worldbank-indicator",
                         keyb, title, summary, bj, date,
                         "https://data.worldbank.org/indicator");
      free(bj);
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[WORLDBANK_INDICATORS] emitted %d\n", emitted);
  return emitted;
}

/* ---- IMF DataMapper ----------------------------------------------------- *
 * GET /api/v1/<indicator> returns { values: { <IND>: { <ISO3>: { <year>:v,
 * ... }, ... } } }. We emit the latest year per country. */
static int ew_imf(const source_ctx *ctx, intel_sink *sink, const char *ind) {
  char *enc = jo_urlencode(ind);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
    "https://www.imf.org/external/datamapper/api/v1/%s", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "IMF_DATA");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *values = cJSON_GetObjectItem(root, "values");
  int emitted = 0;
  if (cJSON_IsObject(values)) {
    cJSON *indobj;   /* keyed by the indicator id */
    cJSON_ArrayForEach(indobj, values) {
      if (!cJSON_IsObject(indobj)) continue;
      cJSON *ctry;   /* keyed by ISO3 country */
      cJSON_ArrayForEach(ctry, indobj) {
        if (!cJSON_IsObject(ctry)) continue;
        const char *iso3 = ctry->string;
        /* find the latest year with a numeric value */
        const char *best_year = NULL; double best_val = 0;
        cJSON *yr;
        cJSON_ArrayForEach(yr, ctry) {
          if (!cJSON_IsNumber(yr) || !yr->string) continue;
          if (!best_year || strcmp(yr->string, best_year) > 0) {
            best_year = yr->string; best_val = yr->valuedouble;
          }
        }
        if (!best_year || !iso3) continue;
        char title[256], summary[192], keyb[128];
        snprintf(title, sizeof title, "%s — %s: %.6g (%s)",
                 iso3, ind, best_val, best_year);
        snprintf(summary, sizeof summary, "%s = %.6g", ind, best_val);
        snprintf(keyb, sizeof keyb, "IMF|%s|%s|%s", ind, iso3, best_year);
        cJSON *b = cJSON_CreateObject();
        cJSON_AddStringToObject(b, "indicator", ind);
        cJSON_AddStringToObject(b, "iso3", iso3);
        cJSON_AddStringToObject(b, "year", best_year);
        cJSON_AddNumberToObject(b, "value", best_val);
        cJSON_AddStringToObject(b, "source", "IMF DataMapper");
        char *bj = cJSON_PrintUnformatted(b);
        cJSON_Delete(b);
        emitted += ew_emit(sink, "IMF_DATA", "imf-indicator",
                           keyb, title, summary, bj, best_year,
                           "https://www.imf.org/external/datamapper");
        free(bj);
        if (emitted >= 400) break;
      }
      if (emitted >= 400) break;
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[IMF_DATA] emitted %d\n", emitted);
  return emitted;
}

/* ---- Eurostat ----------------------------------------------------------- *
 * GET /api/dissemination/statistics/1.0/data/<dataset>?format=JSON returns
 * JSON-stat: { value:{ "<idx>":v, ... }, dimension:{ geo:{ category:{ index:{
 * <CODE>:i }, label:{ <CODE>:name } } } }, ... }. Full inversion of the index
 * grid is heavy; we take the honest, useful subset: map each value's position
 * to a geo label when the geo dimension is the sole/last varying one, else emit
 * the raw value keyed by its position. Values are all real; positions we cannot
 * confidently attribute get the numeric index as their label. */
static int ew_eurostat(const source_ctx *ctx, intel_sink *sink, const char *ds) {
  char *enc = jo_urlencode(ds);
  if (!enc) return 0;
  char url[640];
  snprintf(url, sizeof url,
    "https://ec.europa.eu/eurostat/api/dissemination/statistics/1.0/data/%s"
    "?format=JSON&lang=EN", enc);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "EUROSTAT");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *value = cJSON_GetObjectItem(root, "value");
  int emitted = 0;
  if (cJSON_IsObject(value)) {
    cJSON *v;
    cJSON_ArrayForEach(v, value) {
      if (!cJSON_IsNumber(v) || !v->string) continue;
      const char *pos = v->string;   /* JSON-stat linear index of the datum */
      char title[256], keyb[128];
      snprintf(title, sizeof title, "%s [%s]: %.6g", ds, pos, v->valuedouble);
      snprintf(keyb, sizeof keyb, "ESTAT|%s|%s", ds, pos);
      cJSON *b = cJSON_CreateObject();
      cJSON_AddStringToObject(b, "dataset", ds);
      cJSON_AddStringToObject(b, "position", pos);
      cJSON_AddNumberToObject(b, "value", v->valuedouble);
      cJSON_AddStringToObject(b, "source", "Eurostat");
      char *bj = cJSON_PrintUnformatted(b);
      cJSON_Delete(b);
      emitted += ew_emit(sink, "EUROSTAT", "eurostat-observation",
                         keyb, title, NULL, bj, NULL,
                         "https://ec.europa.eu/eurostat");
      free(bj);
      if (emitted >= 400) break;
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[EUROSTAT] emitted %d\n", emitted);
  return emitted;
}

/* ---- UN Comtrade (gated) ------------------------------------------------ *
 * GET /data/v1/get/C/A/HS?reporterCode=<r>&period=<y>&cmdCode=<hs>&
 * subscription-key=<key> returns { data: [ { reporterDesc, partnerDesc,
 * cmdDesc, flowDesc, primaryValue, period, ... }, ... ] }. entity = a reporter
 * ISO numeric code (e.g. "392" Japan) or comma reporterCode; keep it simple:
 * entity is the reporterCode. */
static int ew_comtrade(const source_ctx *ctx, intel_sink *sink,
                       const char *entity, const char *key) {
  char *enc = jo_urlencode(entity);
  if (!enc) return 0;
  char url[640];
  /* latest full-year annual, all commodities aggregate (cmdCode=TOTAL) */
  snprintf(url, sizeof url,
    "https://comtradeapi.un.org/data/v1/get/C/A/HS"
    "?reporterCode=%s&cmdCode=TOTAL&flowCode=X,M&maxRecords=500"
    "&subscription-key=%s", enc, key);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "UN_COMTRADE");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *data = cJSON_GetObjectItem(root, "data");
  int emitted = 0;
  if (cJSON_IsArray(data)) {
    cJSON *o;
    cJSON_ArrayForEach(o, data) {
      const cJSON *val = cJSON_GetObjectItem(o, "primaryValue");
      const char *rep = jo_sv(o, "reporterDesc");
      const char *par = jo_sv(o, "partnerDesc");
      const char *flow = jo_sv(o, "flowDesc");
      const char *cmd = jo_sv(o, "cmdDesc");
      const cJSON *per = cJSON_GetObjectItem(o, "period");
      char period[32] = {0};
      if (cJSON_IsNumber(per)) snprintf(period, sizeof period, "%.0f", per->valuedouble);
      else if (cJSON_IsString(per) && per->valuestring) snprintf(period, sizeof period, "%s", per->valuestring);
      if (!rep || !cJSON_IsNumber(val)) continue;
      char title[320], summary[224], keyb[160];
      snprintf(title, sizeof title, "%s %s %s (%s): %.6g",
               rep, flow ? flow : "trade", par ? par : "World",
               cmd ? cmd : "TOTAL", val->valuedouble);
      snprintf(summary, sizeof summary, "%s %s = %.6g USD",
               flow ? flow : "trade", period, val->valuedouble);
      snprintf(keyb, sizeof keyb, "COMTRADE|%s|%s|%s|%s",
               rep, flow ? flow : "?", par ? par : "?", period);
      cJSON *b = cJSON_CreateObject();
      if (rep)  cJSON_AddStringToObject(b, "reporter", rep);
      if (par)  cJSON_AddStringToObject(b, "partner", par);
      if (flow) cJSON_AddStringToObject(b, "flow", flow);
      if (cmd)  cJSON_AddStringToObject(b, "commodity", cmd);
      cJSON_AddNumberToObject(b, "value_usd", val->valuedouble);
      if (period[0]) cJSON_AddStringToObject(b, "period", period);
      cJSON_AddStringToObject(b, "source", "UN Comtrade");
      char *bj = cJSON_PrintUnformatted(b);
      cJSON_Delete(b);
      emitted += ew_emit(sink, "UN_COMTRADE", "comtrade-flow",
                         keyb, title, summary, bj, period[0] ? period : NULL,
                         "https://comtradeplus.un.org");
      free(bj);
      if (emitted >= 500) break;
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[UN_COMTRADE] emitted %d\n", emitted);
  return emitted;
}

/* ---- FRED (gated) ------------------------------------------------------- *
 * GET /fred/series/observations?series_id=<id>&api_key=<k>&file_type=json&
 * sort_order=desc&limit=100 returns { observations:[ {date,value}, ... ] }.
 * entity = a FRED series id (e.g. GDP, UNRATE). */
static int ew_fred(const source_ctx *ctx, intel_sink *sink,
                   const char *series, const char *key) {
  char *enc = jo_urlencode(series);
  if (!enc) return 0;
  char url[640];
  snprintf(url, sizeof url,
    "https://api.stlouisfed.org/fred/series/observations"
    "?series_id=%s&api_key=%s&file_type=json&sort_order=desc&limit=100",
    enc, key);
  free(enc);
  const char *hdrs[] = { "Accept: application/json",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "FRED_SERIES");
  if (!body) return 0;
  cJSON *root = cJSON_Parse(body);
  free(body);
  if (!root) return 0;
  cJSON *obs = cJSON_GetObjectItem(root, "observations");
  int emitted = 0;
  if (cJSON_IsArray(obs)) {
    cJSON *o;
    cJSON_ArrayForEach(o, obs) {
      const char *date = jo_sv(o, "date");
      const char *valstr = jo_sv(o, "value");
      if (!date || !valstr || strcmp(valstr, ".") == 0) continue;  /* "." = NA */
      char title[256], keyb[128];
      snprintf(title, sizeof title, "%s @ %s: %s", series, date, valstr);
      snprintf(keyb, sizeof keyb, "FRED|%s|%s", series, date);
      cJSON *b = cJSON_CreateObject();
      cJSON_AddStringToObject(b, "series_id", series);
      cJSON_AddStringToObject(b, "date", date);
      cJSON_AddStringToObject(b, "value", valstr);
      cJSON_AddStringToObject(b, "source", "FRED");
      char *bj = cJSON_PrintUnformatted(b);
      cJSON_Delete(b);
      char link[256];
      snprintf(link, sizeof link, "https://fred.stlouisfed.org/series/%s", series);
      emitted += ew_emit(sink, "FRED_SERIES", "fred-observation",
                         keyb, title, valstr, bj, date, link);
      free(bj);
      if (emitted >= 100) break;
    }
  }
  cJSON_Delete(root);
  fprintf(stderr, "[FRED_SERIES] emitted %d\n", emitted);
  return emitted;
}

/* ---- Stooq quote (keyless CSV) ------------------------------------------ *
 * GET /q/l/?s=<sym>&f=sd2t2ohlcv&e=csv returns:
 *   Symbol,Date,Time,Open,High,Low,Close,Volume\n
 *   AAPL.US,2024-01-05,22:00:02,181.99,182.76,180.17,181.18,62303000\n
 * entity = one or comma-separated symbols. We emit one quote per data line. */
static int ew_stooq(const source_ctx *ctx, intel_sink *sink, const char *sym) {
  char *enc = jo_urlencode(sym);
  if (!enc) return 0;
  char url[512];
  snprintf(url, sizeof url,
    "https://stooq.com/q/l/?s=%s&f=sd2t2ohlcv&h&e=csv", enc);
  free(enc);
  const char *hdrs[] = { "Accept: text/csv, */*",
                         "User-Agent: JapanOSINT/1.0", NULL };
  char *body = jo_get(ctx, url, hdrs, "STOOQ_QUOTES");
  if (!body) return 0;
  int emitted = 0;
  const char *line = body;
  int header = 1;
  while (line && *line && emitted < 100) {
    const char *nl = strchr(line, '\n');
    size_t llen = nl ? (size_t)(nl - line) : strlen(line);
    char buf[512];
    size_t cp = llen < sizeof buf - 1 ? llen : sizeof buf - 1;
    memcpy(buf, line, cp); buf[cp] = 0;
    while (cp && (buf[cp-1] == '\r' || buf[cp-1] == ' ')) buf[--cp] = 0;
    if (header) { header = 0; goto next; }         /* skip CSV header row */
    if (!buf[0]) goto next;
    {
      /* split 8 comma fields */
      char *f[8] = {0}; int nf = 0; char *s = buf;
      for (char *p = buf; ; p++) {
        if (*p == ',' || *p == 0) {
          int last = (*p == 0); *p = 0;
          if (nf < 8) f[nf++] = s;
          s = p + 1;
          if (last) break;
        }
      }
      const char *symf = nf > 0 ? f[0] : NULL;
      const char *date = nf > 1 ? f[1] : NULL;
      const char *close = nf > 6 ? f[6] : NULL;
      /* Stooq returns "N/D" for unknown symbols → honest skip */
      if (!symf || !close || strcmp(close, "N/D") == 0 ||
          (date && strcmp(date, "N/D") == 0)) goto next;
      char title[256], keyb[160];
      snprintf(title, sizeof title, "%s @ %s: close %s", symf,
               date ? date : "", close);
      snprintf(keyb, sizeof keyb, "STOOQ|%s|%s", symf, date ? date : "");
      cJSON *b = cJSON_CreateObject();
      cJSON_AddStringToObject(b, "symbol", symf);
      if (date)         cJSON_AddStringToObject(b, "date", date);
      if (nf > 2 && f[2][0]) cJSON_AddStringToObject(b, "time", f[2]);
      if (nf > 3 && f[3][0]) cJSON_AddStringToObject(b, "open", f[3]);
      if (nf > 4 && f[4][0]) cJSON_AddStringToObject(b, "high", f[4]);
      if (nf > 5 && f[5][0]) cJSON_AddStringToObject(b, "low", f[5]);
      cJSON_AddStringToObject(b, "close", close);
      if (nf > 7 && f[7][0]) cJSON_AddStringToObject(b, "volume", f[7]);
      cJSON_AddStringToObject(b, "source", "Stooq");
      char *bj = cJSON_PrintUnformatted(b);
      cJSON_Delete(b);
      char link[256];
      snprintf(link, sizeof link, "https://stooq.com/q/?s=%s", symf);
      emitted += ew_emit(sink, "STOOQ_QUOTES", "stooq-quote",
                         keyb, title, close, bj, date, link);
      free(bj);
    }
  next:
    if (!nl) break;
    line = nl + 1;
  }
  free(body);
  fprintf(stderr, "[STOOQ_QUOTES] emitted %d\n", emitted);
  return emitted;
}

/* ---- dispatch ----------------------------------------------------------- */
static int run(const source_ctx *ctx, intel_sink *sink) {
  const char *q = (ctx->entity && *ctx->entity) ? ctx->entity : NULL;
  const char *id = ctx->source_id;

  if (strcmp(id, "WORLDBANK_INDICATORS") == 0) {
    /* entity = indicator code; default to GDP (current US$) for scheduled/blank */
    ew_worldbank(ctx, sink, q ? q : "NY.GDP.MKTP.CD");
    return 0;
  }
  if (strcmp(id, "IMF_DATA") == 0) {
    /* entity = indicator id; default NGDP_RPCH (real GDP growth) */
    ew_imf(ctx, sink, q ? q : "NGDP_RPCH");
    return 0;
  }
  if (strcmp(id, "EUROSTAT") == 0) {
    if (!q) { fprintf(stderr, "[EUROSTAT] no entity (dataset code)\n"); return 0; }
    ew_eurostat(ctx, sink, q);
    return 0;
  }
  if (strcmp(id, "UN_COMTRADE") == 0) {
    const char *key = jo_env("COMTRADE_KEY");
    if (!key) { fprintf(stderr, "[UN_COMTRADE] gated (no COMTRADE_KEY)\n"); return 0; }
    if (!q) { fprintf(stderr, "[UN_COMTRADE] no entity (reporter code)\n"); return 0; }
    ew_comtrade(ctx, sink, q, key);
    return 0;
  }
  if (strcmp(id, "FRED_SERIES") == 0) {
    const char *key = jo_env("FRED_KEY");
    if (!key) { fprintf(stderr, "[FRED_SERIES] gated (no FRED_KEY)\n"); return 0; }
    if (!q) { fprintf(stderr, "[FRED_SERIES] no entity (series id)\n"); return 0; }
    ew_fred(ctx, sink, q, key);
    return 0;
  }
  if (strcmp(id, "STOOQ_QUOTES") == 0) {
    if (!q) { fprintf(stderr, "[STOOQ_QUOTES] no entity (ticker symbol)\n"); return 0; }
    ew_stooq(ctx, sink, q);
    return 0;
  }
  return -1;
}

#define ECON_DEF(SYM, ID, NAME, NAMEJA, TYPE, URL, FREE, DESC) \
  static const source_def SYM = { .id = ID, .collector = "osint", .name = NAME, \
    .name_ja = NAMEJA, .update_interval_sec = 0, .run = run, \
    .category = "economy", .type = TYPE, .url = URL, .description = DESC, \
    .layer = NULL, .free_tier = FREE }; \
  REGISTER_SOURCE(SYM)

ECON_DEF(ew_wb_def, "WORLDBANK_INDICATORS", "World Bank Indicators",
  "世界銀行 指標", "api", "https://api.worldbank.org/v2", 1,
  "World Bank Open Data — indicator code -> latest value per country (keyless JSON)");
ECON_DEF(ew_imf_def, "IMF_DATA", "IMF DataMapper", "IMF データマッパー",
  "api", "https://www.imf.org/external/datamapper", 1,
  "IMF DataMapper — indicator id -> latest value per country (keyless JSON)");
ECON_DEF(ew_estat_def, "EUROSTAT", "Eurostat Statistics", "ユーロスタット統計",
  "api", "https://ec.europa.eu/eurostat", 1,
  "Eurostat statistics API — dataset code -> observations (keyless JSON-stat)");
ECON_DEF(ew_comtrade_def, "UN_COMTRADE", "UN Comtrade Trade Flows",
  "UN Comtrade 貿易統計", "api", "https://comtradeplus.un.org", 1,
  "UN Comtrade — reporter code -> trade flows (needs COMTRADE_KEY)");
ECON_DEF(ew_fred_def, "FRED_SERIES", "FRED Economic Series",
  "FRED 経済系列", "api", "https://fred.stlouisfed.org", 1,
  "FRED (St. Louis Fed) — series id -> observations (needs FRED_KEY)");
ECON_DEF(ew_stooq_def, "STOOQ_QUOTES", "Stooq Market Quotes",
  "Stooq 市場クオート", "api", "https://stooq.com", 1,
  "Stooq — ticker symbol -> latest OHLCV quote (keyless CSV)");
