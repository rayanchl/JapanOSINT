/* Norwegian Offshore Directorate (Sodir) monthly field production.
 * Endpoint (keyless CSV export of the FactPages table):
 *   https://factpages.sodir.no/public?/Factpages/external/tableview/
 *   field_production_monthly&rs:Command=Render&rc:Toolbar=false
 *   &rc:Parameters=f&IpAddress=1&CultureCode=en&rs:Format=CSV&Top100=false
 * ~28k rows, comma-separated with a header row and a UTF-8 BOM on the first
 * header cell (stripped before parsing).
 *
 * UNIT TRAP, quoted from the source survey: "UNITS ARE IN THE COLUMN NAMES AND
 * ARE NOT THE SAME: oil/NGL/condensate/oil-equivalent are MILLION Sm3, gas is
 * BILLION Sm3 - multiplying the wrong one by 1e6 is a 1000x error." Each figure
 * is therefore emitted under a field name that carries its own unit
 * (oil_mill_sm3, gas_bill_sm3, ...) plus an explicit units object; no
 * normalisation or summation across units is attempted.
 * DATE: prfYear and prfMonth are SEPARATE integer columns (month 1-12); there
 * is no date column. Rows are ordered by field then time, so the collector
 * keeps only the LATEST (year, month) seen per field.
 * CSV parsing uses lib/csv.h (RFC4180, header-keyed) — field names contain
 * slashes and spaces, so positional indexing is avoided entirely.
 * GEO: this table carries no coordinates; none are invented (R2).
 * Licence: Sodir FactPages open data (NLOD-equivalent) with attribution. */
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../lib/csv.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "sodir-field-production"
#define MAXF 256

static const struct { const char *col, *out, *unit; } COLS[] = {
  { "prfPrdOilNetMillSm3",        "oil_mill_sm3",         "million Sm3" },
  { "prfPrdGasNetBillSm3",        "gas_bill_sm3",         "billion Sm3" },
  { "prfPrdNGLNetMillSm3",        "ngl_mill_sm3",         "million Sm3" },
  { "prfPrdCondensateNetMillSm3", "condensate_mill_sm3",  "million Sm3" },
  { "prfPrdOeNetMillSm3",         "oil_equivalent_mill_sm3", "million Sm3" },
  { "prfPrdProducedWaterInFieldMillSm3", "produced_water_mill_sm3", "million Sm3" },
  { NULL, NULL, NULL }
};

typedef struct { const char *field; int year, month; cJSON *row; } latest_t;

static const char *cell(const cJSON *r, const char *k) {
  const cJSON *v = cJSON_GetObjectItem(r, k);
  return (v && cJSON_IsString(v) && v->valuestring[0]) ? v->valuestring : NULL;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  char *csv = feed_get_text(ctx->http,
    "https://factpages.sodir.no/public?/Factpages/external/tableview/"
    "field_production_monthly&rs:Command=Render&rc:Toolbar=false"
    "&rc:Parameters=f&IpAddress=1&CultureCode=en&rs:Format=CSV&Top100=false",
    120000);
  if (!csv) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  /* strip the UTF-8 BOM so the first header cell matches "prfInformationCarrier" */
  const char *body = csv;
  if ((unsigned char)body[0] == 0xEF && (unsigned char)body[1] == 0xBB &&
      (unsigned char)body[2] == 0xBF) body += 3;

  cJSON *rows = csv_parse(body, 1);       /* header-keyed, RFC4180 */
  free(csv);
  int nrows = cJSON_GetArraySize(rows);
  if (nrows == 0) { cJSON_Delete(rows); fprintf(stderr, "[" SRC "] empty CSV\n"); return -1; }

  /* keep the newest (year, month) per field */
  latest_t best[MAXF]; int nbest = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    const char *fld = cell(r, "prfInformationCarrier");
    const char *ys = cell(r, "prfYear"), *ms = cell(r, "prfMonth");
    if (!fld || !ys || !ms) continue;
    int y = atoi(ys), mo = atoi(ms);
    if (y < 1900 || mo < 1 || mo > 12) continue;
    int idx = -1;
    for (int i = 0; i < nbest; i++)
      if (strcmp(best[i].field, fld) == 0) { idx = i; break; }
    if (idx < 0) {
      if (nbest >= MAXF) continue;
      idx = nbest++;
      best[idx].field = fld; best[idx].year = 0; best[idx].month = 0;
      best[idx].row = NULL;
    }
    if (y > best[idx].year || (y == best[idx].year && mo > best[idx].month)) {
      best[idx].year = y; best[idx].month = mo; best[idx].row = r;
    }
  }

  int n = 0;
  for (int i = 0; i < nbest; i++) {
    if (!best[i].row) continue;
    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "field_name", best[i].field);
    cJSON_AddNumberToObject(p, "year", best[i].year);
    cJSON_AddNumberToObject(p, "month", best[i].month);
    cJSON *units = cJSON_CreateObject();
    int have = 0;
    double oil = 0; int have_oil = 0;
    for (int c = 0; COLS[c].col; c++) {
      const char *s = cell(best[i].row, COLS[c].col);
      if (!s) continue;
      char *e = NULL;
      double v = strtod(s, &e);
      if (e == s) continue;
      cJSON_AddNumberToObject(p, COLS[c].out, v);
      cJSON_AddStringToObject(units, COLS[c].out, COLS[c].unit);
      if (c == 0) { oil = v; have_oil = 1; }
      have = 1;
    }
    if (!have) { cJSON_Delete(units); cJSON_Delete(p); continue; }
    cJSON_AddItemToObject(p, "units", units);
    cJSON_AddStringToObject(p, "unit_warning",
      "oil/NGL/condensate/oil-equivalent are MILLION Sm3; gas is BILLION Sm3");
    const char *npdid = cell(best[i].row, "prfNpdidInformationCarrier");
    if (npdid) cJSON_AddStringToObject(p, "npdid", npdid);
    cJSON_AddStringToObject(p, "country", "NO");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[192], title[288], pub[16];
    snprintf(key, sizeof key, "%s|%04d-%02d", best[i].field, best[i].year,
             best[i].month);
    snprintf(pub, sizeof pub, "%04d-%02d-01", best[i].year, best[i].month);
    if (have_oil)
      snprintf(title, sizeof title, "%s %04d-%02d: oil %.5f million Sm3",
               best[i].field, best[i].year, best[i].month, oil);
    else
      snprintf(title, sizeof title, "%s %04d-%02d production",
               best[i].field, best[i].year, best[i].month);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.published_at    = pub;
    row.lang            = "en";
    row.link            = "https://factpages.sodir.no/";
    row.record_type     = "oil-gas-field-production";
    row.properties_json = pj;
    row.tags_json       = "[\"industry\",\"oil-gas\",\"norway\",\"production\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(rows);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_sodir_field_production_def = {
  .id = SRC, .collector = "industry",
  .name = "Norwegian Offshore Directorate monthly field production",
  .update_interval_sec = 604800, .run = run,
  .category = "industry", .type = "dataset",
  .url = "https://factpages.sodir.no/public?/Factpages/external/tableview/field_production_monthly&rs:Format=CSV",
  .description = "Official monthly oil, gas, NGL, condensate and produced-water volumes for every producing field on the Norwegian continental shelf, each figure carrying its own unit.",
  .license = "Sodir (Norwegian Offshore Directorate) FactPages open data (NLOD-equivalent), attribution required.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_sodir_field_production_def)
