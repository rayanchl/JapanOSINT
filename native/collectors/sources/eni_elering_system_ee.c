/* Elering (Estonian TSO) live power system state.
 * Endpoint: https://dashboard.elering.ee/api/system/with-plan
 *           ?start=<ISO ms Z>&end=<ISO ms Z>                          (keyless)
 * start/end are MANDATORY ISO-8601 with milliseconds and Z; a window longer
 * than 7 days is rejected, so a 3-hour window is requested.
 * Emits one row per timestamp in data.real[]: production / consumption /
 * system_balance / ac_balance / production_renewable (UNIT: MW) and frequency
 * (UNIT: Hz — one of the very few keyless sources publishing measured system
 * frequency).
 *
 * FORECAST TRAP: the envelope is {success, data:{real:[...], plan:[...]}} and
 * ONLY data.real holds measurements — data.plan is the SCHEDULE and is never
 * read, so a plan value can never be presented as an observation.
 * NULL NOTE: losses and solar_energy_production are frequently null; nulls are
 * omitted from the row rather than coerced to 0.
 * timestamp is epoch SECONDS.
 * Licence: Elering live dashboard API, public, no key. */
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SRC "elering-system-ee"

static const struct { const char *key, *out, *unit; } MW_FIELDS[] = {
  { "production",           "production_mw",           "MW" },
  { "consumption",          "consumption_mw",          "MW" },
  { "system_balance",       "system_balance_mw",       "MW" },
  { "ac_balance",           "ac_balance_mw",           "MW" },
  { "production_renewable", "production_renewable_mw", "MW" },
  { "solar_energy_production", "solar_production_mw",  "MW" },
  { "losses",               "losses_mw",               "MW" },
  { NULL, NULL, NULL }
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  time_t now = time(NULL);
  time_t then = now - 3 * 3600;
  struct tm a, b;
  gmtime_r(&then, &a); gmtime_r(&now, &b);
  char t0[40], t1[40];
  strftime(t0, sizeof t0, "%Y-%m-%dT%H:%M:%S.000Z", &a);
  strftime(t1, sizeof t1, "%Y-%m-%dT%H:%M:%S.000Z", &b);

  char url[320];
  snprintf(url, sizeof url,
    "https://dashboard.elering.ee/api/system/with-plan?start=%s&end=%s", t0, t1);

  cJSON *doc = feed_get_json(ctx->http, url, 30000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  cJSON *data = cJSON_GetObjectItem(doc, "data");
  /* ONLY data.real — data.plan is the schedule, not an observation */
  cJSON *real = data ? cJSON_GetObjectItem(data, "real") : NULL;
  if (!cJSON_IsArray(real)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no data.real[]\n"); return -1; }

  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, real) {
    cJSON *ts = cJSON_GetObjectItem(r, "timestamp");
    if (!cJSON_IsNumber(ts)) continue;
    long long epoch = (long long)ts->valuedouble;   /* epoch SECONDS */

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "country", "EE");
    cJSON_AddNumberToObject(p, "timestamp_epoch_s", (double)epoch);
    cJSON_AddStringToObject(p, "measurement", "real (not plan)");
    int have = 0;
    for (int i = 0; MW_FIELDS[i].key; i++) {
      cJSON *v = cJSON_GetObjectItem(r, MW_FIELDS[i].key);
      if (!cJSON_IsNumber(v)) continue;           /* null -> omit, not 0 */
      cJSON_AddNumberToObject(p, MW_FIELDS[i].out, v->valuedouble);
      have = 1;
    }
    if (have) cJSON_AddStringToObject(p, "power_unit", "MW");
    cJSON *fq = cJSON_GetObjectItem(r, "frequency");
    double freq = 0; int have_freq = 0;
    if (cJSON_IsNumber(fq)) {
      freq = fq->valuedouble; have_freq = 1; have = 1;
      cJSON_AddNumberToObject(p, "frequency_hz", freq);
      cJSON_AddStringToObject(p, "frequency_unit", "Hz");
    }
    if (!have) { cJSON_Delete(p); continue; }
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[64], title[224];
    snprintf(key, sizeof key, "%lld", epoch);
    cJSON *pr = cJSON_GetObjectItem(r, "production");
    if (have_freq && cJSON_IsNumber(pr))
      snprintf(title, sizeof title,
               "Estonia grid: production %.2f MW, frequency %.3f Hz",
               pr->valuedouble, freq);
    else
      snprintf(title, sizeof title, "Estonia grid state at epoch %lld", epoch);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.lang            = "en";
    row.link            = "https://dashboard.elering.ee/";
    row.record_type     = "grid-state";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"estonia\",\"frequency\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_elering_system_ee_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "Elering Estonian power system live state",
  .update_interval_sec = 900, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://dashboard.elering.ee/api/system/with-plan",
  .description = "Estonian TSO live production, consumption, renewable share, cross-border balance (MW) and measured grid frequency (Hz).",
  .license = "Elering live dashboard API, public, no key.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_elering_system_ee_def)
