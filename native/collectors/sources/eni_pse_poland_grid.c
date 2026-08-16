/* PSE Poland transmission system operational data (15-minute national balance).
 * Endpoint: https://api.raporty.pse.pl/api/his-wlk-cal
 *           ?%24filter=business_date%20eq%20'<yyyy-mm-dd>'             (keyless)
 *
 * ENCODING TRAP: the OData-style query params must be PERCENT-ENCODED
 * ($filter -> %24filter) or the server IGNORES them and returns the OLDEST
 * rows (its default page starts 2024-06-14) — a silent staleness trap that
 * would look like fresh data. The URL below is built pre-encoded.
 * Envelope is {"value":[...]}. ALL the emitted values are MW:
 *   demand = national demand, jg = scheduled units, wi = wind,
 *   pv = photovoltaic, swm_p / swm_np = synchronous / non-synchronous
 *   cross-border exchange (NEGATIVE = export).
 * dtime is Polish local time and dtime_utc is the UTC twin — dtime_utc is
 * preferred for the row timestamp.
 * Licence: PSE raporty API, public, keyless. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SRC "pse-poland-grid"

static const struct { const char *key, *out; } MW[] = {
  { "demand", "demand_mw"                 },
  { "jg",     "scheduled_generation_mw"   },
  { "wi",     "wind_mw"                   },
  { "pv",     "photovoltaic_mw"           },
  { "swm_p",  "exchange_synchronous_mw"   },
  { "swm_np", "exchange_non_synchronous_mw" },
  { NULL, NULL }
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL) - 24 * 3600;    /* yesterday: the day is complete */
  struct tm tmv; gmtime_r(&now, &tmv);
  char day[16];
  strftime(day, sizeof day, "%Y-%m-%d", &tmv);

  /* %24filter, %20 and %27 must stay percent-encoded (see header note) */
  char url[288];
  snprintf(url, sizeof url,
    "https://api.raporty.pse.pl/api/his-wlk-cal"
    "?%%24filter=business_date%%20eq%%20%%27%s%%27", day);

  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }
  cJSON *arr = cJSON_GetObjectItem(doc, "value");
  if (!cJSON_IsArray(arr)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no value[]\n"); return -1; }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, arr) {
    const char *utc = jo_sv(r, "dtime_utc");
    const char *loc = jo_sv(r, "dtime");
    const char *bd  = jo_sv(r, "business_date");
    /* a row whose business_date is not the one requested means the encoded
     * filter was dropped by the server — refuse it rather than emit 2024 data */
    if (bd && strncmp(bd, day, 10) != 0) continue;

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "country", "PL");
    if (utc) cJSON_AddStringToObject(p, "dtime_utc", utc);
    if (loc) cJSON_AddStringToObject(p, "dtime_local", loc);
    if (bd)  cJSON_AddStringToObject(p, "business_date", bd);
    int have = 0;
    double dem = 0; int have_dem = 0;
    for (int i = 0; MW[i].key; i++) {
      cJSON *v = cJSON_GetObjectItem(r, MW[i].key);
      if (!cJSON_IsNumber(v)) continue;
      cJSON_AddNumberToObject(p, MW[i].out, v->valuedouble);
      if (i == 0) { dem = v->valuedouble; have_dem = 1; }
      have = 1;
    }
    if (!have) { cJSON_Delete(p); continue; }
    cJSON_AddStringToObject(p, "unit", "MW");
    cJSON_AddStringToObject(p, "exchange_sign_convention",
                            "negative cross-border exchange = export");
    cJSON_AddStringToObject(p, "resolution", "15min");
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[96], title[224];
    snprintf(key, sizeof key, "%s", utc ? utc : (loc ? loc : ""));
    if (!key[0]) { free(pj); continue; }
    if (have_dem)
      snprintf(title, sizeof title, "Poland demand %.2f MW at %s",
               dem, utc ? utc : loc);
    else
      snprintf(title, sizeof title, "Poland grid balance at %s", utc ? utc : loc);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.published_at    = utc;
    row.lang            = "en";
    row.link            = "https://raporty.pse.pl/";
    row.record_type     = "grid-balance";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"poland\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_pse_poland_grid_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "PSE Poland transmission system operational data",
  .update_interval_sec = 900, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://api.raporty.pse.pl/api/his-wlk-cal",
  .description = "Polish TSO 15-minute national balance in MW: demand, scheduled generation, wind, PV and cross-border exchange.",
  .license = "PSE raporty API, public, keyless.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_pse_poland_grid_def)
