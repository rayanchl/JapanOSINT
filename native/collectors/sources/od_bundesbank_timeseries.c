/* Deutsche Bundesbank statistics REST (SDMX-JSON).
 * Endpoint: https://api.statistiken.bundesbank.de/rest/data/
 *           BBEX3/D.USD.EUR.BB.AC.000?lastNObservations=10&format=json
 * Same SDMX-JSON positional-index trap as the ECB endpoint: observation keys
 * index structure.dimensions.observation[0].values[] and the series key is
 * positional into structure.dimensions.series[]; both are joined so the
 * emitted period is a real date. Observation values come back as STRINGS
 * here and are parsed as numbers; a null observation is skipped, not zeroed.
 * Labels are German (contentLanguages ["de"]). Keyless.
 * (Destatis GENESIS was not used: it requires a registered account.)
 * Licence: the Bundesbank permits reuse of published statistics with
 * attribution. */
#include "od_shared.c"

#define SID "bundesbank-timeseries"
static const char *URL =
  "https://api.statistiken.bundesbank.de/rest/data/BBEX3/D.USD.EUR.BB.AC.000"
  "?lastNObservations=10&format=json";

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_sdmx_collect(ctx, sink, URL, "fx-reference-rate",
                                    "[\"statistics\",\"fx\",\"de\"]", URL,
                                    500));
}

static const source_def od_bundesbank_timeseries_def = {
  .id = SID, .collector = "statistics",
  .name = "Deutsche Bundesbank statistics REST (SDMX-JSON)",
  .update_interval_sec = 21600, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://api.statistiken.bundesbank.de/rest/data/BBEX3/D.USD.EUR.BB.AC.000?lastNObservations=10&format=json",
  .description = "Bundesbank time-series observations (USD/EUR daily rate from BBEX3) as dated numeric values from the keyless SDMX-JSON API",
  .license = "Bundesbank - reuse of published statistics with attribution",
  .free_tier = 1,
};
REGISTER_SOURCE(od_bundesbank_timeseries_def)
