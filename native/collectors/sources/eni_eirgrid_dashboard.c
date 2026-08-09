/* EirGrid / SONI smart grid dashboard (Ireland + Northern Ireland).
 * Endpoint: https://www.smartgriddashboard.com/DashboardService.svc/data
 *           ?area=<area>&region=<region>&datefrom=<dd-MMM-yyyy HH:mm>
 *           &dateto=<dd-MMM-yyyy HH:mm>                               (keyless)
 * The newer /api/chart path returns HTTP 400 for every parameter combination,
 * so /DashboardService.svc/data is used.
 *
 * DATE-FORMAT TRAP: datefrom/dateto are MANDATORY and must be exactly
 * 'dd-MMM-yyyy HH:mm' (space URL-encoded as +). A wrong format yields an empty
 * Rows[] rather than an error, which would silently look like "no data" — the
 * format is built with strftime %d-%b-%Y %H:%M below and the space encoded.
 * UNITS: Value is MW for demandactual / windactual / interconnection, and
 * gCO2/kWh for area=co2intensity. The unit is chosen per area, never assumed.
 * NULL NOTE: Value is null on not-yet-measured steps — those rows are skipped.
 * Times are Irish local time with no offset marker, recorded as such.
 * Licence: EirGrid/SONI public dashboard service, keyless. */
#include "../../lib/jocore.h"
#include "../../source.h"
#include "../../lib/feedlib.h"
#include "../../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SRC "eirgrid-dashboard"

static const struct { const char *area, *unit, *label; } AREAS[] = {
  { "demandactual",   "MW",       "system demand"      },
  { "windactual",     "MW",       "wind generation"    },
  { "co2intensity",   "gCO2/kWh", "CO2 intensity"      },
  { "interconnection","MW",       "interconnector flow"},
  { NULL, NULL, NULL }
};
static const char *REGIONS[] = { "ROI", "NI", NULL };

static int collect(const source_ctx *ctx, intel_sink *sink, const char *area,
                   const char *unit, const char *label, const char *region,
                   const char *from, const char *to, int *fetched) {
  char url[448];
  snprintf(url, sizeof url,
    "https://www.smartgriddashboard.com/DashboardService.svc/data"
    "?area=%s&region=%s&datefrom=%s&dateto=%s", area, region, from, to);
  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) return 0;
  *fetched = 1;

  cJSON *rows = cJSON_GetObjectItem(doc, "Rows");
  if (!cJSON_IsArray(rows)) { cJSON_Delete(doc); return 0; }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, rows) {
    cJSON *val = cJSON_GetObjectItem(r, "Value");
    if (!cJSON_IsNumber(val)) continue;         /* null = not yet measured */
    const char *when = jo_sv(r, "EffectiveTime");
    const char *field = jo_sv(r, "FieldName");
    const char *reg = jo_sv(r, "Region");
    if (!when) continue;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "area", area);
    cJSON_AddStringToObject(p, "series", label);
    cJSON_AddStringToObject(p, "region", reg ? reg : region);
    if (field) cJSON_AddStringToObject(p, "field_name", field);
    cJSON_AddNumberToObject(p, "value", val->valuedouble);
    cJSON_AddStringToObject(p, "unit", unit);
    cJSON_AddStringToObject(p, "effective_time_local", when);
    cJSON_AddStringToObject(p, "timezone", "Europe/Dublin, no offset in feed");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[192], title[288];
    snprintf(key, sizeof key, "%s|%s|%s", area, reg ? reg : region, when);
    snprintf(title, sizeof title, "%s %s %s = %.1f %s",
             reg ? reg : region, label, when, val->valuedouble, unit);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = "https://www.smartgriddashboard.com/";
    row.record_type     = "grid-dashboard";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"ireland\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  return n;
}

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL);
  struct tm tmv; gmtime_r(&now, &tmv);
  char day[16];
  /* exactly dd-MMM-yyyy; the C locale gives the English %b the service wants */
  strftime(day, sizeof day, "%d-%b-%Y", &tmv);
  char from[40], to[40];
  snprintf(from, sizeof from, "%s+00%%3A00", day);
  snprintf(to,   sizeof to,   "%s+23%%3A59", day);

  int total = 0, fetched = 0;
  for (int a = 0; AREAS[a].area; a++)
    for (int r = 0; REGIONS[r]; r++)
      total += collect(ctx, sink, AREAS[a].area, AREAS[a].unit, AREAS[a].label,
                       REGIONS[r], from, to, &fetched);
  if (!fetched) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  fprintf(stderr, "[" SRC "] emitted %d\n", total);
  return 0;
}

static const source_def eni_eirgrid_dashboard_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "EirGrid / SONI smart grid dashboard (Ireland + NI)",
  .update_interval_sec = 900, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://www.smartgriddashboard.com/DashboardService.svc/data",
  .description = "All-island Irish grid: actual demand and wind generation (MW), interconnector flows and system CO2 intensity (gCO2/kWh) at 15-minute resolution.",
  .license = "EirGrid/SONI public dashboard service, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_eirgrid_dashboard_def)
