/* Energinet (DK TSO) 5-minute realtime production and cross-border exchange.
 * Endpoint: https://api.energidataservice.dk/dataset/ElectricityProdex5MinRealtime
 *           ?limit=200&sort=Minutes5UTC%20DESC                       (keyless)
 * Emits, for the newest 5-minute stamp of EACH price area (DK1, DK2):
 *   OffshoreWindPower / OnshoreWindPower / SolarPower / ProductionGe100MW /
 *   ProductionLt100MW / ExchangeGermany / ExchangeNorway / ExchangeSweden /
 *   ExchangeGreatBelt — all in MW.
 * SIGN NOTE: negative exchange = net EXPORT out of Denmark.
 * KEYING TRAP: two rows share each timestamp (DK1 and DK2). Keying only by
 * time silently overwrites one area, so rows are keyed by PriceArea+time.
 * sort=Minutes5UTC DESC is mandatory: the default order starts in 2017.
 * Licence: Energi Data Service open data, attribution to Energinet. */
#include "lib/jocore.h"
#include "source.h"
#include "lib/feedlib.h"
#include "third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SRC "energinet-realtime-production"

static const char *MW_FIELDS[] = {
  "OffshoreWindPower", "OnshoreWindPower", "SolarPower",
  "ProductionGe100MW", "ProductionLt100MW",
  "ExchangeGermany", "ExchangeNorway", "ExchangeSweden", "ExchangeGreatBelt",
  "ExchangeContinent", "ExchangeNordicCountries", NULL
};

static int run(const source_ctx *ctx, intel_sink *sink) {
  cJSON *doc = feed_get_json(ctx->http,
    "https://api.energidataservice.dk/dataset/ElectricityProdex5MinRealtime"
    "?limit=200&sort=Minutes5UTC%20DESC", 25000);
  if (!doc) { fprintf(stderr, "[" SRC "] fetch failed\n"); return -1; }

  cJSON *recs = cJSON_GetObjectItem(doc, "records");
  if (!cJSON_IsArray(recs)) { cJSON_Delete(doc); fprintf(stderr, "[" SRC "] no records[]\n"); return -1; }

  /* keep the first (=newest, thanks to sort DESC) row seen per price area */
  char seen[8][16]; int nseen = 0;
  int n = 0;
  cJSON *r;
  cJSON_ArrayForEach(r, recs) {
    const char *area = jo_sv(r, "PriceArea");
    const char *tstamp = jo_sv(r, "Minutes5UTC");
    if (!area || !tstamp) continue;
    int dup = 0;
    for (int i = 0; i < nseen; i++) if (strcmp(seen[i], area) == 0) { dup = 1; break; }
    if (dup) continue;
    if (nseen < 8) { snprintf(seen[nseen], sizeof seen[0], "%s", area); nseen++; }

    cJSON *p = cJSON_CreateObject();
    cJSON_AddStringToObject(p, "price_area", area);
    cJSON_AddStringToObject(p, "timestamp_utc", tstamp);
    cJSON_AddStringToObject(p, "unit", "MW");
    cJSON_AddStringToObject(p, "sign_convention",
                            "negative exchange = net export from Denmark");
    int fields = 0;
    double wind = 0; int have_wind = 0;
    for (int i = 0; MW_FIELDS[i]; i++) {
      cJSON *v = cJSON_GetObjectItem(r, MW_FIELDS[i]);
      if (!cJSON_IsNumber(v)) continue;          /* absent/null -> omit (R1) */
      cJSON_AddNumberToObject(p, MW_FIELDS[i], v->valuedouble);
      fields++;
      if (i < 2) { wind += v->valuedouble; have_wind = 1; }
    }
    if (!fields) { cJSON_Delete(p); continue; }  /* no measurement -> no row */
    char *pj = cJSON_PrintUnformatted(p);
    cJSON_Delete(p);

    char key[96], title[200];
    snprintf(key, sizeof key, "%s|%s", area, tstamp);
    if (have_wind)
      snprintf(title, sizeof title, "Energinet %s %s: wind %.1f MW", area, tstamp, wind);
    else
      snprintf(title, sizeof title, "Energinet %s %s realtime production", area, tstamp);

    intel_item row = {0};
    row.remote_key      = key;
    row.title           = title;
    row.summary         = title;
    row.published_at    = tstamp;
    row.lang            = "en";
    row.link            = "https://api.energidataservice.dk/dataset/ElectricityProdex5MinRealtime";
    row.record_type     = "grid-production";
    row.properties_json = pj;
    row.tags_json       = "[\"energy\",\"grid\",\"denmark\",\"production\"]";
    if (sink->emit(sink, &row) >= 0) n++;
    free(pj);
  }
  cJSON_Delete(doc);
  fprintf(stderr, "[" SRC "] emitted %d\n", n);
  return 0;
}

static const source_def eni_energinet_prodex_def = {
  .id = SRC, .collector = "infrastructure",
  .name = "Energinet 5-minute realtime production and exchange (DK)",
  .update_interval_sec = 300, .run = run,
  .category = "infrastructure", .type = "api",
  .url = "https://api.energidataservice.dk/dataset/ElectricityProdex5MinRealtime",
  .description = "Danish TSO realtime production by category and cross-border exchange per price area (DK1/DK2), 5-minute resolution, in MW.",
  .license = "Energi Data Service open data, no key; attribution to Energinet requested.",
  .free_tier = 1,
};
REGISTER_SOURCE(eni_energinet_prodex_def)
