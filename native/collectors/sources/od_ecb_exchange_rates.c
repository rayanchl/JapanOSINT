/* ECB Data Portal SDMX-JSON - euro reference exchange rates.
 * Endpoint: https://data-api.ecb.europa.eu/service/data/EXR/D.USD.EUR.SP00.A
 *           ?lastNObservations=10&format=jsondata
 * PARSE TRAP: the observation keys are POSITIONAL indices into
 * structure.dimensions.observation[0].values[], and the series key
 * ("0:0:0:0:0") is positional into structure.dimensions.series[] - both are
 * joined by the reader in od_shared.c so the emitted period is the real date
 * rather than an array offset. A null observation is skipped, not zeroed.
 * Emits, per observation: the decoded series dimensions (currency, currency
 * denominator, exchange-rate type, series variation), the period and the
 * numeric rate. Distinct host from the already-covered www.ecb.europa.eu
 * press/blog RSS. Keyless. Any ECB dataflow (EXR, ICP, MIR, BSI) uses the
 * same URL shape.
 * Licence: the ECB permits reuse of its statistics with attribution. */
#include "od_shared.c"

#define SID "ecb-exchange-rates"
static const char *URL =
  "https://data-api.ecb.europa.eu/service/data/EXR/D.USD.EUR.SP00.A"
  "?lastNObservations=10&format=jsondata";

static int run(const source_ctx *ctx, intel_sink *sink) {
  return od_rc(SID, od_sdmx_collect(ctx, sink, URL, "fx-reference-rate",
                                    "[\"statistics\",\"fx\",\"ecb\"]", URL,
                                    500));
}

static const source_def od_ecb_exchange_rates_def = {
  .id = SID, .collector = "statistics",
  .name = "ECB Data Portal SDMX - reference exchange rates",
  .update_interval_sec = 21600, .run = run,
  .category = "statistics", .type = "api",
  .url = "https://data-api.ecb.europa.eu/service/data/EXR/D.USD.EUR.SP00.A?lastNObservations=10&format=jsondata",
  .description = "Daily ECB euro reference exchange rates (USD/EUR) as dated numeric observations from the keyless SDMX-JSON API",
  .license = "ECB statistics - reuse permitted with attribution",
  .free_tier = 1,
};
REGISTER_SOURCE(od_ecb_exchange_rates_def)
